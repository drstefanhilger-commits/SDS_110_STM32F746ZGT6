/*
 * Feature_Extraction_Module_122.cpp
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board
#include "Feature_Extraction_Module_122.hpp"
#include "Infrastructure/Utils/FftTables.hpp"
#include <cmath>
#include <cstring>

namespace sds110 {

// ---------------------------------------------------------------- init
void Feature_Extraction_Module_122::init()
{
    // Hann-Fenster über die 3072 echten Samples (aus FFTProcessor)
    for (uint32_t i = 0; i < FRAME_SAMPLES; ++i)
        window_[i] = 0.5f - 0.5f * std::cos(2.0f * PI * i / (FRAME_SAMPLES - 1));

    arm_rfft_fast_init_f32(&fft_, N_FFT);
    fftTablesToRam(fft_);                                      // Twiddles im internen RAM (Board)
    buildMelFilterbank();

    std::memset(prevMag_, 0, sizeof(prevMag_));
    std::memset(amHist_, 0, sizeof(amHist_));
    havePrev_ = false;
    amIdx_ = amCount_ = 0;
}

void Feature_Extraction_Module_122::buildMelFilterbank()
{
    // Dreiecksfilter, Mittenfrequenzen äquidistant auf der Mel-Skala
    const float mLo = hzToMel(MEL_LO_HZ), mHi = hzToMel(MEL_HI_HZ);
    float edgeBin[MEL_BANDS + 2];
    for (uint32_t i = 0; i < MEL_BANDS + 2; ++i) {
        const float hz = melToHz(mLo + (mHi - mLo) * i / (MEL_BANDS + 1));
        edgeBin[i] = hz * N_FFT / SAMPLE_RATE_HZ;          // Bin, nicht gerundet
    }
    uint32_t off = 0;
    for (uint32_t b = 0; b < MEL_BANDS; ++b) {
        const float lo = edgeBin[b], mid = edgeBin[b + 1], hi = edgeBin[b + 2];
        uint32_t k0 = static_cast<uint32_t>(std::ceil(lo));
        uint32_t k1 = static_cast<uint32_t>(std::floor(hi));
        if (k1 >= NUM_BINS) k1 = NUM_BINS - 1;
        if (k1 - k0 + 1 > MEL_MAX_BINS_PER_BAND) k1 = k0 + MEL_MAX_BINS_PER_BAND - 1;
        mel_[b].k0  = k0;
        mel_[b].len = (k1 >= k0) ? (k1 - k0 + 1) : 0;
        if (off + mel_[b].len > MEL_WEIGHTS_MAX) mel_[b].len = MEL_WEIGHTS_MAX - off;   // nicht erwartet (1306)
        mel_[b].off = off;
        for (uint32_t i = 0; i < mel_[b].len; ++i) {
            const float k = static_cast<float>(k0 + i);
            float w = (k <= mid) ? (k - lo) / (mid - lo) : (hi - k) / (hi - mid);
            melW_[off + i] = (w > 0.0f) ? w : 0.0f;
        }
        off += mel_[b].len;
    }
}

// ---------------------------------------------------------------- Bänder
void Feature_Extraction_Module_122::bandBins(uint32_t b, uint32_t& k_lo, uint32_t& k_hi)
{
    const float binHz = static_cast<float>(SAMPLE_RATE_HZ) / N_FFT;
    const float fLo = BAND_LO_HZ + b * BAND_WIDTH_HZ;
    const float fHi = fLo + BAND_WIDTH_HZ;
    k_lo = static_cast<uint32_t>(std::lround(fLo / binHz));
    k_hi = static_cast<uint32_t>(std::lround(fHi / binHz));
    if (k_hi > NUM_BINS) k_hi = NUM_BINS;
    if (k_lo >= k_hi)    k_lo = (k_hi > 0) ? k_hi - 1 : 0;
}

// ---------------------------------------------------------------- STFT
void Feature_Extraction_Module_122::computeSpectrum(const float* x, uint32_t n, Spectrum& out)
{
    if (n > FRAME_SAMPLES) n = FRAME_SAMPLES;
    arm_mult_f32(const_cast<float*>(x), window_, buf_, n);
    std::memset(buf_ + n, 0, sizeof(float) * (N_FFT - n));     // Zero-Padding
    fftPacked(out);
}

void Feature_Extraction_Module_122::computeSpectrum(const AnalysisFrame& frame, uint32_t ch, Spectrum& out)
{
    frame.decode(ch, buf_);                                      // beide Hop-Teile (Blockgleitkomma)
    arm_mult_f32(buf_, window_, buf_, FRAME_SAMPLES);            // Fenster, in place
    std::memset(buf_ + FRAME_SAMPLES, 0, sizeof(float) * (N_FFT - FRAME_SAMPLES));   // Zero-Padding
    fftPacked(out);
}

void Feature_Extraction_Module_122::fftPacked(Spectrum& out)
{
    arm_rfft_fast_f32(&fft_, buf_, fftOut_, 0);
    // CMSIS-Packing: [Re0, ReN/2, Re1, Im1, Re2, Im2, ...]
    // Nyquist-Bin (fftOut_[1]) liegt über SPECTRUM_BINS und wird nicht gespeichert
    out.re[0] = fftOut_[0];            out.im[0] = 0.0f;
    for (uint32_t k = 1; k < SPECTRUM_BINS; ++k) {
        out.re[k] = fftOut_[2 * k];
        out.im[k] = fftOut_[2 * k + 1];
    }
}

// ---------------------------------------------------------------- Merkmale
void Feature_Extraction_Module_122::computeBandPower(const float* mag, float* bandPow)
{
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        uint32_t k0, k1;
        bandBins(b, k0, k1);
        float p = 0.0f;
        for (uint32_t k = k0; k < k1; ++k) p += mag[k] * mag[k];
        bandPow[b] = p;
    }
}

void Feature_Extraction_Module_122::computeMel(const float* mag, float* mel)
{
    for (uint32_t b = 0; b < MEL_BANDS; ++b) {
        float v = 0.0f;
        if (mel_[b].len)
            arm_dot_prod_f32(melW_ + mel_[b].off, const_cast<float*>(mag + mel_[b].k0), mel_[b].len, &v);
        mel[b] = std::log(1e-6f + v);                          // Log-Mel wie MelSpectrogram
    }
}

float Feature_Extraction_Module_122::computeFlux(const float* mag)
{
    if (!havePrev_) { std::memcpy(prevMag_, mag, sizeof(prevMag_)); havePrev_ = true; return 0.0f; }
    float num = 0.0f, den = 1e-9f;
    for (uint32_t k = 0; k < NUM_BINS; ++k) {
        const float d = mag[k] - prevMag_[k];
        if (d > 0.0f) num += d * d;
        den += mag[k] * mag[k];
    }
    std::memcpy(prevMag_, mag, sizeof(prevMag_));
    return std::sqrt(num / den);
}

void Feature_Extraction_Module_122::updateAm(const float* bandPow, float* amDepth)
{
    std::memcpy(amHist_[amIdx_], bandPow, sizeof(float) * NUM_BANDS);
    amIdx_ = (amIdx_ + 1) % AM_HISTORY_FRAMES;
    if (amCount_ < AM_HISTORY_FRAMES) ++amCount_;

    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        float mean = 0.0f;
        for (uint32_t i = 0; i < amCount_; ++i) mean += amHist_[i][b];
        mean /= amCount_;
        float var = 0.0f;
        for (uint32_t i = 0; i < amCount_; ++i) {
            const float d = amHist_[i][b] - mean;
            var += d * d;
        }
        var /= amCount_;
        amDepth[b] = (mean > 1e-12f) ? std::sqrt(var) / mean : 0.0f;   // Modulationstiefe
    }
}

// ---------------------------------------------------------------- Frame
void Feature_Extraction_Module_122::process(const AnalysisFrame& frame, Spectrum& refSpectrum, FeatureVector& f)
{
    computeSpectrum(frame, REF_MIC, refSpectrum);

    // |X| über alle Bins direkt aus der gepackten FFT (fftOut_ gilt noch); gleiche Rechnung wie
    // bisher aus dem vollen Spektrum (Im von Bin 0 und N/2 ist 0)
    mag_[0] = std::sqrt(fftOut_[0] * fftOut_[0] + 0.0f * 0.0f);
    mag_[NUM_BINS - 1] = std::sqrt(fftOut_[1] * fftOut_[1] + 0.0f * 0.0f);
    for (uint32_t k = 1; k < NUM_BINS - 1; ++k)
        mag_[k] = std::sqrt(fftOut_[2 * k] * fftOut_[2 * k] + fftOut_[2 * k + 1] * fftOut_[2 * k + 1]);

    float bandPow[NUM_BANDS];
    computeBandPower(mag_, bandPow);
    for (uint32_t b = 0; b < NUM_BANDS; ++b) f.band_log_power[b] = std::log(1e-12f + bandPow[b]);

    computeMel(mag_, f.mel);
    f.spectral_flux = computeFlux(mag_);
    updateAm(bandPow, f.band_am_depth);
}

} // namespace sds110
