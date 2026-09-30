/*
 * Machine_Learning_Module_124.cpp – HBD / MLP -> s(t)
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board
#include "Machine_Learning_Module_124.hpp"
#include "arm_math.h"
#include <cmath>
#include <cstring>

namespace sds110 {

static inline float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

bool Machine_Learning_Module_124::init()
{
    HBD_InitParams_48k(hbdParams_);
    HBD_InitState(hbdState_, hbdParams_);
    frame_ = 0;
    sinceDetect_ = HBD_HOLD_FRAMES;
    ctxHead_ = ctxCount_ = 0;
    setMode(mode_);
    ready_ = true;
    return true;
}

void Machine_Learning_Module_124::setMode(Ml124Mode m)
{
    mode_ = m;
    smoothHbd_.reset();
    smoothMl_.reset();
    shadow_ = ShadowStats{};
}

bool Machine_Learning_Module_124::infer(const float* mag, const FeatureVector& features, AcousticState& s)
{
    if (!ready_ || !mag) return false;
    HBD_ProcessFrame(hbdParams_, hbdState_, mag);
    sinceDetect_ = hbdState_.droneDetected ? 0 : (sinceDetect_ < HBD_HOLD_FRAMES ? sinceDetect_ + 1 : HBD_HOLD_FRAMES);
    s.frame_id = frame_++;

    float pMl[NUM_BANDS];
    if (mode_ != Ml124Mode::Hbd) {
        pushContext(features);
        mlpForward(x_, mlOut_);
        std::memcpy(pMl, mlOut_, sizeof(pMl));
        applyGate(pMl);
        smoothMl_.apply(pMl);
    }
    if (mode_ != Ml124Mode::Ml) {
        bandProbabilities(s.p);
        applyGate(s.p);
        smoothHbd_.apply(s.p);
        if (mode_ == Ml124Mode::Shadow) updateShadow(s.p, pMl);
    } else {
        std::memcpy(s.p, pMl, sizeof(pMl));
    }
    return true;
}

// ------------------------------------------------------------------ HBD -> p_b
void Machine_Learning_Module_124::bandProbabilities(float* pOut) const
{
    const HBD_State& st = hbdState_;
    const HBD_Params& p = hbdParams_;

    // 1) Band-SNR aus Noise-Floor
    float q[NUM_BANDS];
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        uint32_t k0, k1; Feature_Extraction_Module_122::bandBins(b, k0, k1);
        float sig = -1e9f, noise = 0.0f;
        for (uint32_t k = k0; k < k1; ++k) { if (st.magDb[k] > sig) sig = st.magDb[k]; noise += st.noiseFloorDb[k]; }
        const float snr = (k1 > k0) ? sig - noise / static_cast<float>(k1 - k0) : 0.0f;
        q[b] = sigmoid((snr - HBD_BAND_SNR_DB) / HBD_SIGMOID_DB);
        pOut[b] = HBD_GATE_FLOOR * q[b];                    // ohne Harmonische: nie selektiert
    }

    // 2) Bänder mit Harmonischen
    const uint8_t H = (p.harmonic.numHarmonics > HBD_MAX_HARMONICS) ? HBD_MAX_HARMONICS : p.harmonic.numHarmonics;
    for (uint8_t h = 0; h < H; ++h) {
        if (st.harmonicBin[h] == 0) continue;
        const float hz = static_cast<float>(st.harmonicBin[h]) * SAMPLE_RATE_HZ / N_FFT;
        if (hz < BAND_LO_HZ || hz >= BAND_HI_HZ) continue;
        const uint32_t b = static_cast<uint32_t>((hz - BAND_LO_HZ) / BAND_WIDTH_HZ);
        if (b >= NUM_BANDS) continue;
        const float ph = st.consistencyHistory[h] * sigmoid((st.lastBandSnr[h] - p.snr.perBandSnrDb[h]) / HBD_SIGMOID_DB);
        const float v = (ph > q[b]) ? ph : q[b];
        if (v > pOut[b]) pOut[b] = v;
    }
}

// 3) Gate: HBD-Detektion innerhalb der letzten HBD_HOLD_FRAMES Frames
void Machine_Learning_Module_124::applyGate(float* p) const
{
    if (sinceDetect_ >= HBD_HOLD_FRAMES)
        for (uint32_t b = 0; b < NUM_BANDS; ++b) p[b] *= HBD_GATE_FLOOR;
}

// 4) Glättung
void Machine_Learning_Module_124::Smoother::apply(float* p)
{
    std::memcpy(hist[idx], p, sizeof(hist[idx]));
    idx = (idx + 1) % STATE_SMOOTH_FRAMES;
    if (count < STATE_SMOOTH_FRAMES) ++count;
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        float acc = 0.0f;
        for (uint32_t i = 0; i < count; ++i) acc += hist[i][b];
        p[b] = acc / count;
    }
}

// ------------------------------------------------------------------ MLP
void Machine_Learning_Module_124::pushContext(const FeatureVector& f)
{
    using namespace ml124model;
    ctxHead_ = (ctxHead_ + 1) % CONTEXT;
    float* c = ctx_[ctxHead_];
    std::memcpy(c, f.band_log_power, sizeof(f.band_log_power)); c += NUM_BANDS;
    std::memcpy(c, f.mel, sizeof(f.mel));                       c += MEL_BANDS;
    *c++ = f.spectral_flux;
    std::memcpy(c, f.band_am_depth, sizeof(f.band_am_depth));
    if (ctxCount_ == 0)                                         // Anfang: mit dem ersten Frame auffüllen
        for (uint32_t i = 0; i < CONTEXT; ++i)
            if (i != ctxHead_) std::memcpy(ctx_[i], ctx_[ctxHead_], sizeof(ctx_[i]));
    if (ctxCount_ < CONTEXT) ++ctxCount_;
    // x = (t, t−1, …): neuester zuerst
    for (uint32_t d = 0; d < CONTEXT; ++d)
        std::memcpy(x_ + d * ML_FEATURES, ctx_[(ctxHead_ + CONTEXT - d) % CONTEXT], sizeof(ctx_[0]));
}

void Machine_Learning_Module_124::mlpForward(float* x, float* out)
{
    using namespace ml124model;
    for (uint32_t i = 0; i < NUM_INPUTS; ++i) x[i] = (x[i] - MEAN[i]) / STD[i];

    float buf[2][MAX_WIDTH];
    const float* in = x;
    for (uint32_t l = 0; l < NUM_LAYERS; ++l) {
        const Layer& L = LAYERS[l];
        float* o = (l + 1 == NUM_LAYERS) ? out : buf[l & 1];
        for (uint32_t j = 0; j < L.n_out; ++j) {
            float acc;
            arm_dot_prod_f32(const_cast<float*>(L.W + j * L.n_in), const_cast<float*>(in), L.n_in, &acc);
            acc += L.b[j];
            o[j] = (L.act == Act::Relu) ? (acc > 0.0f ? acc : 0.0f) : sigmoid(acc);
        }
        in = o;
    }
}

void Machine_Learning_Module_124::updateShadow(const float* pHbd, const float* pMl)
{
    ShadowStats& st = shadow_;
    uint32_t nH = 0, nM = 0;
    float diff = 0.0f;
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        const bool h = pHbd[b] > THETA_SEL, m = pMl[b] > THETA_SEL;
        nH += h; nM += m; st.selBoth += (h && m);
        diff += std::fabs(pHbd[b] - pMl[b]);
    }
    const bool dH = nH >= B_MIN, dM = nM >= B_MIN;
    ++st.frames;
    st.selHbd += nH; st.selMl += nM;
    st.detectHbd += dH; st.detectMl += dM; st.detectAgree += (dH == dM);
    st.sumAbsDiff += diff / NUM_BANDS;
}

} // namespace sds110
