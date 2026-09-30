/*
 * SoundSpeed.hpp  (Infrastructure/Utils)
 *
 * Temperaturkorrigierte Schallgeschwindigkeit (FSL9 §5, A23). Die Lufttemperatur sendet der
 * PC-Monitor im Sync-Kommando (USB Typ 7, doc/ICD_SDS_PC_Monitor.md); 120 übergibt c an 126
 * (Paarverzögerungen, Lag-Fenster, Peilung) und 128, der ProcessingTask an den Simulator.
 * Ohne Temperatur gilt SPEED_OF_SOUND aus SDS_110_Config.hpp (343 m/s ≈ 20 °C).
 *
 * c(T) = 331,3 m/s · sqrt(1 + T / 273,15 °C)   (trockene Luft; Feuchte < 0,5 % Einfluss)
 * Reine Logik ohne Hardware (Host-Test t_sound_speed).
 */
#pragma once
#include <cmath>
#include <cstdint>

namespace sds110 {

class SoundSpeed {
public:
    static constexpr float   T_MIN_C = -40.0f, T_MAX_C = 60.0f;   // angenommener Bereich
    static constexpr int16_t TEMP_UNKNOWN = INT16_MIN;           // Wire-Wert "keine Temperatur"

    static float fromTemperature(float tC) { return 331.3f * std::sqrt(1.0f + tC / 273.15f); }

    /// Wire-Wert (0,01 °C) -> Temperatur; false bei "unbekannt" oder außerhalb T_MIN_C…T_MAX_C
    static bool decode(int16_t centiC, float& tC)
    {
        if (centiC == TEMP_UNKNOWN) return false;
        const float t = static_cast<float>(centiC) * 0.01f;
        if (t < T_MIN_C || t > T_MAX_C) return false;
        tC = t;
        return true;
    }
};

} // namespace sds110
