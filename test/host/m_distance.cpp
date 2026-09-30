/*
 * m_distance – Distanzschätzung aus dem Pegel
 *
 * Bezug: Befund 9 (und 17)
 * Prüft/misst: Simulator-Pegel ∝ 1/r, 10..200 m: levelA vor NS/AGC, r = K/A, Verhältnis r_geschätzt/r_wahr (soll konstant sein)
 * Aufruf: build/test_host/m_distance
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include <algorithm>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "Sensor_Unit_112/Pre_Processor_118.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml; static Spectrum s0;
int main()
{
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    std::printf("  r_wahr | levelA (Median) | r_geschätzt = K/A | r_gesch./r_wahr\n");
    for (float r : { 10.f, 20.f, 50.f, 100.f, 200.f }) {
        SimParams p; p.scenario = SimScenario::DroneStatic; p.distance_m = r; p.snr_db = 20; sim.init(p); g_fa.reset();
        pre.init(); feat.init(); ml.init();
        FeatureVector fv{}; AcousticState st{}; float lv[400]; int n = 0;
        for (int i = 0; i < 240; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, s0, fv);
            ml.infer(feat.magnitude(), fv, st);
            if (i < 120) continue;
            float a = 0; for (uint32_t b = 0; b < NUM_BANDS; ++b) a += std::exp(fv.band_log_power[b]) * st.p[b];
            a = std::sqrt(a) / pre.frameCenterGain(REF_MIC);   // wie Processing_Module_120
            lv[n++] = a;
        }
        std::sort(lv, lv + n); const float A = lv[n / 2]; const float rEst = LEVEL_DIST_K_REF / (A + LEVEL_DIST_EPS);
        std::printf("  %6.0f | %15.4g | %17.4g | %8.4g\n", r, A, rEst, rEst / r);
    }
}
