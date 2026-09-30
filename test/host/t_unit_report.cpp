/*
 * t_unit_report – Serialisierung des UnitReport (Nachricht id 5)
 *
 * Bezug: Befunde 10, 33
 * Prüft:
 *   1. Feld nsel = tatsächlich gesendete Bänder (max. 51), Bandliste innerhalb der 128-Byte-Nutzlast
 *   2. Zeitstempel u64 in µs und Zeitquelle: ohne Abgleich Laufzeit (Quelle 0), nach dem
 *      UTC-Abgleich (UtcClock, USB-Kommando Typ 7) UTC = Laufzeit + Versatz (Quelle 1), µs-genau
 *   3. UtcClock::fromSync verwirft unplausible Zeiten (vor 2020, kleiner als die Laufzeit)
 * Aufruf: make check  bzw.  build/test_host/t_unit_report
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cstring>
#include "Processing_Module_120/Output_Interface_130/Output_Interface_130.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"
#include "Infrastructure/Model/SDS_Structs.hpp"
using namespace sds110;
namespace sds110 { extern ::MessageData g_lastMsg; extern uint32_t g_lastId; }

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

int main()
{
    Output_Interface_130 out; out.init();
    constexpr uint32_t H = Output_Interface_130::UNIT_REPORT_HEAD, MAXB = (sizeof(::MessageData) - H) / 2;

    // 1) Bandliste
    for (uint32_t nsel : { 0u, 3u, 8u, 51u, 52u, 64u }) {
        UnitReport r{}; r.unit_id = 0x1234; r.bearing_deg = 42.5f; r.valid_pairs = 27; r.level = 0.1f; r.num_selected = nsel;
        for (uint32_t i = 0; i < nsel; ++i) { r.band_index[i] = i; r.band_prob[i] = (i + 1) / 64.0f; }
        std::memset(&g_lastMsg, 0xEE, sizeof(g_lastMsg));
        out.send(r);
        const uint8_t* b = g_lastMsg.b; const uint32_t n = b[20];          // Kopf: 2+8+1+4+4+1 -> nsel an Offset 20
        const uint32_t expect = nsel > MAXB ? MAXB : nsel;
        bool ok = g_lastId == Output_Interface_130::UNIT_REPORT_ID && n == expect && H + 2 * n <= sizeof(::MessageData);
        for (uint32_t i = 0; i < n && ok; ++i) ok = b[H + i] == i && b[H + n + i] == (uint8_t)(((i + 1) / 64.0f) * 255.0f);
        float bearing; std::memcpy(&bearing, b + 11, 4);
        ok = ok && bearing == 42.5f && b[19] == 27;
        std::printf("num_selected %2u -> nsel %2u, Bytes %3u/128 %s\n", nsel, n, H + 2 * n, ok ? "OK" : "FEHLER");
        g_fail |= !ok;
    }

    // 2) Zeitstempel
    auto sendAt = [&](uint64_t uptime, uint64_t& t, uint8_t& src) {
        CandidateLocation loc{}; AcousticState s{}; ComponentSelection sel{}; UnitReport r{};
        out.buildReport(loc, s, sel, 0.0f, uptime, r);
        out.send(r);
        std::memcpy(&t, g_lastMsg.b + 2, 8); src = g_lastMsg.b[10];
    };
    auto& dm = SDS_Data::instance();
    uint64_t t; uint8_t src;
    dm.setUtcOffset(UtcOffset{});
    sendAt(123456789ULL, t, src);
    std::printf("ohne Abgleich: t = %llu µs, Quelle %u\n", (unsigned long long)t, src);
    check(t == 123456789ULL && src == 0, "ohne Abgleich: Laufzeit in µs, Quelle 0");

    const uint64_t utcSync = 1790000000123456ULL;                   // 2026-09-21, µs
    const uint64_t rxUptime = 5000000ULL;                           // Empfang nach 5 s Laufzeit
    UtcOffset o;
    check(UtcClock::fromSync(utcSync, rxUptime, TimeSource::PcUtc, o), "fromSync: gültige UTC angenommen");
    dm.setUtcOffset(o);
    sendAt(rxUptime + 32001ULL, t, src);                            // Frame 32,001 ms nach dem Abgleich
    std::printf("nach Abgleich: t = %llu µs, Quelle %u\n", (unsigned long long)t, src);
    check(t == utcSync + 32001ULL && src == 1, "nach Abgleich: UTC in µs, Quelle 1");

    // 3) Plausibilität
    UtcOffset keep = o;
    check(!UtcClock::fromSync(1500000000ULL * 1000000ULL, rxUptime, TimeSource::PcUtc, keep) &&
          keep.offsetUs == o.offsetUs, "fromSync: Zeit vor 2020 verworfen, Versatz unverändert");
    check(!UtcClock::fromSync(0, rxUptime, TimeSource::PcUtc, keep), "fromSync: 0 verworfen");
    return g_fail;
}
