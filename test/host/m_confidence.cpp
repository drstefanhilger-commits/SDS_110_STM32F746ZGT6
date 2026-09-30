/*
 * m_confidence – Konfidenz und Peilresiduum
 *
 * Bezug: Befund 18
 * Prüft/misst: Verteilung von candidateConfidence() und Residuum (Samples) für Drohne 30..-3 dB, Wind, Stille, Einzelton
 * Aufruf: build/test_host/m_confidence
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "ml_mode.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
#include "Processing_Module_120/Localisation_Module_128/Localisation_Module_128.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr; static Localisation_Module_128 loc; static Spectrum sp[NUM_MICS];
static float wrap(float e) { while (e > 180) e -= 360; while (e < -180) e += 360; return e; }
static void run(SimScenario sc, float snr, const char* name)
{
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    std::vector<float> res, pairs, err, conf;
    for (int az = 0; az < 360; az += 60) {
        SimParams p; p.scenario = sc; p.snr_db = snr; p.azimuth_deg = az; sim.init(p); g_fa.reset();
        pre.init(); feat.init(); ml.init(); corr.init(arr); { const Vec3 o{0,0,0}; loc.init(&o, 1); }
        FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
        for (int i = 0; i < 200; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp[REF_MIC], fv);
            for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
            ml.infer(feat.magnitude(), fv, st); corr.deriveSelection(st, sel); corr.estimateBearing(sp, sel, br);
            if (i < 100 || !br.valid) continue;
            CandidateLocation cl{}; loc.fromBearing(br, 1.0f, cl); conf.push_back(cl.confidence);
            res.push_back(br.residual * SAMPLE_RATE_HZ); pairs.push_back(br.valid_pairs);
            err.push_back(std::fabs(wrap(br.azimuth_deg - sim.trueAzimuth())));
        }
    }
    auto q = [](std::vector<float> v, double x) -> double { if (v.empty()) return NAN; std::sort(v.begin(), v.end()); return v[(size_t)(x * (v.size() - 1))]; };
    std::printf("%-10s %5.1f dB | Konfidenz 5%% %.2f 50%% %.2f 95%% %.2f | Residuum 50%% %.2f Samples | |Δaz| 50%% %.2f° 95%% %.2f°\n",
                name, snr, q(conf, .05), q(conf, .5), q(conf, .95), q(res, .5), q(err, .5), q(err, .95));
}
int main()
{
    mlModeFromEnv(ml);
    for (float s : { 30.f, 20.f, 10.f, 3.f, 0.f, -3.f }) run(SimScenario::DroneStatic, s, "Drohne");
    run(SimScenario::WindNoise, 20.f, "Wind"); run(SimScenario::Silence, 20.f, "Stille"); run(SimScenario::SingleTone, 20.f, "Einzelton");
}
