/*
 * FeedbackCodec.hpp  (Infrastructure/Utils)
 *
 * Feedback der Tracking-Einheit an das Board (FSL9 §10, Anspruch 8), USB-Kommando Id 8,
 * 52 Byte (doc/ICD_SDS_PC_Monitor.md 4.3):
 *   [0..3]   DE AD BE EF
 *   [4]      08
 *   [5..7]   Länge 00 00 34 (52)
 *   [8..39]  ŝ: 64 Bänder × 4 Bit, Band 2i im oberen, 2i+1 im unteren Halbbyte; ŝ_b = q / 15
 *   [40..41] vorhergesagter Azimut, u16 BE, 0,01° (0° = Nord, im Uhrzeigersinn)
 *   [42..43] vorhergesagte Distanz, u16 BE, 0,1 m
 *   [44]     Flags: Bit 0 = ŝ gültig (0 = Feedback zurücksetzen), Bit 1 = Position gültig
 *   [45..47] reserviert (0)
 *   [48..51] CRC32 BE über Byte 0–47 (noch nicht geprüft, Befund 12)
 * 4 Bit reichen: 126 nutzt ŝ nur für ŝ_b > θ_ref = 0,6 und Gewicht × (1 + ŝ_b).
 * Reine Logik ohne Hardware (Host-Test t_feedback).
 */
#pragma once
#include <cstdint>
#include "Data_Interface_140/Candidate_Report_140.hpp"

namespace sds110 {

class FeedbackCodec {
public:
    static constexpr uint32_t LENGTH = 52;
    static constexpr uint8_t  FLAG_STATE = 0x01, FLAG_POSITION = 0x02;

    /// Kommando (ab Magic) -> TrackingFeedback; false bei falscher Länge oder Werten außerhalb
    static bool decode(const uint8_t* rx, uint32_t len, TrackingFeedback& fb, bool& positionValid)
    {
        if (len != LENGTH) return false;
        TrackingFeedback f{};
        for (uint32_t i = 0; i < NUM_BANDS / 2; ++i) {
            const uint8_t v = rx[8 + i];
            f.ref_state[2 * i]     = static_cast<float>(v >> 4) / 15.0f;
            f.ref_state[2 * i + 1] = static_cast<float>(v & 0x0F) / 15.0f;
        }
        const uint16_t az = static_cast<uint16_t>((rx[40] << 8) | rx[41]);
        const uint16_t r  = static_cast<uint16_t>((rx[42] << 8) | rx[43]);
        if (az >= 36000) return false;
        f.pred_azimuth_deg = static_cast<float>(az) * 0.01f;
        f.pred_distance_m  = static_cast<float>(r) * 0.1f;
        f.valid            = (rx[44] & FLAG_STATE) != 0;
        positionValid      = (rx[44] & FLAG_POSITION) != 0;
        fb = f;
        return true;
    }
};

} // namespace sds110
