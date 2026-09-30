/*
 * Candidate_Report_140.hpp
 * Datenschnittstelle 140 (Patent, Abschnitt 7, FIG. 6).
 *
 * Zuordnung Patent -> Hardware:
 *   STM32-Board (SDS_110-Firmware) = Sensoreinheit 112-n, mit vorverlagerten Stufen
 *                                    122, 124 und dem Intra-Unit-Teil von 126.
 *   PC-Monitor                     = Processing Module 120 (Inter-Unit-Teil von 126,
 *                                    128, 130) und – als getrennte Komponente – Tracking Unit 150.
 *
 * Daher zwei Report-Typen:
 *   UnitReport      : Board -> PC (pro Frame und Einheit). Kein Patentbegriff; Vorstufe.
 *   CandidateReport : 130 -> 150 (Patent FIG. 6). Entsteht erst aus der Inter-Unit-TDOA-Lösung
 *                     in 128 auf dem PC. Hier definiert als gemeinsame Vereinbarung beider Seiten.
 *   TrackingFeedback: 150 -> 126 (Patent Abschnitt 10, FIG. 8).
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"
#include "Infrastructure/Utils/UtcClock.hpp"

namespace sds110 {

/// UAV source-specific acoustic state s(t) = (p_1 .. p_B)   [Abschnitt 3]
struct AcousticState {
    float    p[NUM_BANDS] = {};
    uint32_t frame_id     = 0;
};

/// Bericht einer Sensoreinheit an das Processing Module (Board -> PC)
struct UnitReport {
    uint16_t unit_id        = 0;   // eindeutige Kennung der Sensoreinheit 112-n
    uint64_t time_utc_us    = 0;   // Frame-Beginn: UTC in µs, ohne Abgleich Laufzeit (time_source)
    TimeSource time_source  = TimeSource::Uptime;
    float    bearing_deg    = 0;   // Intra-Unit-Peilung (126, Fernfeld)
    float    bearing_residual = 0; // LS-Residuum der Peilung (s)
    float    confidence     = 0;   // 0..1 aus 128 (nur Legacy-Frame, nicht im UnitReport-Wire-Format)
    uint8_t  valid_pairs    = 0;   // gültige Mikrofonpaare (max. NUM_MIC_PAIRS)
    float    level          = 0;   // Pegelmaß der selektierten Bänder (Einzel-Unit-Distanz-Fallback)
    uint8_t  num_selected   = 0;
    uint8_t  band_index[NUM_BANDS] = {};
    float    band_prob [NUM_BANDS] = {};
};

/// Kandidatenbericht 130 -> 150 (Patent FIG. 6), erzeugt vom Processing Module auf dem PC
struct CandidateReport {
    uint64_t time_utc_us    = 0;   // Zeitreferenz
    float    azimuth_deg    = 0;   // φ, relativ zum Referenzpunkt (Zentroid der Units)
    float    distance_m     = 0;   // r
    uint8_t  accepted_pairs = 0;   // Qualität: akzeptierte Unit-Paare
    float    ls_residual    = 0;   // Qualität: Least-Squares-Residuum
    uint8_t  num_selected   = 0;
    uint8_t  band_index[NUM_BANDS] = {};
    float    band_prob [NUM_BANDS] = {};
};

/// Rückmeldung der Tracking Unit                              [Abschnitt 10, FIG. 8]
struct TrackingFeedback {
    float    ref_state[NUM_BANDS] = {}; // ŝ
    float    pred_azimuth_deg     = 0;  // x̂(t+1)
    float    pred_distance_m      = 0;
    bool     valid                = false;
};

} // namespace sds110
