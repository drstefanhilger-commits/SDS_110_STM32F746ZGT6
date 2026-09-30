/*
 * t_usb_commands – Kommandos PC -> SDS aus dem USB-Bytestrom (Befund 32, CommandAssembler.hpp)
 *
 * Prüft:
 *   1. ein Kommando je Paket (bisheriger Fall)
 *   2. mehrere Kommandos in einem Paket: Feedback (52) + Unit-ID (16) wie von Windows
 *      zusammengefasst, auf 64 + 4 Byte verteilt; Unit-ID + SRP in einem Paket
 *   3. ein Kommando auf zwei Pakete verteilt (Sync 24 Byte: 10 + 14)
 *   4. Müll vor dem Magic, scheinbares Magic mit falscher Länge -> Fehler, danach Resync
 *   5. veralteter Rest (> 20 ms) wird verworfen und nicht mit dem nächsten Kommando verbunden
 * Aufruf: build/test_host/t_usb_commands
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include "Infrastructure/Utils/CommandAssembler.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

using Bytes = std::vector<uint8_t>;

static Bytes cmd(uint8_t id, uint32_t len, uint32_t value)
{
    Bytes b(len, 0);
    const uint8_t h[8] = {0xDE, 0xAD, 0xBE, 0xEF, id, uint8_t(len >> 16), uint8_t(len >> 8), uint8_t(len)};
    std::memcpy(b.data(), h, 8);
    for (int i = 0; i < 4; ++i) b[8 + i] = uint8_t(value >> (24 - 8 * i));
    return b;
}

struct Out { std::vector<Bytes> cmds; int errors = 0; };

static Out feed(CommandAssembler& a, const Bytes& pkt, uint64_t t)
{
    Out o;
    a.push(pkt.data(), static_cast<uint32_t>(pkt.size()), t);
    const uint8_t* p; uint32_t n;
    for (;;) {
        const auto r = a.next(p, n);
        if (r == CommandAssembler::Result::None) break;
        if (r == CommandAssembler::Result::Error) { ++o.errors; continue; }
        o.cmds.emplace_back(p, p + n);
    }
    return o;
}

static Bytes cat(const Bytes& a, const Bytes& b) { Bytes r = a; r.insert(r.end(), b.begin(), b.end()); return r; }

int main()
{
    const Bytes unit = cmd(5, 16, 42), srp = cmd(6, 16, 1), fb = cmd(8, 52, 0x11223344), sync = cmd(7, 24, 0x0006);

    // 1) ein Kommando je Paket
    {
        CommandAssembler a;
        Out o = feed(a, unit, 1000);
        check(o.cmds.size() == 1 && o.cmds[0] == unit && o.errors == 0 && a.pending() == 0, "ein Kommando je Paket");
    }
    // 2) zusammengefasst
    {
        CommandAssembler a;
        const Bytes s = cat(fb, unit);                              // 68 Byte
        Out o1 = feed(a, Bytes(s.begin(), s.begin() + 64), 1000);
        Out o2 = feed(a, Bytes(s.begin() + 64, s.end()), 1100);
        check(o1.cmds.size() == 1 && o1.cmds[0] == fb && o2.cmds.size() == 1 && o2.cmds[0] == unit &&
              o1.errors + o2.errors == 0, "Feedback + Unit-ID über 64 + 4 Byte");
        Out o3 = feed(a, cat(unit, srp), 2000);
        check(o3.cmds.size() == 2 && o3.cmds[0] == unit && o3.cmds[1] == srp && o3.errors == 0,
              "Unit-ID + SRP in einem Paket");
    }
    // 3) geteilt
    {
        CommandAssembler a;
        Out o1 = feed(a, Bytes(sync.begin(), sync.begin() + 10), 1000);
        Out o2 = feed(a, Bytes(sync.begin() + 10, sync.end()), 1500);
        check(o1.cmds.empty() && o2.cmds.size() == 1 && o2.cmds[0] == sync && o1.errors + o2.errors == 0,
              "Sync auf 10 + 14 Byte verteilt");
    }
    // 4) Müll und scheinbares Magic
    {
        CommandAssembler a;
        Bytes junk = {0x00, 0x11, 0xDE, 0xAD, 0xBE, 0xEF, 0x05, 0x00, 0x00, 0x03};   // Länge 3: ungültig
        Out o = feed(a, cat(junk, unit), 1000);
        check(o.cmds.size() == 1 && o.cmds[0] == unit && o.errors >= 2, "Müll + falsche Länge -> Fehler, danach Resync");
        Out o2 = feed(a, Bytes{0x12, 0x34, 0xDE, 0xAD}, 2000);             // Magic-Anfang am Ende bleibt
        Out o3 = feed(a, Bytes(unit.begin() + 2, unit.end()), 2500);
        check(o2.cmds.empty() && o3.cmds.size() == 1 && o3.cmds[0] == unit, "Magic über die Paketgrenze");
    }
    // 5) veralteter Rest
    {
        CommandAssembler a;
        Out o1 = feed(a, Bytes(unit.begin(), unit.begin() + 10), 1000);
        Out o2 = feed(a, srp, 1000 + CommandAssembler::PARTIAL_TIMEOUT_US + 1);
        check(o1.cmds.empty() && o2.cmds.size() == 1 && o2.cmds[0] == srp && o2.errors == 1 &&
              a.staleDropped() == 10, "Rest nach > 20 ms verworfen, nächstes Kommando unverfälscht");
    }
    return g_fail;
}
