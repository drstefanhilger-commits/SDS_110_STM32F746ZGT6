/*
 * UtcClock.hpp  (Infrastructure/Utils)
 *
 * UTC-Bezug der internen µs-Zeit (TimeBase::nowUs, Laufzeit seit Start).
 * Alle Zeitstempel im Gerät bleiben Laufzeit; erst der UnitReport (130) rechnet sie mit dem
 * Versatz in UTC um und sendet die Quelle mit:
 *   Uptime  – kein Abgleich, Zeit = Laufzeit seit Start (µs)
 *   PcUtc   – UTC vom PC-Monitor (USB-Kommando Typ 7); genau auf die USB-Laufzeit (~1 ms)
 *   GnssPps – vorgesehen für HW-Version 2 (GPS-PPS stellt den Zähler, < 1 µs, FSL9 A6)
 * Reine Logik ohne Hardware (Host-Test t_utc_clock).
 */
#pragma once
#include <cstdint>

namespace sds110 {

enum class TimeSource : uint8_t { Uptime = 0, PcUtc = 1, GnssPps = 2 };

struct UtcOffset {
    int64_t    offsetUs = 0;                 // UTC = Laufzeit + offsetUs
    TimeSource source   = TimeSource::Uptime;
};

class UtcClock {
public:
    /// Untergrenze für plausible UTC-Werte: 01.01.2020 00:00:00 UTC (µs seit 1970)
    static constexpr uint64_t MIN_UTC_US = 1577836800ULL * 1000000ULL;

    /// Abgleich: utcUs = UTC laut Sender, rxUptimeUs = Laufzeit beim Empfang.
    /// false (out unverändert) bei unplausibler Zeit.
    static bool fromSync(uint64_t utcUs, uint64_t rxUptimeUs, TimeSource src, UtcOffset& out)
    {
        if (utcUs < MIN_UTC_US || utcUs < rxUptimeUs) return false;
        out.offsetUs = static_cast<int64_t>(utcUs - rxUptimeUs);
        out.source   = src;
        return true;
    }

    /// Laufzeit (µs) -> Zeitstempel für den Report (UTC, falls abgeglichen)
    static uint64_t toReport(uint64_t uptimeUs, const UtcOffset& o)
    {
        return (o.source == TimeSource::Uptime) ? uptimeUs
                                                : uptimeUs + static_cast<uint64_t>(o.offsetUs);
    }
};

} // namespace sds110
