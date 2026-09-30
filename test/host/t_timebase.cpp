/*
 * t_timebase – 64-bit-Erweiterung des DWT-Zyklenzählers
 *
 * Bezug: Befund 16
 * Prüft/misst: CycleExtender monoton und exakt bei DMA-Takt, zufälligen Abständen, Pausen > 19,9 s (verpasste Überläufe), Tick-Jitter
 * Aufruf: make check  bzw.  build/test_host/t_timebase
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "Infrastructure/Utils/TimeBase.hpp"
using namespace sds110;
int main()
{
    const uint64_t CPM = 216000;                 // Zyklen je ms bei 216 MHz
    int fail = 0; std::srand(7);
    struct Scenario { const char* name; double stepMin_ms, stepMax_ms; int steps; };
    const Scenario sc[] = {
        { "DMA-Takt 2,67 ms, 10 min",                 2.667, 2.667, 225000 },
        { "zufällige Abstände 0..15 s",               0.0, 15000.0, 20000 },
        { "Pausen 18..60 s (verpasste Überläufe)",    18000.0, 60000.0, 5000 },
        { "Pausen bis 1 h",                            0.0, 3600000.0, 2000 },
    };
    for (const auto& s : sc) {
        CycleExtender ext(CPM);
        uint64_t trueCyc = 12345ull * CPM + 777;   // Start nach 12,345 s
        uint64_t last = 0; double maxErrUs = 0; bool mono = true;
        for (int i = 0; i < s.steps; ++i) {
            const double step = s.stepMin_ms + (s.stepMax_ms - s.stepMin_ms) * (std::rand() / (double)RAND_MAX);
            trueCyc += (uint64_t)(step * CPM);
            const uint32_t cyc = (uint32_t)trueCyc;
            // Tick ist ms-gerundet und darf bis 1 ms hinterherhinken (ISR-Priorität)
            const uint32_t tick = (uint32_t)(trueCyc / CPM) - (std::rand() % 2);
            const uint64_t got = ext.update(cyc, tick);
            if (i == 0) { last = got; continue; }
            if (got < last) mono = false;
            last = got;
            // Abstand zum wahren Wert relativ zum ersten Aufruf
            static uint64_t off; if (i == 1) off = trueCyc - got;
            const double err = std::fabs((double)(int64_t)(trueCyc - got - off)) / 216.0;
            if (err > maxErrUs) maxErrUs = err;
        }
        const bool ok = mono && maxErrUs < 1.0;
        std::printf("%-42s monoton %d, max. Fehler %8.3f µs  %s\n", s.name, mono, maxErrUs, ok ? "OK" : "FEHLER");
        fail += !ok;
    }
    return fail;
}
