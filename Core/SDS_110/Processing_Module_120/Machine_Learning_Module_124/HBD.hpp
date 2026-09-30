/*
 * HBD.hpp – Harmonic Band Detector (klassische Routine, kein trainiertes Modell)
 *
 * Übernommen aus der HBD-Vorlage (STM32F746 @ 48 kHz), angepasst an SDS_110:
 *  - arbeitet auf dem Betragsspektrum |X(t,k)| aus 122 (NUM_BINS = N_FFT/2+1 statt N_FFT)
 *  - dB-Werte je Bin werden einmal pro Frame berechnet (magDb_), nicht mehrfach
 *  - Parameter/Zustand bleiben Strukturen wie in der Vorlage, Werte über HBD_InitParams_48k()
 *  - Pegel in dBFS: |X| wird mit windowGain = 2/Σw normiert (Sinus der Amplitude A -> 20·log10(A))
 *  - Noise-Floor: EMA je Bin; Bins, die deutlich über ihrer spektralen Umgebung liegen
 *    (schmale Peaks, z. B. Rotorharmonische), heben ihn nur ratenbegrenzt an, damit ein
 *    stehender Drohnenton nicht innerhalb von ~1 s als Rauschen gelernt wird. Breitbandige
 *    Pegeländerungen (AGC, Wind) folgen normal. Start: Umgebungsmittel des ersten Frames.
 *
 * Ablauf pro Frame: Noise-Floor-Nachführung -> f0-Suche 80..350 Hz -> SNR je Harmonischer
 * -> Konsistenzhistorie -> gewichteter Score -> Entscheidung.
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"

namespace sds110 {

struct HBD_FftParams        { uint32_t sampleRateHz; uint16_t fftSize; uint16_t hopSize; float windowGain; };
struct HBD_HarmonicBand     { float fundamentalMinHz; float fundamentalMaxHz; uint8_t numHarmonics; float bandHalfWidthHz; };
struct HBD_NoiseFloorParams {
    float smoothingFactor;       // EMA-Faktor für rauschartige Bins
    float minFloorDb;            // Grenzen des Floors (dBFS)
    float maxFloorDb;
    float peakMarginDb;          // Bin gilt als Peak, wenn magDb > Umgebungsmittel + Marge
    float maxRiseDbPerFrame;     // max. Anstieg des Floors je Frame an Peak-Bins
    uint16_t localHalfWidthBins; // Umgebung: mittlerer dB-Pegel über ±Bins
};
struct HBD_SnrThresholds    { float globalSnrDb; float perBandSnrDb[8]; uint8_t numBandsUsed; };
struct HBD_ConsistencyParams{ float minConsistency; uint8_t minStableHarmonics; uint8_t temporalWindow; };
struct HBD_DecisionParams   { float bandWeights[8]; uint8_t numWeightsUsed; float finalScoreThreshold;
                              uint16_t warmupFrames; };   // keine Entscheidung, solange der Noise-Floor einschwingt

struct HBD_Params {
    HBD_FftParams         fft;
    HBD_HarmonicBand      harmonic;
    HBD_NoiseFloorParams  noiseFloor;
    HBD_SnrThresholds     snr;
    HBD_ConsistencyParams consistency;
    HBD_DecisionParams    decision;
};

constexpr uint8_t HBD_MAX_HARMONICS = 8;

struct HBD_State {
    float   noiseFloorDb[NUM_BINS];              // Noise-Floor pro Bin (dB)
    float   magDb[NUM_BINS];                     // |X| in dBFS, aktuelles Frame
    float   localDb[NUM_BINS];                   // spektrales Umgebungsmittel von magDb
    float   lastBandSnr[HBD_MAX_HARMONICS];      // SNR je Harmonischer (dB)
    float   consistencyHistory[HBD_MAX_HARMONICS];
    uint16_t harmonicBin[HBD_MAX_HARMONICS];     // Mittenbin je Harmonischer (0 = ungültig)
    float   f0Hz;
    float   globalSnrAvgDb;
    float   score;
    uint8_t stableCount;
    uint8_t consistentBands;
    bool    droneDetected;
    bool    floorInit;                           // Noise-Floor initialisiert
    uint32_t frameCount;                         // verarbeitete Frames (Anlaufzeit)
};

void HBD_InitParams_48k(HBD_Params& p);
void HBD_InitState(HBD_State& s, const HBD_Params& p);

/// mag: Betragsspektrum, NUM_BINS Werte. Rückgabe: Drohne erkannt (Score/Konsistenz/SNR).
bool HBD_ProcessFrame(const HBD_Params& p, HBD_State& s, const float* mag);

} // namespace sds110
