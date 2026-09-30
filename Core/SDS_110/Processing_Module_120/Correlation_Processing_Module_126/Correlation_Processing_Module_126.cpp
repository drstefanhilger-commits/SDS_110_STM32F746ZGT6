/*
 * Correlation_Processing_Module_126.cpp
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board
#include "Correlation_Processing_Module_126.hpp"
#include "Infrastructure/Utils/FftTables.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"
#include <cmath>
#include <cstring>

namespace sds110 {

// ---------------------------------------------------------------- init
float Correlation_Processing_Module_126::win_[2 * WIN_MAX + 1];

void Correlation_Processing_Module_126::init(const Microphone_Array_114& array)
{
    arm_rfft_fast_init_f32(&ifft_, N_FFT);
    fftTablesToRam(ifft_);                             // dieselben Twiddles wie 122, im internen RAM
    float dmax = 0.0f;
    for (uint32_t m = 0; m < NUM_MICS; ++m) {
        micPos_[m] = array.position(m);
        for (uint32_t n = m + 1; n < NUM_MICS; ++n) {
            const Vec3& a = array.position(m); const Vec3& b = array.position(n);
            const float d = std::sqrt((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
            if (d > dmax) dmax = d;
        }
    }
    dmax_ = dmax;
    updateGeometry();
    std::memset(pairCorr_, 0, sizeof(pairCorr_));
    for (uint32_t s = 0; s < SRP_AZ_STEPS; ++s) {
        const float phi = static_cast<float>(s) * (2.0f * PI / SRP_AZ_STEPS);
        srpCos_[s] = std::cos(phi); srpSin_[s] = std::sin(phi);
    }
    clearFeedback();
}

void Correlation_Processing_Module_126::clearFeedback()
{
    for (uint32_t b = 0; b < NUM_BANDS; ++b) { thetaSel_[b] = THETA_SEL; weightBoost_[b] = 1.0f; }
    feedback_ = TrackingFeedback{};
}

void Correlation_Processing_Module_126::applyFeedback(const TrackingFeedback& fb)
{
    feedback_ = fb;
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        const bool ref = fb.valid && fb.ref_state[b] > THETA_REF;
        thetaSel_[b]    = ref ? THETA_LOW : THETA_SEL;          // Abschnitt 10 (a)
        weightBoost_[b] = ref ? (1.0f + fb.ref_state[b]) : 1.0f; // Abschnitt 10 (b)
    }
}

// ---------------------------------------------------------------- Abschnitt 4
void Correlation_Processing_Module_126::deriveSelection(const AcousticState& s, ComponentSelection& sel) const
{
    std::memset(sel.selected, 0, sizeof(sel.selected));
    std::memset(sel.weight, 0, sizeof(sel.weight));
    sel.num_bands = sel.num_bins = 0;

    // Bänder mit p_b > θ_sel; falls weniger als B_MIN, die B_MIN stärksten nehmen
    bool take[NUM_BANDS];
    uint32_t n = 0;
    for (uint32_t b = 0; b < NUM_BANDS; ++b) { take[b] = s.p[b] > thetaSel_[b]; if (take[b]) ++n; }
    if (n < B_MIN) {
        for (uint32_t k = n; k < B_MIN; ++k) {
            uint32_t best = NUM_BANDS; float pv = -1.0f;
            for (uint32_t b = 0; b < NUM_BANDS; ++b)
                if (!take[b] && s.p[b] > pv) { pv = s.p[b]; best = b; }
            if (best == NUM_BANDS) break;
            take[best] = true;
        }
    }
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        if (!take[b]) continue;
        ++sel.num_bands;
        const float w = std::pow(s.p[b], WEIGHT_GAMMA) * weightBoost_[b];
        uint32_t k0, k1;
        Feature_Extraction_Module_122::bandBins(b, k0, k1);
        for (uint32_t k = k0; k < k1 && k < SPECTRUM_BINS; ++k) {
            sel.selected[k] = true; sel.weight[k] = w; ++sel.num_bins;
        }
    }
}

// ---------------------------------------------------------------- Schallgeschwindigkeit
void Correlation_Processing_Module_126::updateGeometry()
{
    maxIntraDelay_s_ = dmax_ / c_ * 1.1f;             // 10 % Reserve
    uint32_t idx = 0;
    for (uint32_t i = 0; i < NUM_MICS; ++i)
        for (uint32_t j = i + 1; j < NUM_MICS; ++j, ++idx) {
            pairDx_[idx] = (micPos_[j].x - micPos_[i].x) / c_ * SAMPLE_RATE_HZ;
            pairDy_[idx] = (micPos_[j].y - micPos_[i].y) / c_ * SAMPLE_RATE_HZ;
        }
}

void Correlation_Processing_Module_126::setSpeedOfSound(float c)
{
    if (c == c_ || !(c > 100.0f)) return;               // unverändert oder unplausibel
    c_ = c;
    updateGeometry();
    srpValid_ = false;                                  // pairCorr_ passt nicht mehr zu pairDx_
}

// ---------------------------------------------------------------- Abschnitt 5
int Correlation_Processing_Module_126::maxLagFor(float maxDelay_s)
{
    int maxLag = static_cast<int>(maxDelay_s * SAMPLE_RATE_HZ);
    if (maxLag < 1) maxLag = 1;
    if (maxLag > static_cast<int>(N_FFT / 2 - 1)) maxLag = N_FFT / 2 - 1;
    return maxLag;
}

bool Correlation_Processing_Module_126::prepareBins(const ComponentSelection& sel, int maxLag)
{
    direct_ = false;
    int need = maxLag + 1;                                     // Peak-Suche liest ±(maxLag+1)
    if (srpFrame_ && need < static_cast<int>(SRP_MAX_LAG)) need = static_cast<int>(SRP_MAX_LAG);
    if (directMaxBins_ == 0 || sel.num_bins == 0 || sel.num_bins > directMaxBins_ || need > WIN_MAX)
        return false;
    winHalf_ = need;
    nBins_ = 0;
    for (uint32_t k = 1; k < SPECTRUM_BINS && nBins_ < directMaxBins_; ++k) {
        if (!sel.selected[k]) continue;
        const float th = 2.0f * PI * static_cast<float>(k) / static_cast<float>(N_FFT);
        binK_[nBins_] = static_cast<uint16_t>(k);
        binW_[nBins_] = sel.weight[k];
        binCos_[nBins_] = std::cos(th); binSin_[nBins_] = std::sin(th);
        ++nBins_;
    }
    direct_ = true;
    return true;
}

bool Correlation_Processing_Module_126::crossCorrelate(const Spectrum& X, const Spectrum& Y,
                                                        const ComponentSelection& sel,
                                                        float maxDelay_s, TdoaMeasurement& out)
{
    const int maxLag = maxLagFor(maxDelay_s);
    prepareBins(sel, maxLag);
    return correlatePair(X, Y, sel, maxLag, out);
}

bool Correlation_Processing_Module_126::correlatePair(const Spectrum& X, const Spectrum& Y,
                                                       const ComponentSelection& sel,
                                                       int maxLag, TdoaMeasurement& out)
{
    out.valid = false;
    if (sel.num_bins == 0) return false;

    if (direct_) {
        // Schnellpfad: IFFT von R(k) nur für |Lag| <= winHalf_, wie arm_rfft_fast_f32 (inv., 1/N):
        // r[n] = (2/N) · Σ_k Re(R(k) · e^{+i2πkn/N}); ein Drehzeiger je Bin für ±n
        std::memset(win_, 0, sizeof(win_));
        constexpr float scale = 2.0f / static_cast<float>(N_FFT);
        float* const w0 = win_ + WIN_MAX;
        const int wh = winHalf_;
        for (uint32_t b = 0; b < nBins_; ++b) {
            const uint32_t k = binK_[b];
            const float cr = X.re[k] * Y.re[k] + X.im[k] * Y.im[k];
            const float ci = X.im[k] * Y.re[k] - X.re[k] * Y.im[k];
            const float mag = std::sqrt(cr * cr + ci * ci);
            if (mag < 1e-9f) continue;
            const float g = binW_[b] * scale / mag;
            const float ar = g * cr, ai = g * ci;
            const float c = binCos_[b], s = binSin_[b];
            // Lag +n und −n gemeinsam: e^{∓iθn} = (zr, ∓zi) ->
            //   r[+n] += ar·zr − ai·zi,  r[−n] += ar·zr + ai·zi  (ein Drehzeiger für beide)
            w0[0] += ar;
            float zr = c, zi = s;                              // e^{iθ·1}
            for (int n = 1; n <= wh; ++n) {
                const float re = ar * zr, im = ai * zi;
                w0[n]  += re - im;
                w0[-n] += re + im;
                const float t = zr * c - zi * s; zi = zr * s + zi * c; zr = t;
            }
        }
    } else {
        // R(k) = w(k) · X Y* / |X Y*| auf S(t), sonst 0  -> CMSIS-Packing [Re0, ReN/2, Re1, Im1, ...]
        std::memset(spec_, 0, sizeof(float) * N_FFT);
        for (uint32_t k = 1; k < SPECTRUM_BINS; ++k) {   // darüber nie selektiert
            if (!sel.selected[k]) continue;
            const float cr = X.re[k] * Y.re[k] + X.im[k] * Y.im[k];
            const float ci = X.im[k] * Y.re[k] - X.re[k] * Y.im[k];
            float mag = std::sqrt(cr * cr + ci * ci);
            if (mag < 1e-9f) continue;
            spec_[2 * k]     = sel.weight[k] * cr / mag;
            spec_[2 * k + 1] = sel.weight[k] * ci / mag;
        }
        arm_rfft_fast_f32(&ifft_, spec_, corr_, 1);      // reelle Kreuzkorrelation, zirkulär
    }

    // Peak in ±maxLag (IFFT: negative Lags liegen am Ende des Puffers)
    auto at = [this](int lag) { return lagValue(lag); };

    int bestLag = 0; float best = -1e30f;
    for (int lag = -maxLag; lag <= maxLag; ++lag) { const float v = at(lag); if (v > best) { best = v; bestLag = lag; } }
    if (best <= 0.0f) return false;
    // Maximum am Fensterrand: der eigentliche Peak liegt außerhalb von ±maxDelay
    if (bestLag == -maxLag || bestLag == maxLag) return false;

    // Peak-Ratio-Test: zweithöchstes lokales Maximum außerhalb der Hauptkeule.
    // Hauptkeule = vom Hauptpeak aus nach beiden Seiten, solange die Korrelation fällt
    // (bei schmalbandigen Harmonischen < 1,5 kHz deutlich breiter als wenige Samples).
    int lobeLo = bestLag, lobeHi = bestLag;
    while (lobeLo > -maxLag && at(lobeLo - 1) <= at(lobeLo)) --lobeLo;
    while (lobeHi <  maxLag && at(lobeHi + 1) <= at(lobeHi)) ++lobeHi;
    float second = 0.0f;
    for (int lag = -maxLag; lag <= maxLag; ++lag) {
        if (lag >= lobeLo && lag <= lobeHi) continue;
        const float v = at(lag);
        if (v > second && v >= at(lag - 1) && v >= at(lag + 1)) second = v;   // at() ist zirkulär
    }
    // ohne Nebenmaximum (Hauptkeule füllt das Fenster) wäre die Ratio unbegrenzt; begrenzt,
    // weil 128::solve() sie als Gewicht nutzt
    float ratio = (second > 0.0f) ? best / second : PEAK_RATIO_MAX;
    if (ratio > PEAK_RATIO_MAX) ratio = PEAK_RATIO_MAX;

    // Sub-Sample-Interpolation (Parabel)
    const float ym = at(bestLag - 1), y0 = at(bestLag), yp = at(bestLag + 1);
    const float den = ym - 2.0f * y0 + yp;
    const float delta = (std::fabs(den) > 1e-12f) ? 0.5f * (ym - yp) / den : 0.0f;

    out.tdoa_s     = (bestLag + delta) / static_cast<float>(SAMPLE_RATE_HZ);
    out.peak       = best;
    out.peak_ratio = ratio;
    out.valid      = ratio >= PEAK_RATIO_MIN;
    return out.valid;
}

// ---------------------------------------------------------------- Intra-Unit-Peilung
// Fernfeld: Quelle in Richtung u = (cos φ, sin φ) -> Ankunftszeit t_m = -(p_m · u) / c.
// crossCorrelate(X_i, X_j) liefert τ_ij = t_i - t_j = ((p_j - p_i) · u) / c
// (gleiche Konvention wie 128::solve: r_i - r_j = c·τ_ij). Gewichtete LS in (ux, uy),
// Gewicht = Peakhöhe; Azimut = atan2(uy, ux).
bool Correlation_Processing_Module_126::estimateBearing(const Spectrum* S, const ComponentSelection& sel, Bearing& out)
{
    out = Bearing{};
    float peakSum = 0;
    uint32_t idx = 0, valid = 0;
    const int maxLag = maxLagFor(maxIntraDelay_s_);
    srpFrame_ = srpReference() && srpCount_ == 0;              // Referenzscan in jedem srpEvery_-ten Frame
    if (srpReference()) srpCount_ = (srpCount_ + 1) % srpEvery_;
    prepareBins(sel, maxLag);                                  // einmal je Frame für alle 28 Paare
    srpValid_ = srpFrame_;                                     // pairCorr_ nur dann aktuell
    for (uint32_t i = 0; i < NUM_MICS; ++i) {
        for (uint32_t j = i + 1; j < NUM_MICS; ++j, ++idx) {
            TdoaMeasurement& m = pairTdoa_[idx];
            m.i = i; m.j = j;
            const bool ok = correlatePair(S[i], S[j], sel, maxLag, m);
            // Korrelationsfenster sichern: für den SRP-Referenzscan und für die Auflösung
            // mehrdeutiger Paare (Befund 26); jeder Frame, 65 bereits berechnete Werte je Paar
            for (int lag = -static_cast<int>(SRP_MAX_LAG); lag <= static_cast<int>(SRP_MAX_LAG); ++lag)
                pairCorr_[idx][lag + SRP_MAX_LAG] = (sel.num_bins == 0) ? 0.0f : lagValue(lag);
            if (ok) ++valid;
        }
    }
    const uint32_t minPairs = static_cast<uint32_t>(NUM_MIC_PAIRS * BEARING_MIN_PAIRS_FRACTION);
    guided_ = false; rejected_ = 0;
    out.valid_pairs = static_cast<uint8_t>(valid);
    if (sel.num_bins == 0) return false;

    float ux = 0.0f, uy = 0.0f;
    // Befund 26: bei hohem f0 hat eine Paarkorrelation mehrere fast gleich hohe Spitzen; einzelne
    // Paare nehmen die falsche (Versatz ≥ eine Periode der höchsten genutzten Frequenz, ≥ 12 Samples).
    // (1) robuste LS: solange ein Paar klar daneben liegt (> OUTLIER_SAMPLES), das Paar mit dem
    //     größten Residuum verwerfen. Rauschen bleibt fast immer darunter und kostet so keine Paare.
    TdoaMeasurement saved[NUM_MIC_PAIRS];
    std::memcpy(saved, pairTdoa_, sizeof(saved));
    bool solved = valid >= minPairs && solveLs(ux, uy, valid, peakSum);
    float resMax = solved ? maxResidualSamples(ux, uy) : 1e9f;
    while (solved && resMax > OUTLIER_SAMPLES && valid > minPairs) {
        int worst = -1; float wr = 0.0f;
        for (uint32_t k = 0; k < NUM_MIC_PAIRS; ++k) {
            const TdoaMeasurement& m = pairTdoa_[k];
            if (!m.valid) continue;
            const float r = std::fabs(m.tdoa_s * SAMPLE_RATE_HZ - (pairDx_[k] * ux + pairDy_[k] * uy));
            if (r > wr) { wr = r; worst = static_cast<int>(k); }
        }
        pairTdoa_[worst].valid = false; ++rejected_;
        solved = solveLs(ux, uy, valid, peakSum);
        resMax = solved ? maxResidualSamples(ux, uy) : 1e9f;
    }
    // (2) weiter inkonsistent oder zu wenige eindeutige Paare: Richtung aus dem SRP-Scan der
    //     gesicherten Paarkorrelationen, abweichende Paare dort neu wählen, klare Ausreißer verwerfen
    if (!solved || resMax > OUTLIER_SAMPLES) {
        TdoaMeasurement robust[NUM_MIC_PAIRS];
        std::memcpy(robust, pairTdoa_, sizeof(robust));
        const bool robustOk = false;                         // (1) war nicht konsistent
        const float uxR = ux, uyR = uy; const uint32_t validR = valid, rejR = rejected_;
        int step; float delta, best, second;
        bool guidedOk = false;
        if (scanSrp(step, delta, best, second)) {
            std::memcpy(pairTdoa_, saved, sizeof(saved));
            const float phi = (static_cast<float>(step) + delta) * (2.0f * PI / static_cast<float>(SRP_AZ_STEPS));
            repickPairs(std::cos(phi), std::sin(phi), maxLag);
            rejected_ = 0;
            if (solveLs(ux, uy, valid, peakSum) && valid >= minPairs) {
                for (uint32_t k = 0; k < NUM_MIC_PAIRS; ++k) {
                    TdoaMeasurement& m = pairTdoa_[k];
                    if (m.valid && std::fabs(m.tdoa_s * SAMPLE_RATE_HZ - (pairDx_[k] * ux + pairDy_[k] * uy)) > OUTLIER_SAMPLES)
                    { m.valid = false; ++rejected_; }
                }
                guidedOk = solveLs(ux, uy, valid, peakSum) && valid >= minPairs &&
                           maxResidualSamples(ux, uy) <= OUTLIER_SAMPLES;
            }
        }
        if (guidedOk) {
            guided_ = true;
        } else if (robustOk) {                               // Rauschen statt Mehrdeutigkeit
            std::memcpy(pairTdoa_, robust, sizeof(robust));
            ux = uxR; uy = uyR; rejected_ = rejR;
            solveLs(ux, uy, valid, peakSum);
            (void)validR;
        } else {
            std::memcpy(pairTdoa_, saved, sizeof(saved));
            uint32_t n = 0; for (const auto& m : saved) n += m.valid;
            out.valid_pairs = static_cast<uint8_t>(n);
            return false;                                    // keine falsche Peilung melden
        }
    }
    out.valid_pairs = static_cast<uint8_t>(valid);

    // Residuum
    float res = 0.0f; idx = 0;
    for (uint32_t i = 0; i < NUM_MICS; ++i)
        for (uint32_t j = i + 1; j < NUM_MICS; ++j, ++idx) {
            const TdoaMeasurement& m = pairTdoa_[idx];
            if (!m.valid) continue;
            const float pred = ((micPos_[j].x - micPos_[i].x) * ux + (micPos_[j].y - micPos_[i].y) * uy) / c_;
            res += m.peak * (m.tdoa_s - pred) * (m.tdoa_s - pred);
        }
    out.residual    = std::sqrt(res / peakSum);
    out.mean_peak   = peakSum / valid;
    out.azimuth_deg = Azimuth::fromArray(ux, uy);        // 0° = Nord, im Uhrzeigersinn
    out.valid = true;
    return true;
}

// Gewichtete LS in (ux, uy) über die gültigen Paare; valid/peakSum werden neu gezählt
bool Correlation_Processing_Module_126::solveLs(float& ux, float& uy, uint32_t& valid, float& peakSum) const
{
    float sxx = 0, sxy = 0, syy = 0, bx = 0, by = 0;
    valid = 0; peakSum = 0.0f;
    uint32_t idx = 0;
    for (uint32_t i = 0; i < NUM_MICS; ++i)
        for (uint32_t j = i + 1; j < NUM_MICS; ++j, ++idx) {
            const TdoaMeasurement& m = pairTdoa_[idx];
            if (!m.valid) continue;
            ++valid; peakSum += m.peak;
            const float ax = (micPos_[j].x - micPos_[i].x) / c_;
            const float ay = (micPos_[j].y - micPos_[i].y) / c_;
            const float w  = m.peak;
            sxx += w * ax * ax; sxy += w * ax * ay; syy += w * ay * ay;
            bx  += w * ax * m.tdoa_s; by += w * ay * m.tdoa_s;
        }
    if (valid < 2) return false;
    const float det = sxx * syy - sxy * sxy;
    if (std::fabs(det) < 1e-18f) return false;
    ux = ( syy * bx - sxy * by) / det;
    uy = (-sxy * bx + sxx * by) / det;
    return true;
}

// größtes |τ_gemessen − τ(u)| der gültigen Paare in Samples
float Correlation_Processing_Module_126::maxResidualSamples(float ux, float uy) const
{
    float worst = 0.0f;
    for (uint32_t k = 0; k < NUM_MIC_PAIRS; ++k) {
        const TdoaMeasurement& m = pairTdoa_[k];
        if (!m.valid) continue;
        const float r = std::fabs(m.tdoa_s * SAMPLE_RATE_HZ - (pairDx_[k] * ux + pairDy_[k] * uy));
        if (r > worst) worst = r;
    }
    return worst;
}

// Paare, die nicht zur Richtung (ux, uy) passen (oder ungültig sind, z. B. Peak-Ratio zu klein
// wegen der Mehrdeutigkeit): höchste lokale Spitze ±REPICK_HALF Samples um die vorhergesagte
// Verzögerung; ohne lokale Spitze dort ungültig
void Correlation_Processing_Module_126::repickPairs(float ux, float uy, int maxLag)
{
    const int lim = (maxLag - 1 < static_cast<int>(SRP_MAX_LAG) - 1) ? maxLag - 1 : static_cast<int>(SRP_MAX_LAG) - 1;
    for (uint32_t k = 0; k < NUM_MIC_PAIRS; ++k) {
        TdoaMeasurement& m = pairTdoa_[k];
        const float* c = pairCorr_[k] + SRP_MAX_LAG;             // c[lag], |lag| <= SRP_MAX_LAG
        const float pred = pairDx_[k] * ux + pairDy_[k] * uy;
        if (m.valid && std::fabs(m.tdoa_s * SAMPLE_RATE_HZ - pred) <= CONSIST_MAX_SAMPLES) continue;   // passt
        const int p0 = static_cast<int>(std::floor(pred + 0.5f));
        int lo = p0 - REPICK_HALF, hi = p0 + REPICK_HALF;
        if (lo < -lim) lo = -lim;
        if (hi >  lim) hi =  lim;
        int bestLag = 0; float best = 0.0f; bool found = false;
        for (int lag = lo; lag <= hi; ++lag) {
            const float v = c[lag];
            if (v > best && v >= c[lag - 1] && v >= c[lag + 1]) { best = v; bestLag = lag; found = true; }
        }
        if (!found) { m.valid = false; continue; }
        const float ym = c[bestLag - 1], yp = c[bestLag + 1];
        const float den = ym - 2.0f * best + yp;
        const float delta = (std::fabs(den) > 1e-12f) ? 0.5f * (ym - yp) / den : 0.0f;
        m.tdoa_s = (static_cast<float>(bestLag) + delta) / static_cast<float>(SAMPLE_RATE_HZ);
        m.peak   = best;
        m.valid  = true;
    }
}

// ---------------------------------------------------------------- SRP-PHAT-Referenz
// SRP(φ) = Σ_pairs C_ij(τ_ij(φ)), τ_ij(φ) = ((p_j - p_i)·u(φ)) / c · fs, linear interpoliert.
float Correlation_Processing_Module_126::srpAt(uint32_t step) const
{
    const float ux = srpCos_[step], uy = srpSin_[step];
    float acc = 0.0f;
    for (uint32_t k = 0; k < NUM_MIC_PAIRS; ++k) {
        // Lag in Samples + Versatz; |τ| <= Arraydurchmesser/c·fs (28 bei 20 °C, 31,4 bei −40 °C)
        // < SRP_MAX_LAG -> pos > 0,
        // die Ganzzahlumwandlung ist dann floor()
        const float pos = pairDx_[k] * ux + pairDy_[k] * uy + static_cast<float>(SRP_MAX_LAG);
        if (pos < 0.0f) continue;
        const int i0 = static_cast<int>(pos);
        if (i0 + 1 > static_cast<int>(2 * SRP_MAX_LAG)) continue;
        const float fr = pos - static_cast<float>(i0);
        acc += pairCorr_[k][i0] * (1.0f - fr) + pairCorr_[k][i0 + 1] * fr;
    }
    return acc;
}

bool Correlation_Processing_Module_126::scanSrp(int& bestStep, float& delta, float& best, float& second) const
{
    constexpr int N = static_cast<int>(SRP_AZ_STEPS);
    auto at = [&](int s) { return srpAt(static_cast<uint32_t>(((s % N) + N) % N)); };
    // grob: alle SRP_COARSE_STEP Grad (72 statt 360 Richtungen); die Hauptkeule des 200-mm-Arrays
    // ist bei den genutzten Frequenzen (< 4 kHz) viel breiter als 5°
    float bestC = -1e30f; int bc = 0;
    second = -1e30f;
    for (int s = 0; s < N; s += static_cast<int>(SRP_COARSE_STEP)) {
        const float acc = at(s);
        if (acc > bestC) { second = bestC; bestC = acc; bc = s; }
        else if (acc > second) second = acc;
    }
    // fein: ±SRP_FINE_HALF Grad im 1°-Raster um das grobe Maximum
    best = bestC; bestStep = bc;
    for (int d = -static_cast<int>(SRP_FINE_HALF); d <= static_cast<int>(SRP_FINE_HALF); ++d) {
        if (d == 0) continue;
        const float acc = at(bc + d);
        if (acc > best) { best = acc; bestStep = bc + d; }
    }
    if (best <= 0.0f) return false;
    // Parabel-Interpolation um das Maximum (1°-Raster)
    const float ym = at(bestStep - 1), yp = at(bestStep + 1);
    const float den = ym - 2.0f * best + yp;
    delta = (std::fabs(den) > 1e-12f) ? 0.5f * (ym - yp) / den : 0.0f;
    return true;
}

bool Correlation_Processing_Module_126::srpScan(float& azimuth_deg, float& peakPower, float& peakRatio) const
{
    if (!srpValid_) return false;
    int step; float delta, best, second;
    if (!scanSrp(step, delta, best, second)) return false;
    // Raster srpCos_/srpSin_ im Array-Koordinatensystem -> Azimut (0° = Nord, im Uhrzeigersinn)
    azimuth_deg = Azimuth::fromArrayAngle((static_cast<float>(step) + delta) * (360.0f / SRP_AZ_STEPS));
    // Verhältnis zum zweitbesten Punkt des Grobrasters (vorher: zweitbester 1°-Punkt, fast immer
    // der Nachbar des Maximums, Verhältnis ≈ 1)
    peakPower = best; peakRatio = best / (std::fabs(second) + 1e-9f);
    return true;
}

} // namespace sds110
