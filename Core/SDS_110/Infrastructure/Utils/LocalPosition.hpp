/*
 * LocalPosition.hpp  (Infrastructure/Utils)
 *
 * Standort der Sensoreinheit in lokalen Koordinaten (doc/ICD_SDS_PC_Monitor.md 4.5, 5.5):
 *   x = Ost, y = Nord, z = Oben, in m relativ zum lokalen Ursprung [0, 0, 0]
 *   (Achsen wie der Lageplan im PC-Monitor und der Azimut: 0° = Nord, 90° = Ost, Azimuth.hpp).
 * FSL9 §1 (A7): Positionen der Einheiten vermessen und gespeichert – Grundlage der Lokalisation
 * mit mehreren Einheiten. Beim Start steht die Einheit im Ursprung; der PC-Monitor setzt die
 * Position mit Kommando Id 10. Der Simulator (FlyBy) legt seine Flugbahn um den Ursprung und
 * rechnet Azimut und Distanz von dieser Position aus.
 *
 * Kommando Id 10 (28 Byte, big-endian):
 *   [0..7]   DE AD BE EF 0A 00 00 1C
 *   [8..11]  Ost   i32, mm, |x| ≤ 100 km
 *   [12..15] Nord  i32, mm, |y| ≤ 100 km
 *   [16..19] Oben  i32, mm, −1000 … +10000 m
 *   [20]     Flags Bit 0 = Position setzen (0 = zurück auf den Ursprung [0, 0, 0])
 *   [21..23] reserviert
 *   [24..27] CRC32 BE (noch nicht geprüft, Befund 12)
 * Nachricht Id 6 (SDS -> PC, 144 Byte, Nutzlast little-endian): siehe encodeReport().
 *
 * Bis 29.09.2026 WGS84 (Breite, Länge, Höhe) mit GNSS-Vorrang; ersetzt durch lokale Koordinaten.
 * Reine Logik ohne Hardware (Host-Test t_local_position).
 */
#pragma once
#include <cstdint>
#include <cstring>

namespace sds110 {

struct LocalPosition {
    int32_t eastMm  = 0;             // x
    int32_t northMm = 0;             // y
    int32_t upMm    = 0;             // z
    bool    set     = false;         // vom PC gesetzt (false: Grundwert Ursprung)

    float eastM()  const { return static_cast<float>(eastMm)  * 0.001f; }
    float northM() const { return static_cast<float>(northMm) * 0.001f; }
    float upM()    const { return static_cast<float>(upMm)    * 0.001f; }
};

class LocalPositionCodec {
public:
    static constexpr uint32_t CMD_LENGTH = 28;
    static constexpr uint8_t  FLAG_SET = 0x01;
    static constexpr int32_t  HORIZ_LIMIT_MM = 100000000;               // 100 km
    static constexpr int32_t  UP_MIN_MM = -1000000, UP_MAX_MM = 10000000;
    static constexpr uint32_t REPORT_PAYLOAD = 16;                      // genutzte Bytes der 128-Byte-Nutzlast

    /// Kommando Id 10 (ab Magic) -> Position; false bei falscher Länge oder Werten außerhalb
    static bool decode(const uint8_t* rx, uint32_t len, LocalPosition& out)
    {
        if (len != CMD_LENGTH) return false;
        LocalPosition p;
        p.eastMm  = be32(rx + 8);
        p.northMm = be32(rx + 12);
        p.upMm    = be32(rx + 16);
        p.set     = (rx[20] & FLAG_SET) != 0;
        if (p.set && (p.eastMm < -HORIZ_LIMIT_MM || p.eastMm > HORIZ_LIMIT_MM ||
                      p.northMm < -HORIZ_LIMIT_MM || p.northMm > HORIZ_LIMIT_MM ||
                      p.upMm < UP_MIN_MM || p.upMm > UP_MAX_MM)) return false;
        if (!p.set) p = LocalPosition{};                                  // zurück auf den Ursprung
        out = p;
        return true;
    }

    /// Nutzlast der Nachricht Id 6 (little-endian):
    ///   [0..1] Unit-ID u16  [2] reserviert (0)  [3] Flags Bit 0 = vom PC gesetzt
    ///   [4..7] Ost i32 mm  [8..11] Nord i32 mm  [12..15] Oben i32 mm
    static void encodeReport(const LocalPosition& p, uint16_t unit, uint8_t* out)
    {
        std::memset(out, 0, REPORT_PAYLOAD);
        std::memcpy(out, &unit, 2);
        out[3] = p.set ? FLAG_SET : 0;
        const int32_t v[3] = { p.eastMm, p.northMm, p.upMm };
        std::memcpy(out + 4, v, sizeof(v));
    }

private:
    static int32_t be32(const uint8_t* b)
    {
        return static_cast<int32_t>((static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
                                    (static_cast<uint32_t>(b[2]) << 8) | b[3]);
    }
};

} // namespace sds110
