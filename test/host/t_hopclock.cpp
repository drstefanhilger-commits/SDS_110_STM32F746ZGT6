/*
 * t_hopclock – Takt der Simulation im ProcessingTask
 *
 * Bezug: Befund 28 (Task-Takt 40 ms passte nicht zu 32-ms-Hops)
 * Prüft (HopClock, Infrastructure/Utils/HopClock.hpp):
 *   1. Hop-Rate: Task wacht zu nextTick() auf, Verarbeitung 0–30 ms -> nach 60 s genau
 *      die fälligen Hops (31,25 /s), keine übersprungen
 *   2. Nachholen: 100 ms verspätet -> 4 Hops auf einmal; 1 s verspätet -> 4 nachgeholt, Rest gezählt
 *   3. Tick-Überlauf (uint32) und nicht ganzzahlige Periode (1024 Hz) über 10 min
 * Aufruf: build/test_host/t_hopclock
 */
#include <cstdio>
#include <cstdint>
#include <random>
#include "Infrastructure/Utils/HopClock.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

// Task-Schleife nachbilden: Hops erzeugen, rechnen, bis nextTick() schlafen
static uint64_t runTask(HopClock& c, uint32_t t0, uint32_t hz, uint32_t durTicks, uint32_t maxWork)
{
    std::mt19937 rng(1);
    uint32_t now = t0; uint64_t hops = 0;
    c.start(now, hz);
    while (now - t0 < durTicks) {
        hops += c.due(now, 4);
        now += rng() % (maxWork + 1);                          // Rechenzeit
        const uint32_t next = c.nextTick();
        if (static_cast<int32_t>(next - now) > 0) now = next;  // osDelayUntil
    }
    return hops;
}

int main()
{
    HopClock c;
    // 1) 1 kHz, 60 s, Rechenzeit bis 30 ms (< 32 ms Periode)
    uint64_t hops = runTask(c, 0, 1000, 60000, 30);
    std::printf("1 kHz, 60 s: %llu Hops (erwartet 1875), übersprungen %u\n", (unsigned long long)hops, c.skipped());
    check(hops == 1875 || hops == 1876, "Hop-Rate 31,25 /s");
    check(c.skipped() == 0, "keine übersprungenen Hops");

    // 2) Nachholen
    c.start(0, 1000);
    uint32_t n0 = c.due(0, 4), n1 = c.due(32, 4), n2 = c.due(32 + 100, 4);
    std::printf("Nachholen: %u %u %u (erwartet 1 1 3)\n", n0, n1, n2);
    check(n0 == 1 && n1 == 1 && n2 == 3, "fällige Hops nach 100 ms Verspätung");
    const uint32_t n3 = c.due(132 + 1000, 4);
    std::printf("1 s verspätet: %u erzeugt, %u übersprungen\n", n3, c.skipped());
    check(n3 == 4 && c.skipped() == 27, "höchstens 4 nachholen, Rest zählen");
    check(c.nextTick() > 132 + 1000 - 32 && c.nextTick() <= 132 + 1000 + 32, "nächster Hop nach dem Überspringen innerhalb einer Periode");

    // 3) Überlauf und 1024 Hz über 10 min
    hops = runTask(c, 0xFFFF0000u, 1000, 600000, 20);
    check(hops >= 18750 && hops <= 18751 && c.skipped() == 0, "Tick-Überlauf: 18750 Hops in 10 min");
    hops = runTask(c, 123, 1024, 600 * 1024, 20);
    std::printf("1024 Hz, 10 min: %llu Hops\n", (unsigned long long)hops);
    check(hops >= 18750 && hops <= 18751 && c.skipped() == 0, "nicht ganzzahlige Periode (32,768 Ticks)");
    return g_fail;
}
