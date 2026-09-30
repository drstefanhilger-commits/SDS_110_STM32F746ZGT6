/*
 * t_gcc_direct – Schnellpfad der GCC-PHAT in 126 (direkte Korrelation nur für die Lags ±64)
 *
 * Bezug: Rechenzeit ProcessingTask (Board ~270 ms je 32-ms-Hop, 126 größter Anteil im Host-Profil)
 * Prüft, dass der Schnellpfad dieselben Ergebnisse liefert wie die IFFT (setDirectMaxBins(0)):
 *   1. crossCorrelate() auf verzögerten Spektren (τ innerhalb ±90 % des Intra-Unit-Fensters,
 *      bei 200 mm ±27 Samples; 3–24 Bänder, Rauschen):
 *      gleiche Gültigkeit, |Δτ| ≤ 1e-3 Samples, |Δpeak|, |Δratio| relativ ≤ 1e-3
 *   2. volle Kette (DroneStatic 10 dB, 12 Richtungen): estimateBearing() und srpScan() gleich
 *      (|Δaz| ≤ 0,01°), der Schnellpfad wird tatsächlich benutzt
 *   3. SRP-Referenzscan abgeschaltet (setSrpReference(false), USB Typ 6): Peilung bitgleich,
 *      srpScan() liefert false; wieder eingeschaltet erst nach dem nächsten estimateBearing() gültig;
 *      mit setSrpEvery(4) nur in jedem 4. Frame gültig (Peilung unverändert); Zeit je srpScan()
 * und misst die Zeit von estimateBearing() je Frame in beiden Pfaden.
 * Aufruf: build/test_host/t_gcc_direct
 */
#include <cstdio>
#include <cmath>
#include <chrono>
#include <random>
#include "chain.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }
static float rel(float a, float b) { return std::fabs(a - b) / std::fmax(std::fabs(b), 1e-6f); }

static Spectrum X, Y, sp[NUM_MICS];
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 fast, ref;

int main()
{
    auto& arr = Microphone_Array_114::instance();
    fast.init(arr); ref.init(arr); ref.setDirectMaxBins(0);

    // 1) synthetische Paare
    std::mt19937 rng(7); std::normal_distribution<float> nd(0.0f, 1.0f); std::uniform_real_distribution<float> ud(0.0f, 1.0f);
    int agree = 0, direct = 0, validN = 0; float dTau = 0, dPeak = 0, dRatio = 0;
    constexpr int TRIALS = 400;
    const float maxDelay = fast.maxIntraDelay();                      // wie estimateBearing()
    const float tauMax = 0.9f * maxDelay * SAMPLE_RATE_HZ;
    std::printf("Intra-Unit-Fenster ±%.1f Samples (Radius %.0f mm), Schnellpfad ±%u Lags\n",
                maxDelay * SAMPLE_RATE_HZ, 1e3f * MIC_RADIUS_M, SRP_MAX_LAG);
    for (int t = 0; t < TRIALS; ++t) {
        const float tau = tauMax * (2.0f * ud(rng) - 1.0f);
        for (uint32_t k = 0; k < SPECTRUM_BINS; ++k) {      // Port: Spectrum nur bis SPECTRUM_BINS
            const float a = nd(rng), b = nd(rng), ph = -2.0f * 3.14159265f * k * tau / N_FFT;
            X.re[k] = a; X.im[k] = b;
            Y.re[k] = a * std::cos(ph) - b * std::sin(ph) + 0.3f * nd(rng);
            Y.im[k] = a * std::sin(ph) + b * std::cos(ph) + 0.3f * nd(rng);
        }
        ComponentSelection sel;
        const int nb = 3 + static_cast<int>(ud(rng) * 22);            // 3 … 24 Bänder
        for (int i = 0; i < nb; ++i) {
            const uint32_t b = static_cast<uint32_t>(ud(rng) * NUM_BANDS) % NUM_BANDS;
            uint32_t k0, k1; Feature_Extraction_Module_122::bandBins(b, k0, k1);
            if (!sel.selected[k0]) ++sel.num_bands;
            for (uint32_t k = k0; k < k1; ++k) if (!sel.selected[k]) { sel.selected[k] = true; sel.weight[k] = 0.3f + 0.7f * ud(rng); ++sel.num_bins; }
        }
        TdoaMeasurement mf, mr;
        const bool vf = fast.crossCorrelate(X, Y, sel, maxDelay, mf);
        const bool vr = ref.crossCorrelate(X, Y, sel, maxDelay, mr);
        direct += fast.lastWasDirect();
        if (vf == vr) ++agree;
        if (vf && vr) {
            ++validN;
            dTau   = std::fmax(dTau, std::fabs(mf.tdoa_s - mr.tdoa_s) * SAMPLE_RATE_HZ);
            dPeak  = std::fmax(dPeak, rel(mf.peak, mr.peak));
            dRatio = std::fmax(dRatio, rel(mf.peak_ratio, mr.peak_ratio));
        }
    }
    std::printf("Paare: %d/%d Schnellpfad, Gültigkeit gleich %d/%d, gültig %d; max |Δτ| %.2e Samples, "
                "max rel. Δpeak %.2e, Δratio %.2e\n", direct, TRIALS, agree, TRIALS, validN, dTau, dPeak, dRatio);
    check(direct > TRIALS / 2, "Schnellpfad benutzt (<= DIRECT_MAX_BINS Bins)");
    check(agree == TRIALS, "Gültigkeit wie IFFT");
    check(dTau <= 1e-3f && dPeak <= 1e-3f && dRatio <= 1e-3f, "τ, Peak, Ratio wie IFFT");

    // 2) volle Kette
    auto& sim = Signal_Simulator::instance();
    double tFast = 0, tRef = 0, tSrp = 0; int srpN = 0; int frames = 0, same = 0, cmp = 0, directFrames = 0; float dAz = 0, dSrp = 0;
    FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing bf{}, br{};
    for (int az = 0; az < 360; az += 30) {
        SimParams p; p.scenario = SimScenario::DroneStatic; p.snr_db = 10.0f; p.azimuth_deg = static_cast<float>(az);
        sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init();
        for (int i = 0; i < 160; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp[REF_MIC], fv);
            for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
            ml.infer(feat.magnitude(), fv, st);
            if (i < 100) continue;
            fast.deriveSelection(st, sel);
            auto t0 = std::chrono::steady_clock::now();
            const bool okf = fast.estimateBearing(sp, sel, bf);
            auto t1 = std::chrono::steady_clock::now();
            const bool okr = ref.estimateBearing(sp, sel, br);
            auto t2 = std::chrono::steady_clock::now();
            tFast += std::chrono::duration<double>(t1 - t0).count(); tRef += std::chrono::duration<double>(t2 - t1).count();
            ++frames; directFrames += fast.lastWasDirect(); same += (okf == okr);
            if (okf && okr) {
                ++cmp; float d = std::fabs(bf.azimuth_deg - br.azimuth_deg); if (d > 180) d = 360 - d; dAz = std::fmax(dAz, d);
                float af, pf, rf, ar, pr, rr;
                auto s0 = std::chrono::steady_clock::now();
                const bool sf = fast.srpScan(af, pf, rf);
                tSrp += std::chrono::duration<double>(std::chrono::steady_clock::now() - s0).count(); ++srpN;
                if (sf && ref.srpScan(ar, pr, rr)) { float e = std::fabs(af - ar); if (e > 180) e = 360 - e; dSrp = std::fmax(dSrp, e); }
            }
        }
    }
    std::printf("Kette: %d Frames, Schnellpfad %d, Gültigkeit gleich %d, max |Δaz| %.4f° (SRP %.4f°); "
                "estimateBearing je Frame: Schnellpfad %.3f ms, IFFT %.3f ms (x86)\n",
                frames, directFrames, same, dAz, dSrp, 1e3 * tFast / frames, 1e3 * tRef / frames);
    std::printf("srpScan je Aufruf: %.4f ms (x86, %d Aufrufe)\n", srpN ? 1e3 * tSrp / srpN : 0.0, srpN);
    check(directFrames == frames, "Kette: Schnellpfad in jedem Frame");
    check(same == frames && cmp > 0 && dAz <= 0.01f && dSrp <= 0.01f, "Kette: Peilung und SRP wie IFFT");

    // 3) SRP-Referenzscan aus: gleiche Peilung, kein Scan
    Correlation_Processing_Module_126 off; off.init(arr);
    Bearing bOn{}, bOff{}; float a, pw, r;
    const bool okOn = fast.estimateBearing(sp, sel, bOn);
    off.setSrpReference(false);
    const bool okOff = off.estimateBearing(sp, sel, bOff);
    const bool scanOff = off.srpScan(a, pw, r);
    off.setSrpReference(true);
    const bool scanStale = off.srpScan(a, pw, r);
    off.estimateBearing(sp, sel, bOff);
    const bool scanAgain = off.srpScan(a, pw, r);
    std::printf("SRP aus: Peilung %.4f° / %.4f° (ein), srpScan aus %d, nach Einschalten %d, nach Peilung %d\n",
                bOff.azimuth_deg, bOn.azimuth_deg, scanOff, scanStale, scanAgain);
    check(okOn == okOff && bOn.azimuth_deg == bOff.azimuth_deg, "SRP aus: Peilung bitgleich");
    check(!scanOff && !scanStale && scanAgain, "SRP aus: srpScan() erst nach neuer Peilung wieder gültig");

    // Referenzscan nur jeden 4. Frame (120: SRP_EVERY_N): gültig in Frame 1, 5, 9; Peilung bitgleich
    off.setSrpEvery(4);
    int validMask = 0; bool sameBearing = true; float azN = 0, pwN = 0, rN = 0, az1 = 0, pw1 = 0, r1 = 0;
    for (int k = 0; k < 9; ++k) {
        Bearing bk{};
        off.estimateBearing(sp, sel, bk);
        sameBearing = sameBearing && bk.azimuth_deg == bOn.azimuth_deg;
        if (off.srpScan(azN, pwN, rN)) validMask |= 1 << k;
    }
    fast.srpScan(az1, pw1, r1);
    std::printf("SRP jeden 4. Frame: gültig in Frames (Bitmaske) 0x%03X, Azimut %.4f° / %.4f° (jeder Frame)\n",
                validMask, azN, az1);
    check(validMask == 0x111 && sameBearing && azN == az1, "SRP jeden 4. Frame: Scan in Frame 1, 5, 9, Peilung unverändert");
    return g_fail;
}
