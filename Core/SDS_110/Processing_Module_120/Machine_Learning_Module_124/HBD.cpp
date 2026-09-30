/*
 * HBD.cpp – Harmonic Band Detector
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board
#include "HBD.hpp"
#include <cmath>

namespace sds110 {

static inline float clampf(float x, float lo, float hi) { return (x < lo) ? lo : (x > hi) ? hi : x; }
static inline float magToDb(float mag) { return 20.0f * std::log10(mag + 1e-12f); }

void HBD_InitParams_48k(HBD_Params& p)
{
    // Hann über FRAME_SAMPLES (122): Σw = (N-1)/2 -> |X|·2/Σw = Amplitude eines Sinus
    const float windowSum = 0.5f * static_cast<float>(FRAME_SAMPLES - 1);
    p.fft = { SAMPLE_RATE_HZ, static_cast<uint16_t>(N_FFT), static_cast<uint16_t>(HOP_SAMPLES), 2.0f / windowSum };
    p.harmonic = { 80.0f, 350.0f, 8, 40.0f };
    // Grenzen in dBFS; die alte Obergrenze -60 dB lag unter dem Rauschpegel nach der AGC
    // Werte je Frame aus den Zeitkonstanten (Frame-Takt = Hop, 32 ms); stehender Ton wird nach ~2 min gelernt
    p.noiseFloor = { std::exp(-HOP_S / HBD_FLOOR_TAU_S), -140.0f, 0.0f, 10.0f, HBD_FLOOR_RISE_DB_S * HOP_S, 8 };
    p.snr.globalSnrDb = 12.0f;
    p.snr.numBandsUsed = 8;
    const float thr[8] = { 14, 12, 10, 8, 7, 6, 5, 4 };
    for (int i = 0; i < 8; ++i) p.snr.perBandSnrDb[i] = thr[i];
    p.consistency = { 0.60f, 5, static_cast<uint8_t>(framesFor(HBD_CONSISTENCY_S)) };
    const float w[8] = { 0.32f, 0.28f, 0.20f, 0.12f, 0.05f, 0.02f, 0.01f, 0.005f };
    for (int i = 0; i < 8; ++i) p.decision.bandWeights[i] = w[i];
    p.decision.numWeightsUsed = 8;
    p.decision.finalScoreThreshold = 0.48f;
    p.decision.warmupFrames = static_cast<uint16_t>(framesFor(HBD_WARMUP_S));   // Floor schwingt nach dem Start (AGC-Anlauf) ~2,5 s ein
}

void HBD_InitState(HBD_State& s, const HBD_Params& p)
{
    for (uint32_t i = 0; i < NUM_BINS; ++i) { s.noiseFloorDb[i] = p.noiseFloor.minFloorDb; s.magDb[i] = p.noiseFloor.minFloorDb; s.localDb[i] = p.noiseFloor.minFloorDb; }
    for (uint8_t k = 0; k < HBD_MAX_HARMONICS; ++k) { s.lastBandSnr[k] = 0.0f; s.consistencyHistory[k] = 0.0f; s.harmonicBin[k] = 0; }
    s.f0Hz = 0.0f; s.globalSnrAvgDb = 0.0f; s.score = 0.0f;
    s.stableCount = 0; s.consistentBands = 0; s.droneDetected = false;
    s.floorInit = false;
    s.frameCount = 0;
}

// Mittlerer dB-Pegel über ±localHalfWidthBins (gleitende Summe)
static void localMeanDb(const HBD_Params& p, HBD_State& s)
{
    const int hw = p.noiseFloor.localHalfWidthBins, n = static_cast<int>(NUM_BINS);
    float sum = 0.0f; int cnt = 0;
    for (int j = 0; j <= hw && j < n; ++j) { sum += s.magDb[j]; ++cnt; }
    for (int i = 0; i < n; ++i) {
        s.localDb[i] = sum / static_cast<float>(cnt);
        const int add = i + hw + 1, rem = i - hw;
        if (add < n) { sum += s.magDb[add]; ++cnt; }
        if (rem >= 0) { sum -= s.magDb[rem]; --cnt; }
    }
}

bool HBD_ProcessFrame(const HBD_Params& p, HBD_State& s, const float* mag)
{
    const float df = static_cast<float>(SAMPLE_RATE_HZ) / N_FFT;
    const uint8_t H = (p.harmonic.numHarmonics > HBD_MAX_HARMONICS) ? HBD_MAX_HARMONICS : p.harmonic.numHarmonics;

    // 1) dBFS je Bin und Noise-Floor nachführen
    for (uint32_t i = 0; i < NUM_BINS; ++i) s.magDb[i] = magToDb(mag[i] * p.fft.windowGain);
    localMeanDb(p, s);
    if (!s.floorInit) {
        for (uint32_t i = 0; i < NUM_BINS; ++i) s.noiseFloorDb[i] = s.localDb[i];
        s.floorInit = true;
    }
    const float a = 1.0f - p.noiseFloor.smoothingFactor;
    for (uint32_t i = 0; i < NUM_BINS; ++i) {
        float step = a * (s.magDb[i] - s.noiseFloorDb[i]);   // EMA (auf und ab)
        const bool peak = s.magDb[i] > s.localDb[i] + p.noiseFloor.peakMarginDb;
        if (peak && step > p.noiseFloor.maxRiseDbPerFrame)
            step = p.noiseFloor.maxRiseDbPerFrame;           // schmaler Peak: nur langsam anheben
        s.noiseFloorDb[i] = clampf(s.noiseFloorDb[i] + step, p.noiseFloor.minFloorDb, p.noiseFloor.maxFloorDb);
    }

    // 2) f0: stärkster Peak im Grundfrequenzbereich
    uint32_t binMin = static_cast<uint32_t>(p.harmonic.fundamentalMinHz / df);
    uint32_t binMax = static_cast<uint32_t>(p.harmonic.fundamentalMaxHz / df);
    if (binMax >= NUM_BINS) binMax = NUM_BINS - 1;
    uint32_t f0Bin = binMin; float f0Mag = -1e9f;
    for (uint32_t i = binMin; i <= binMax; ++i)
        if (s.magDb[i] > f0Mag) { f0Mag = s.magDb[i]; f0Bin = i; }
    s.f0Hz = df * static_cast<float>(f0Bin);

    // 3) SNR je Harmonischer + Konsistenz
    const uint32_t halfWidthBins = static_cast<uint32_t>(p.harmonic.bandHalfWidthHz / df);
    uint8_t stableCount = 0; float globalSnrSum = 0.0f;
    for (uint8_t h = 0; h < H; ++h) {
        const uint32_t centerBin = static_cast<uint32_t>(s.f0Hz * (h + 1) / df);
        if (centerBin >= NUM_BINS) { s.lastBandSnr[h] = 0.0f; s.harmonicBin[h] = 0; continue; }
        s.harmonicBin[h] = static_cast<uint16_t>(centerBin);
        const uint32_t b0 = (centerBin > halfWidthBins) ? centerBin - halfWidthBins : 0;
        uint32_t b1 = centerBin + halfWidthBins; if (b1 >= NUM_BINS) b1 = NUM_BINS - 1;

        float sigDb = -1e9f, noiseSum = 0.0f;
        for (uint32_t i = b0; i <= b1; ++i) { if (s.magDb[i] > sigDb) sigDb = s.magDb[i]; noiseSum += s.noiseFloorDb[i]; }
        const float noiseDb = noiseSum / static_cast<float>(b1 - b0 + 1);
        const float snrDb = sigDb - noiseDb;
        s.lastBandSnr[h] = snrDb;
        globalSnrSum += snrDb;

        const bool stable = (snrDb >= p.snr.perBandSnrDb[h]);
        if (stable) ++stableCount;
        s.consistencyHistory[h] = (s.consistencyHistory[h] * (p.consistency.temporalWindow - 1) + (stable ? 1.0f : 0.0f))
                                  / static_cast<float>(p.consistency.temporalWindow);
    }
    s.stableCount = stableCount;
    s.globalSnrAvgDb = globalSnrSum / static_cast<float>(H);

    // 4) Konsistenz
    uint8_t consistent = 0;
    for (uint8_t h = 0; h < H; ++h) if (s.consistencyHistory[h] >= p.consistency.minConsistency) ++consistent;
    s.consistentBands = consistent;
    const bool consistencyOk = (consistent >= p.consistency.minStableHarmonics);

    // 5) Score
    float score = 0.0f;
    for (uint8_t h = 0; h < p.decision.numWeightsUsed && h < H; ++h) {
        float snrNorm = s.lastBandSnr[h] / (p.snr.globalSnrDb + 1e-6f);
        if (snrNorm < 0.0f) snrNorm = 0.0f;
        score += p.decision.bandWeights[h] * snrNorm;
    }
    s.score = score;

    // 6) Entscheidung
    const bool snrOk   = (s.globalSnrAvgDb >= p.snr.globalSnrDb);
    const bool scoreOk = (score >= p.decision.finalScoreThreshold);
    if (s.frameCount < p.decision.warmupFrames) ++s.frameCount;
    const bool warm = s.frameCount >= p.decision.warmupFrames;
    s.droneDetected = warm && snrOk && consistencyOk && scoreOk;
    return s.droneDetected;
}

} // namespace sds110
