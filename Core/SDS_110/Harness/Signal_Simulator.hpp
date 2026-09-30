/*
 * Signal_Simulator.hpp  (Harness, kein Patentmodul)
 *
 * Erzeugt synthetische Mikrofonsignale ohne Hardware und speist sie über
 * Microphone_Array_114::pushBlock() ein – identisch zum DMA-Pfad von 116.
 * Alles ab 118 läuft damit real auf dem Board.
 *
 * Szenarien (SimScenario):
 *   DroneSweep   – Rotorsignal (f0 + Harmonische mit Blattpass-AM), Azimut dreht, Distanz steigt
 *   DroneStatic  – wie oben, feste Position (Parameter testen)
 *   SingleTone   – ein reiner Ton (harmonisch NICHT drohnentypisch) -> Fehlalarm-Test
 *   WindNoise    – tieffrequentes Rauschen (1/f) -> Fehlalarm-Test
 *   Silence      – nur Grundrauschen -> Noise-Floor-Einschwingen
 *   FlyBy        – gerader Überflug (Standard 5 s), danach Pause mit Rauschen (5 s), wiederholt;
 *                  die Flugrichtung dreht je Durchgang um flyby_track_step_deg. Die Bahn liegt im
 *                  lokalen System (LocalPosition.hpp) mit dem kürzesten Abstand zum Ursprung;
 *                  Azimut und Distanz gelten von der Position der Einheit aus (setUnitPosition)
 *
 * Auswahl zur Laufzeit über USB-Kommando Id 3 (SimScenario.hpp).
 *
 * Ersetzt SDS/Harness/UnitTestSignals (Breitbandrauschen) und MicTask::simulateMic.
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"
#include "Sensor_Unit_112/Microphone_Array_114.hpp"
#include "SimScenario.hpp"

namespace sds110 {

struct SimParams {
    SimScenario scenario   = SimScenario::DroneSweep;
    float azimuth_deg      = 30.0f;    // Startwert bzw. fest
    float distance_m       = 50.0f;
    float f0_hz            = 180.0f;   // Rotor-Grundfrequenz (Drone) / Tonfrequenz (SingleTone)
    uint8_t harmonics      = 8;
    float bpf_mod_hz       = 4.0f;     // Blattpass-Amplitudenmodulation
    float snr_db           = 20.0f;    // Quelle zu Rauschen am Array (bei distance_m)
    float sweep_az_step    = 1.0f;     // DroneSweep: Grad pro Frame
    float sweep_dist_step  = 0.5f;     // DroneSweep: m pro Umdrehung
    float source_level     = 0.3f;     // Amplitude bei 1 m (float-Vollaussteuerung = 1)
    // FlyBy: gerade Bahn im lokalen System, Mitte des Flugs am kürzesten Abstand zum Ursprung.
    // snr_db gilt für eine Quelle in flyby_cpa_m Abstand; das Rauschen bleibt über Flug und Pause
    // gleich. Steht die Einheit nicht im Ursprung, ändern sich Abstand, Azimut und SNR entsprechend.
    float flyby_speed_mps      = 15.0f;
    float flyby_cpa_m          = 30.0f;   // kürzester Abstand der Bahn zum Ursprung
    float flyby_alt_m          = 0.0f;    // Flughöhe (Oben) im lokalen System
    float flyby_track_deg      = 90.0f;   // Flugrichtung im ersten Durchgang (0° = Nord, im Uhrzeigersinn)
    float flyby_track_step_deg = 45.0f;   // Drehung der Flugrichtung je Durchgang
    float flyby_flight_s       = 5.0f;    // Flugdauer
    float flyby_pause_s        = 5.0f;    // Pause (nur Rauschen)
};

class Signal_Simulator {
public:
    static Signal_Simulator& instance();
    void init(const SimParams& p);
    SimParams& params() { return p_; }
    /// erzeugt einen Hop (HOP_SAMPLES je Mikrofon) und pusht ihn blockweise in 114
    void generateHop(uint64_t time_utc_us);
    float trueAzimuth() const { return p_.azimuth_deg; }
    float trueDistance() const { return p_.distance_m; }
    /// false während der FlyBy-Pause (keine Quelle; Azimut/Distanz bleiben auf dem letzten Wert)
    bool  sourceActive() const { return active_; }
    /// Position der Einheit im lokalen System (Ost, Nord, Oben in m); Grundwert Ursprung
    void  setUnitPosition(float eastM, float northM, float upM) { unit_[0] = eastM; unit_[1] = northM; unit_[2] = upM; }
    /// FlyBy: Richtung und Abstand der Quelle von der Einheit (unitENU: Ost, Nord, Oben in m) nach
    /// t Sekunden ab Beginn des Durchgangs cycle (az 0° = Nord, im Uhrzeigersinn); false in der Pause
    static bool flyByPosition(const SimParams& p, float t, uint32_t cycle, const float unitENU[3],
                              float& azDeg, float& distM);
    /// Schallgeschwindigkeit der simulierten Luft (m/s); ProcessingTask setzt sie wie in 126
    void  setSpeedOfSound(float c) { if (c > 100.0f) c_ = c; }

private:
    Signal_Simulator() = default;
    float noise();
    void  advanceSweep();
    void  advanceFlyBy();

    SimParams p_{};
    Microphone_Array_114& array_ = Microphone_Array_114::instance();
    float    delaySamples_[NUM_MICS] = {};
    float    c_ = SPEED_OF_SOUND;
    // Oszillatoren als Zeiger (Re, Im) in float, je Sample um den Schritt gedreht: auf dem Board
    // (FPU nur einfache Genauigkeit) war sin() in double je Harmonische und Sample der Großteil
    // der Rechenzeit des ProcessingTask. Im entspricht dem bisherigen sin(Phase).
    static constexpr uint32_t MAX_HARM = 16;
    float    oscRe_[MAX_HARM] = {}, oscIm_[MAX_HARM] = {};
    float    stepRe_[MAX_HARM] = {}, stepIm_[MAX_HARM] = {};
    float    amRe_ = 1.0f, amIm_ = 0.0f, amStepRe_ = 1.0f, amStepIm_ = 0.0f;
    void     updateOscillators();        // Schritte aus f0/bpf_mod_hz, Zeiger normieren (je Hop)
    bool     active_ = true;             // Quelle hörbar (FlyBy: false in der Pause)
    float    unit_[3] = {};              // Position der Einheit (Ost, Nord, Oben), LocalPosition
    float    flyT_ = 0.0f;               // FlyBy: Zeit im Durchgang (s)
    uint32_t flyCycle_ = 0;              // FlyBy: Durchgang (Flugrichtung)
    uint32_t rng_ = 0x12345678;
    float    pinkState_[3] = {};
    // Quellsignal: [GUARD Vergangenheit][HOP_SAMPLES aktuell][GUARD Vorlauf]; zwischen zwei
    // Aufrufen wird um HOP_SAMPLES geschoben, jedes Sample wird genau einmal erzeugt
    static constexpr uint32_t GUARD = 64;   // > max. Verzögerung (Radius 0,1 m, −40 °C -> 16 Samples)
    float src_[HOP_SAMPLES + 2 * GUARD];
    bool  primed_ = false;
    float sourceSample();
    int32_t block_[DMA_BLOCK_SAMPLES * NUM_MICS];
};

} // namespace sds110
