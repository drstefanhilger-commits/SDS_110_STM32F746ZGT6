/*
 * SDS_Params.hpp  (Infrastructure)
 *
 * Nicht patentrelevante Parameter: UI, Simulation, Versionierung.
 * Alle Patent-/DSP-Parameter (Mikrofongeometrie, Sample-Rate, FFT, Bänder,
 * Schwellen, Bandpass) liegen in SDS_110_Config.hpp.
 *
 * Migration aus SDS/Model/SDS_Params.hpp – entfernt:
 *   SDS_NUM_MICS, SDS_MIC_RADIUS, SDS_MIC_POSITIONS  -> Config / Microphone_Array_114
 *   SDS_SAMPLE_RATE, SDS_FFT_LEN, SDS_FRAME_LEN, SDS_BLOCK_SIZE -> Config
 *   SDS_AZ_*, SDS_NUM_AZIMUTH_BINS, SDS_DIST_GAIN   -> SRP-spezifisch, entfällt (126/128 arbeiten mit TDOA)
 *   SDS_BANDPASS_LOW/HIGH                          -> Config (BANDPASS_LO/HI_HZ)
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"

// Build-Schalter (aus SDS übernommen)
#define SDS_STM32_ACTIVE
#define SDS_TEST_DATA

namespace sds110::params {

// --- Simulation (Harness / MicTask-Simulationspfad) ---------------------
constexpr float    SIM_INIT_DISTANCE = 100.0f;
constexpr float    SIM_MIN_DISTANCE  = 10.0f;
constexpr float    SIM_DISTANCE_STEP = 1.0f;
constexpr float    SIM_ANGLE_STEP    = 1.01f;
constexpr uint32_t SIM_NUM_SAMPLES   = 2048;

// --- UI / Display -------------------------------------------------------
constexpr uint32_t UI_FPS            = 60;
constexpr uint32_t DSP_FPS           = 30;      // Report-Rate (Patent ~30/s)
constexpr uint32_t NUM_POWER_BINS    = 36;      // Spektrum-Widget
constexpr float    RADAR_MAX_POWER   = 100.0f;

// --- Versionierung ------------------------------------------------------
constexpr uint32_t PARAM_VERSION     = 4;       // 3 = SDS, 4 = SDS_110

} // namespace sds110::params
