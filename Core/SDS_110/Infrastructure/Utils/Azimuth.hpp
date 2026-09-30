/*
 * Azimuth.hpp  (Infrastructure/Utils)
 *
 * Azimut-Konvention (FSL9 §6, FIG. 5; festgelegt 28.09.2026):
 *   0° = Nord, gezählt im Uhrzeigersinn (90° = Ost), Bereich [0, 360).
 *   Mikrofon 0 zeigt nach Nord.
 *
 * Array-Koordinaten (Microphone_Array_114, unverändert): x-Achse = Richtung Mikrofon 0, die
 * Mikrofone m liegen bei m · 45° gegen den Uhrzeigersinn (von oben gesehen) ab x. Mit Mikrofon 0
 * = Nord ist x = Nord und y = West (MIC_NUMBERING_CLOCKWISE = false). Sind die Mikrofone auf der
 * Platine im Uhrzeigersinn nummeriert, ist y = Ost: dann MIC_NUMBERING_CLOCKWISE = true setzen.
 * Die Unit-Positionen in 128 verwenden dieselben Achsen.
 *
 * Reine Logik ohne Hardware; hier statt in SDS_110_Config.hpp, weil die Config in die
 * Merkmalsversion eingeht.
 */
#pragma once
#include <cmath>
#include <cstdint>

namespace sds110 {

class Azimuth {
public:
    static constexpr bool MIC_NUMBERING_CLOCKWISE = false;

    static float wrap360(float deg)
    {
        deg = std::fmod(deg, 360.0f);
        if (deg < 0.0f) deg += 360.0f;
        return (deg >= 360.0f) ? 0.0f : deg;
    }

    /// kleinster Winkelabstand zweier Azimute (0 … 180°)
    static float diff(float a, float b)
    {
        const float d = std::fabs(wrap360(a) - wrap360(b));
        return (d > 180.0f) ? 360.0f - d : d;
    }

    /// Winkel im Array-Koordinatensystem (gegen den Uhrzeigersinn ab x, Grad) -> Azimut
    static float fromArrayAngle(float phiDeg) { return wrap360(MIC_NUMBERING_CLOCKWISE ? phiDeg : -phiDeg); }

    /// Richtungsvektor (ux, uy) im Array-Koordinatensystem -> Azimut
    static float fromArray(float ux, float uy)
    {
        return fromArrayAngle(std::atan2(uy, ux) * (180.0f / 3.14159265f));
    }

    /// Nordabgleich (USB-Kommando Id 9, doc/ICD_SDS_PC_Monitor.md 4.4): Offset als i32 in 0,01°,
    /// zulässig −180,00° … +180,00°; false außerhalb (Offset bleibt dann unverändert)
    static constexpr int32_t OFFSET_LIMIT_CENTI = 18000;
    static bool offsetFromCenti(int32_t centi, float& deg)
    {
        if (centi < -OFFSET_LIMIT_CENTI || centi > OFFSET_LIMIT_CENTI) return false;
        deg = static_cast<float>(centi) * 0.01f;
        return true;
    }

    /// Azimut -> Einheitsvektor (ux, uy) im Array-Koordinatensystem
    static void toArray(float azDeg, float& ux, float& uy)
    {
        const float phi = (MIC_NUMBERING_CLOCKWISE ? azDeg : -azDeg) * (3.14159265f / 180.0f);
        ux = std::cos(phi);
        uy = std::sin(phi);
    }
};

} // namespace sds110
