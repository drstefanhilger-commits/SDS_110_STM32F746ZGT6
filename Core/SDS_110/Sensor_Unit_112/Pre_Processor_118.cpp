/*
 * Pre_Processor_118.cpp
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board
#include "Pre_Processor_118.hpp"
#include "Infrastructure/Utils/DspScratch.hpp"
#include <cstring>
#include <cmath>

namespace sds110 {

// ---------------------------------------------------------------- Design
// RBJ Audio-EQ-Cookbook, Q = 1/sqrt(2) (Butterworth 2. Ordnung)
// Koeffizientenlayout CMSIS: b0 b1 b2 a1 a2 (a1/a2 negiert)
void Pre_Processor_118::designHighpass(float fc, float fs, float* c)
{
    const float w0 = 2.0f * PI * fc / fs;
    const float cw = std::cos(w0), sw = std::sin(w0);
    const float alpha = sw / (2.0f * 0.70710678f);
    const float a0 = 1.0f + alpha;
    c[0] = ((1.0f + cw) * 0.5f) / a0;
    c[1] = (-(1.0f + cw)) / a0;
    c[2] = ((1.0f + cw) * 0.5f) / a0;
    c[3] = -((-2.0f * cw) / a0);
    c[4] = -((1.0f - alpha) / a0);
}

void Pre_Processor_118::designLowpass(float fc, float fs, float* c)
{
    const float w0 = 2.0f * PI * fc / fs;
    const float cw = std::cos(w0), sw = std::sin(w0);
    const float alpha = sw / (2.0f * 0.70710678f);
    const float a0 = 1.0f + alpha;
    c[0] = ((1.0f - cw) * 0.5f) / a0;
    c[1] = (1.0f - cw) / a0;
    c[2] = ((1.0f - cw) * 0.5f) / a0;
    c[3] = -((-2.0f * cw) / a0);
    c[4] = -((1.0f - alpha) / a0);
}

void Pre_Processor_118::init()
{
    designHighpass(BANDPASS_LO_HZ, static_cast<float>(SAMPLE_RATE_HZ), &coeffs_[0]);
    designLowpass (BANDPASS_HI_HZ, static_cast<float>(SAMPLE_RATE_HZ), &coeffs_[5]);

    // Zeitkonstanten -> Glättungsfaktoren je Hop
    aAttack_  = 1.0f - std::exp(-HOP_S / AGC_ATTACK_TAU_S);
    aRelease_ = 1.0f - std::exp(-HOP_S / AGC_RELEASE_TAU_S);
    aFloor_   = 1.0f - std::exp(-HOP_S / NS_FLOOR_TAU_S);

    for (uint32_t ch = 0; ch < NUM_MICS; ++ch) {
        arm_biquad_cascade_df2T_init_f32(&iir_[ch], 2, coeffs_, state_[ch]);
        gain_[ch]     = 1.0f;
        noiseRms_[ch] = 1e-3f;
        applied_[ch]  = 1.0f;
        prevApplied_[ch] = 1.0f;
    }
}

// ---------------------------------------------------------------- Stufen
float Pre_Processor_118::rms(const float* x, uint32_t n)
{
    float r = 0.0f;
    arm_rms_f32(x, n, &r);
    return r;
}

void Pre_Processor_118::bandpass(uint32_t ch, float* x, uint32_t n)
{
    arm_biquad_cascade_df2T_f32(&iir_[ch], x, x, n);
}

// Verstärkung der Rauschunterdrückung aus dem Hop-RMS r (nach Bandpass)
float Pre_Processor_118::noiseGain(uint32_t ch, float r)
{
    // Rauschboden: schnelles Absenken, langsames Anheben (Minimum-Tracking)
    float& nf = noiseRms_[ch];
    if (r < nf) nf = r;
    else        nf += aFloor_ * (r - nf);

    // Wiener-artige Verstärkung: g = 1 - (nf/r)^2, begrenzt auf NS_MAX_ATTEN
    if (r <= 1e-9f) return 1.0f;
    const float snr = (r * r) / (nf * nf + 1e-12f);
    float g = 1.0f - 1.0f / snr;
    if (g < NS_MAX_ATTEN) g = NS_MAX_ATTEN;
    return (g < 1.0f) ? g : 1.0f;
}

// AGC-Verstärkung aus dem Hop-RMS r (nach NS)
float Pre_Processor_118::agcGain(uint32_t ch, float r)
{
    float& g = gain_[ch];
    if (r <= 1e-9f) return g;
    float target = AGC_TARGET_RMS / r;
    if (target > AGC_MAX_GAIN) target = AGC_MAX_GAIN;
    if (target < AGC_MIN_GAIN) target = AGC_MIN_GAIN;
    const float a = (target < g) ? aAttack_ : aRelease_;   // Pegel steigt -> schnell runterregeln
    g += a * (target - g);
    return g;
}

// ---------------------------------------------------------------- Hop
// NS und AGC ergeben eine Gesamtverstärkung je Hop. Sie wird als lineare Rampe vom Wert
// des vorigen Hops aus angewendet: Die Analyse-Frames (122) überdecken zwei Hops, ein
// Verstärkungssprung an der Hop-Grenze läge mitten im Frame und verschmierte das Spektrum.
// Zwischenpuffer eines Kanals im gemeinsamen DSP-Arbeitsspeicher (DspScratch.hpp): Rohdaten
// dekodieren, Bandpass, RMS und Rampe laufen dort, das Ergebnis geht als Blockgleitkomma zurück.
void Pre_Processor_118::process(MicFrame& frame)
{
    for (uint32_t ch = 0; ch < NUM_MICS; ++ch) {
        float* x = dspScratch();
        frame.decode(ch, x);
        if (bandpassOn_) bandpass(ch, x, HOP_SAMPLES);

        const float r = rms(x, HOP_SAMPLES);
        const float gNs  = nsOn_  ? noiseGain(ch, r) : 1.0f;
        const float gAgc = agcOn_ ? agcGain(ch, r * gNs) : 1.0f;
        const float g0 = applied_[ch], g1 = gNs * gAgc;
        if (g0 != 1.0f || g1 != 1.0f) {
            const float step = (g1 - g0) / static_cast<float>(HOP_SAMPLES);
            for (uint32_t i = 0; i < HOP_SAMPLES; ++i) x[i] *= g0 + step * static_cast<float>(i + 1);
        }
        prevApplied_[ch] = g0;
        applied_[ch] = g1;
        frame.encode(ch, x);
    }
    frame.processed = true;
}

} // namespace sds110
