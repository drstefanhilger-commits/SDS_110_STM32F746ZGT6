/*
 * SimScenario.hpp  (Harness, kein Patentmodul)
 *
 * Szenarien des Signal_Simulator und ihre Auswahl über USB-Kommando Id 3 (Simulation):
 *   0       Mikrofone (SAI/DMA), kein Simulator
 *   1       Simulator mit dem Standardszenario SIM_SCENARIO_ID (SDS_110_Config.hpp, bisheriges Verhalten)
 *   2 + k   Simulator mit Szenario k (2 DroneSweep … 7 FlyBy)
 * Reine Logik ohne Hardware (auch für LCD, USB und Host-Tests).
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"

namespace sds110 {

enum class SimScenario : uint8_t { DroneSweep = 0, DroneStatic, SingleTone, WindNoise, Silence, FlyBy };

constexpr uint32_t SIM_NUM_SCENARIOS       = 6;
constexpr uint32_t SIM_CMD_OFF             = 0;   // Mikrofone
constexpr uint32_t SIM_CMD_DEFAULT         = 1;   // Standardszenario
constexpr uint32_t SIM_CMD_FIRST_SCENARIO  = 2;   // 2 + k -> Szenario k
static_assert(SIM_SCENARIO_ID < SIM_NUM_SCENARIOS, "SIM_SCENARIO_ID außerhalb der Szenarien");

/// Wert von USB Id 3 -> Szenario; false bei 0 (Mikrofone) oder unbekanntem Wert
inline bool simScenarioFromCommand(uint32_t v, SimScenario& s)
{
    if (v == SIM_CMD_DEFAULT) { s = static_cast<SimScenario>(SIM_SCENARIO_ID); return true; }
    if (v >= SIM_CMD_FIRST_SCENARIO && v < SIM_CMD_FIRST_SCENARIO + SIM_NUM_SCENARIOS) {
        s = static_cast<SimScenario>(v - SIM_CMD_FIRST_SCENARIO);
        return true;
    }
    return false;
}

/// gültiger Wert für USB Id 3 (Mikrofone oder ein Szenario)
inline bool simCommandValid(uint32_t v) { SimScenario s; return v == SIM_CMD_OFF || simScenarioFromCommand(v, s); }

inline const char* simScenarioName(SimScenario s)
{
    static const char* const names[SIM_NUM_SCENARIOS] = { "DroneSweep", "DroneStatic", "SingleTone", "WindNoise", "Silence", "FlyBy" };
    const uint32_t i = static_cast<uint32_t>(s);
    return (i < SIM_NUM_SCENARIOS) ? names[i] : "?";
}

} // namespace sds110
