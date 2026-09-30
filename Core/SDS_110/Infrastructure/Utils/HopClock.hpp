/*
 * HopClock.hpp  (Infrastructure/Utils)
 *
 * Takt der Simulation im ProcessingTask (Befund 28): Hop k ist fällig zum Tick
 * t0 + k · HOP_SAMPLES · tickHz / SAMPLE_RATE_HZ (32 ms bei 1 kHz). due() liefert, wie viele
 * Hops seit dem letzten Aufruf fällig geworden sind – so bleibt die Simulation im Mittel
 * exakt bei SAMPLE_RATE_HZ / HOP_SAMPLES Hops/s (31,25), auch wenn ein Durchlauf länger dauert.
 * Ist die Verarbeitung dauerhaft zu langsam, werden höchstens maxCatchUp Hops nachgeholt,
 * der Rest übersprungen und gezählt (skipped()); die Simulation läuft dann langsamer statt
 * sich aufzustauen.
 *
 * Portabel (keine RTOS-Aufrufe), Ticks als uint32_t mit Überlauf; Host-Test t_hopclock.
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"

namespace sds110 {

class HopClock {
public:
    void start(uint32_t nowTick, uint32_t tickHz)
    {
        // Hop-Periode als gekürzter Bruch p/q Ticks: nach q Hops genau p Ticks -> t0 nachführen,
        // damit die 32-bit-Tickdifferenz nie überläuft
        uint64_t p = static_cast<uint64_t>(HOP_SAMPLES) * tickHz, q = SAMPLE_RATE_HZ;
        const uint64_t g = gcd(p, q);
        p_ = static_cast<uint32_t>(p / g); q_ = static_cast<uint32_t>(q / g);
        t0_ = nowTick; count_ = 0; skipped_ = 0;
    }

    /// Anzahl jetzt zu erzeugender Hops (0 … maxCatchUp); zählt sie als erzeugt
    uint32_t due(uint32_t nowTick, uint32_t maxCatchUp)
    {
        const uint32_t elapsed = nowTick - t0_;
        const uint64_t avail = static_cast<uint64_t>(elapsed) * q_ / p_ + 1;   // Hop 0 sofort
        uint32_t n = (avail > count_) ? static_cast<uint32_t>(avail - count_) : 0;
        if (n > maxCatchUp) { skipped_ += n - maxCatchUp; count_ += n - maxCatchUp; n = maxCatchUp; }
        count_ += n;
        while (count_ >= q_) { count_ -= q_; t0_ += p_; }
        return n;
    }

    /// Tick, zu dem der nächste Hop fällig wird (aufgerundet)
    uint32_t nextTick() const
    {
        return t0_ + static_cast<uint32_t>((static_cast<uint64_t>(count_) * p_ + q_ - 1) / q_);
    }

    uint32_t skipped() const { return skipped_; }

private:
    static uint64_t gcd(uint64_t a, uint64_t b) { while (b) { const uint64_t t = a % b; a = b; b = t; } return a; }

    uint32_t t0_ = 0, p_ = 1, q_ = 1;
    uint32_t count_ = 0;          // erzeugte Hops seit t0_ (< q_ nach due())
    uint32_t skipped_ = 0;
};

} // namespace sds110
