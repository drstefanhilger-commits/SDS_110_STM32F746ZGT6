/*
 * t_bearing_f0 – Mehrdeutigkeit der TDOA-LS-Peilung bei hohem f0 (Befund 26)
 *
 * Prüft (126 estimateBearing: robuste LS, SRP-geführte Neuwahl, sonst ungültig):
 *   DroneStatic, SNR 20 dB, 12 Richtungen, f0 = 180, 480, 1000 Hz, Frames 40…119
 *   - höchstens 1 % grobe Fehler (> 30°) unter den gültigen Peilungen
 *   - 95-%-Fehler ≤ 10°, gültig ≥ 70 %
 *   vorher (27.09.2026): 480 Hz 95 % 20,6°, max 168°; 1000 Hz 95 % 137,5°, gültig 66 %
 * Aufruf: build/test_host/t_bearing_f0
 */
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>
#include "chain.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr; static Spectrum sp[NUM_MICS];
static float wrap(float e) { while (e > 180) e -= 360; while (e < -180) e += 360; return e; }

int main()
{
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    for (float f0 : { 180.f, 480.f, 1000.f }) {
        int frames = 0, valid = 0, guided = 0; std::vector<float> err;
        for (int az = 0; az < 360; az += 30) {
            SimParams p; p.scenario = SimScenario::DroneStatic; p.azimuth_deg = static_cast<float>(az);
            p.snr_db = 20.0f; p.f0_hz = f0;
            sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init(); corr.init(arr);
            FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
            for (int i = 0; i < 120; ++i) {
                const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);
                feat.process(f, sp[REF_MIC], fv);
                for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
                ml.infer(feat.magnitude(), fv, st); corr.deriveSelection(st, sel); corr.estimateBearing(sp, sel, br);
                if (i < 40) continue;
                ++frames; guided += corr.lastWasGuided();
                if (br.valid) { ++valid; err.push_back(std::fabs(wrap(br.azimuth_deg - p.azimuth_deg))); }
            }
        }
        std::sort(err.begin(), err.end());
        const float p95 = err.empty() ? 999.0f : err[static_cast<size_t>(0.95 * (err.size() - 1))];
        const long gross = std::count_if(err.begin(), err.end(), [](float e) { return e > 30.0f; });
        std::printf("f0 %4.0f Hz: gültig %d/%d, SRP-geführt %d, 95 %% %.2f°, grob %ld\n", f0, valid, frames, guided, p95, gross);
        char what[96];
        std::snprintf(what, sizeof(what), "f0 %.0f Hz: ≤ 1 %% grobe Fehler, 95 %% ≤ 10°, gültig ≥ 70 %%", f0);
        check(gross * 100 <= static_cast<long>(err.size()) && p95 <= 10.0f && valid * 10 >= frames * 7, what);
    }
    return g_fail;
}
