/*
 * TimeBase.hpp  (Infrastructure/Utils)
 *
 * Monotone Zeitbasis in µs seit dem Start, aus DWT->CYCCNT (SYSCLK, 216 MHz -> 4,6 ns).
 * CYCCNT ist 32 bit und läuft alle 2^32 / 216 MHz = 19,9 s über. CycleExtender führt
 * einen 64-bit-Zähler; Überläufe, die zwischen zwei Aufrufen verpasst wurden (Pause
 * > 19,9 s, z. B. im Simulationsbetrieb), werden über den 1-ms-HAL-Tick ermittelt.
 *
 * Achtung: Die Zeit ist Laufzeit seit dem Start, keine UTC. Den UTC-Bezug liefert UtcClock
 * (Versatz aus USB-Kommando Typ 7, ~1 ms genau; angewendet erst im UnitReport). Für die
 * Inter-Unit-Synchronisation (FSL9: 10 µs) ist GNSS-PPS in HW-Version 2 vorgesehen.
 */
#pragma once
#include <cstdint>

namespace sds110 {

/// Reine Logik (ohne Hardware, auf dem Host testbar)
class CycleExtender {
public:
    explicit CycleExtender(uint32_t cyclesPerMs) : cyclesPerMs_(cyclesPerMs) {}

    /// cyc: aktueller 32-bit-Zyklenzähler, tickMs: ms-Tick zum selben Zeitpunkt.
    /// Rückgabe: Zyklen seit Start (64 bit, monoton).
    uint64_t update(uint32_t cyc, uint32_t tickMs)
    {
        if (!init_) {
            total_ = static_cast<uint64_t>(tickMs) * cyclesPerMs_;   // Start: Laufzeit laut Tick
            lastCyc_ = cyc; lastTick_ = tickMs; init_ = true;
            return total_;
        }
        uint64_t d = static_cast<uint32_t>(cyc - lastCyc_);           // modulo 2^32
        const uint64_t expected = static_cast<uint64_t>(static_cast<uint32_t>(tickMs - lastTick_)) * cyclesPerMs_;
        if (expected > d + (1ull << 31))                              // verpasste Überläufe (gerundet)
            d += ((expected - d + (1ull << 31)) >> 32) << 32;
        total_ += d;
        lastCyc_ = cyc; lastTick_ = tickMs;
        return total_;
    }

private:
    uint32_t cyclesPerMs_;
    uint64_t total_ = 0;
    uint32_t lastCyc_ = 0, lastTick_ = 0;
    bool     init_ = false;
};

class TimeBase {
public:
    /// µs seit Start; ISR- und Task-sicher (kurzer kritischer Abschnitt)
    static uint64_t nowUs();
};

} // namespace sds110
