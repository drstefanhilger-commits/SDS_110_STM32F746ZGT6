/*
 * t_uart_selftest – Logik des USART1-Selbsttests (UartSelfTestCore.hpp, SDS110_UART_SELFTEST)
 *
 * Prüft:
 *   1. Logger-Nachricht Id 99: Magic, len_id 0x63000090, Text, CRC32 wie zlib (CRC32::computeCRC32)
 *   2. ein Kommando mit richtiger CRC -> "ECHO id=10 len=28 crc=OK c=<crc>"
 *   3. falsche CRC -> "crc=BAD", Zähler bad
 *   4. zwei Kommandos in einem Block und ein Kommando Byte für Byte -> je ein ECHO
 *   5. Müll vor dem Magic -> "ERR n=2 00 11", danach ECHO
 *   6. Herzschlag nennt die Zähler
 * Aufruf: build/test_host/t_uart_selftest
 */
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Infrastructure/Utils/UartSelfTestCore.hpp"
#include "Infrastructure/Utils/crc32.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

using Bytes = std::vector<uint8_t>;

// Kommando PC -> SDS (ICD 4): Magic, Id, Länge BE, Nutzlast, CRC32 BE über alles davor
static Bytes cmd(uint8_t id, uint32_t len, uint32_t value, bool goodCrc = true)
{
    Bytes b(len, 0);
    const uint8_t h[8] = {0xDE, 0xAD, 0xBE, 0xEF, id, uint8_t(len >> 16), uint8_t(len >> 8), uint8_t(len)};
    std::memcpy(b.data(), h, 8);
    for (int i = 0; i < 4; ++i) b[8 + i] = uint8_t(value >> (24 - 8 * i));
    uint32_t c = CRC32::computeCRC32(b.data(), len - 4);
    if (!goodCrc) c ^= 1u;
    for (int i = 0; i < 4; ++i) b[len - 4 + i] = uint8_t(c >> (24 - 8 * i));
    return b;
}

static std::vector<std::string> feed(UartSelfTestCore& c, const Bytes& d, uint64_t us)
{
    std::vector<std::string> out;
    c.onRx(d.data(), static_cast<uint32_t>(d.size()), us, [&](const char* t) { out.emplace_back(t); });
    return out;
}

static uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }

int main()
{
    UartSelfTestCore core;

    // 1) Logger-Nachricht
    {
        uint8_t m[UartSelfTestCore::MSG_LEN];
        core.buildLog(m, "hallo");
        check(le32(m) == 0xDEADBEEFu && le32(m + 4) == 0x63000090u && le32(m + 8) == 0 &&
              std::strcmp(reinterpret_cast<const char*>(m + 12), "hallo") == 0 &&
              le32(m + 140) == CRC32::computeCRC32(m, 140), "Logger Id 99: Rahmen, Text, CRC wie zlib");
        const uint8_t probe[] = "123456789";
        check(core.crc32(probe, 9) == 0xCBF43926u, "Tabellen-CRC32 = Prüfwert CBF43926");
    }
    const Bytes pos  = cmd(10, 28, 0x0001E240);
    const Bytes unit = cmd(5, 16, 42);
    char want[64];
    std::snprintf(want, sizeof(want), "ECHO id=10 len=28 crc=OK c=%08X", CRC32::computeCRC32(pos.data(), 24));

    // 2) ein Kommando
    {
        auto r = feed(core, pos, 1000);
        check(r.size() == 1 && r[0] == want, "Id 10 -> ECHO mit crc=OK und CRC-Wert");
    }
    // 3) falsche CRC
    {
        auto r = feed(core, cmd(5, 16, 42, false), 2000);
        check(r.size() == 1 && r[0].find("ECHO id=5 len=16 crc=BAD") == 0 && core.crcBad() == 1, "falsche CRC -> crc=BAD");
    }
    // 4) zusammengefasst und Byte für Byte
    {
        Bytes two = unit; two.insert(two.end(), pos.begin(), pos.end());
        auto r = feed(core, two, 3000);
        check(r.size() == 2 && r[0].find("ECHO id=5 len=16 crc=OK") == 0 && r[1] == want, "zwei Kommandos in einem Block");
        std::vector<std::string> all;
        for (size_t i = 0; i < pos.size(); ++i) { auto x = feed(core, Bytes{pos[i]}, 4000 + i * 11); all.insert(all.end(), x.begin(), x.end()); }
        check(all.size() == 1 && all[0] == want, "Kommando Byte für Byte (wie die Polling-Schleife)");
    }
    // 5) Müll
    {
        Bytes junk = {0x00, 0x11}; junk.insert(junk.end(), unit.begin(), unit.end());
        auto r = feed(core, junk, 5000);
        check(r.size() == 2 && r[0] == "ERR n=2 00 11" && r[1].find("ECHO id=5") == 0, "Müll -> ERR, danach Resync");
    }
    // 6) Herzschlag
    {
        char t[UartSelfTestCore::TEXT_LEN];
        core.heartbeat(t, sizeof(t), 1234, 3, 4, 5);
        char exp[UartSelfTestCore::TEXT_LEN];
        std::snprintf(exp, sizeof(exp), "UART-SELFTEST t=1234 rx=%u cmd=6 bad=1 err=1 ore=3 fe=4 drop=5", core.rxBytes());
        check(std::string(t) == exp && core.commands() == 6, "Herzschlag mit Zählern");
    }
    return g_fail;
}
