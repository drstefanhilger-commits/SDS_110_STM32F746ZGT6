/*
 * t_ml124 – MLP-Inferenz in Modul 124 (AP6, ML_Test docs/Trainingskonzept_ML124.md Abschnitt 8)
 *
 * Prüft:
 *   1. Merkmalsversion des Modells = Merkmalsversion dieses Code-Stands (beim Übersetzen)
 *   2. C++-Inferenz gegen die Referenzvektoren aus ML_Test (Keras), max. Abweichung ≤ 1e-5
 *   3. Kontextstapel in infer(): MLP-Eingang = Merkmale t, t−1, … (neuester zuerst, am Anfang
 *      mit dem ersten Frame aufgefüllt, wie stack_context() in ML_Test) – bitgenau
 *   4. Stufen: Shadow liefert bitgenau dasselbe s(t) wie Hbd; Ml liefert das geglättete,
 *      gegatete MLP; Schatten-Statistik zählt alle Frames
 * Aufruf: build/test_host/t_ml124
 */
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include "chain.hpp"
#include "ML124_Model_Ref.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
using namespace sds110;
namespace mm = ml124model;

#ifndef SDS_FEATURE_VERSION
#error "SDS_FEATURE_VERSION fehlt (Makefile)"
#endif
// STM32F746ZGT6-Port: Die Merkmalskette speichert Hops als 16-Bit-Blockgleitkomma (kein SDRAM),
// ihr Quelltext-Hash weicht deshalb von der Version ab, mit der das Modell (Repo SDS_110,
// Discovery-Board) trainiert wurde. Die Merkmale selbst ändern sich nur um die Quantisierung
// (< 2^-15 des Blockmaximums). Statt des Abbruchs beim Übersetzen: Hinweis zur Laufzeit.
// Vor dem Einsatz der Stufe Ml (ML124_MODE) mit Merkmalen dieser Kette neu trainieren.
static bool strEq(const char* a, const char* b) { return *a == *b && (*a == 0 || strEq(a + 1, b + 1)); }

static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat;
static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

static void flatten(const FeatureVector& f, float* c)
{
    std::memcpy(c, f.band_log_power, sizeof(f.band_log_power)); c += NUM_BANDS;
    std::memcpy(c, f.mel, sizeof(f.mel));                       c += MEL_BANDS;
    *c++ = f.spectral_flux;
    std::memcpy(c, f.band_am_depth, sizeof(f.band_am_depth));
}

int main()
{
    std::printf("Merkmalsversion Code %s, Modell %s%s\n", SDS_FEATURE_VERSION, mm::MODEL_FEATURE_VERSION,
                strEq(mm::MODEL_FEATURE_VERSION, SDS_FEATURE_VERSION) ? "" :
                " (HINWEIS: abweichend – Blockgleitkomma-Hops des Board-Ports; vor Stufe Ml neu trainieren)");
    // 2) Referenzvektoren
    float x[mm::NUM_INPUTS], y[NUM_BANDS], err = 0.0f;
    for (uint32_t r = 0; r < mm::REF_N; ++r) {
        std::memcpy(x, mm::REF_X + r * mm::NUM_INPUTS, sizeof(x));
        Machine_Learning_Module_124::mlpForward(x, y);
        for (uint32_t b = 0; b < NUM_BANDS; ++b) err = std::fmax(err, std::fabs(y[b] - mm::REF_Y[r * NUM_BANDS + b]));
    }
    std::printf("Modell %s, %u Referenzvektoren, max |C++ − Keras| = %.2e\n", mm::MODEL_NAME, mm::REF_N, err);
    check(err <= 1e-5f, "Referenzvektoren <= 1e-5");

    // 3) + 4) Signalkette, drei Instanzen in den drei Stufen parallel
    static Machine_Learning_Module_124 mh, ms, mml;
    mh.setMode(Ml124Mode::Hbd); ms.setMode(Ml124Mode::Shadow); mml.setMode(Ml124Mode::Ml);
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    SimParams p; p.scenario = SimScenario::DroneStatic; p.snr_db = 10.0f; sim.init(p); g_fa.reset();
    pre.init(); feat.init(); mh.init(); ms.init(); mml.init();
    constexpr int N = 200;
    const uint32_t F = Machine_Learning_Module_124::ML_FEATURES;
    std::vector<float> hist;                      // alle Merkmale, Frame für Frame
    FeatureVector fv{}; AcousticState sh{}, ss{}, sm{};
    bool stackOk = true, shadowOk = true, finite = true; int det = 0;
    for (int i = 0; i < N; ++i) {
        const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);
        static Spectrum sp; feat.process(f, sp, fv);
        mh.infer(feat.magnitude(), fv, sh); ms.infer(feat.magnitude(), fv, ss); mml.infer(feat.magnitude(), fv, sm);
        hist.resize((i + 1) * F); flatten(fv, &hist[i * F]);
        for (uint32_t d = 0; d < mm::CONTEXT; ++d) {
            const int src = (i - static_cast<int>(d) < 0) ? 0 : i - static_cast<int>(d);
            std::memcpy(x + d * F, &hist[src * F], F * sizeof(float));
        }
        Machine_Learning_Module_124::mlpForward(x, y);
        if (std::memcmp(y, mml.mlRaw(), sizeof(y)) != 0) stackOk = false;
        if (std::memcmp(sh.p, ss.p, sizeof(sh.p)) != 0) shadowOk = false;
        uint32_t k = 0;
        for (uint32_t b = 0; b < NUM_BANDS; ++b) { finite &= std::isfinite(sm.p[b]) && sm.p[b] >= 0 && sm.p[b] <= 1; k += sm.p[b] > THETA_SEL; }
        det += (i >= 100 && k >= B_MIN);
    }
    check(stackOk, "Kontextstapel in infer() = stack_context (bitgenau)");
    check(shadowOk, "Shadow: s(t) bitgenau wie Hbd");
    check(ms.shadow().frames == N, "Shadow: alle Frames gezählt");
    check(finite, "Ml: s(t) endlich, in [0, 1]");
    const auto& st = ms.shadow();
    std::printf("DroneStatic 10 dB: ML detected %d%% (ab Frame 100) | Schatten: Detektion gleich %.0f%%, "
                "Band-Überlappung %.2f, mittl. |Δp| %.3f\n", det, 100 * st.detectAgreement(), st.bandOverlap(), st.meanAbsDiff());
    check(det > 0, "Ml: Drohne wird detektiert");
    return g_fail;
}
