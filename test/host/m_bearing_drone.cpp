/*
 * m_bearing_drone – Peilung eines Drohnensignals über die volle Kette
 *
 * Bezug: Befunde 8, 17, 23, 24
 * Prüft/misst: 118->122->124->126, 12 Richtungen, SNR 30..0 dB: gültige Peilungen, Paare, Peak-Ratio, Fehler TDOA-LS und SRP
 * Aufruf: build/test_host/m_bearing_drone [Szenario: 1 = DroneStatic (Standard), 0 = DroneSweep]
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "ml_mode.hpp"
#include "Sensor_Unit_112/Pre_Processor_118.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr; static Spectrum sp[NUM_MICS];
static float wrap(float e) { while (e > 180) e -= 360; while (e < -180) e += 360; return e; }
int main(int argc, char** argv)
{
    mlModeFromEnv(ml);
    const int scen = argc > 1 ? atoi(argv[1]) : 1;
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    for (float snr : { 30.f, 20.f, 10.f, 6.f, 3.f, 0.f }) {
        int frames = 0, valid = 0, srpOkN = 0; double pairs = 0, ratioMed = 0; std::vector<float> err, srpErr;
        for (int az = 0; az < 360; az += 30) {
            SimParams p; p.scenario = (SimScenario)scen; p.azimuth_deg = az; p.snr_db = snr;
            sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init(); corr.init(arr);
            FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
            for (int i = 0; i < 120; ++i) {
                const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);
                feat.process(f, sp[REF_MIC], fv);
                for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
                ml.infer(feat.magnitude(), fv, st); corr.deriveSelection(st, sel); corr.estimateBearing(sp, sel, br);
                float sa, spw, sr; const bool so = corr.srpScan(sa, spw, sr);
                if (i < 40) continue;
                ++frames; pairs += br.valid_pairs;
                std::vector<float> r; for (uint32_t k = 0; k < NUM_MIC_PAIRS; ++k) r.push_back(corr.lastPairTdoa()[k].peak_ratio);
                std::nth_element(r.begin(), r.begin() + r.size() / 2, r.end()); ratioMed += r[r.size() / 2];
                const float tru = sim.trueAzimuth();
                if (br.valid) { ++valid; err.push_back(std::fabs(wrap(br.azimuth_deg - tru))); }
                if (so) { ++srpOkN; srpErr.push_back(std::fabs(wrap(sa - tru))); }
            }
        }
        auto q = [](std::vector<float> v, double x) -> double { if (v.empty()) return NAN; std::sort(v.begin(), v.end()); return (double)v[(size_t)(x * (v.size() - 1))]; };
        std::printf("SNR %5.1f dB | Peilung gültig %3d%% | Paare %4.1f/28 | Ratio-Median %5.2f | |Δaz| Median %6.2f° 95%% %6.2f° | SRP |Δaz| Median %6.2f° 95%% %6.2f°\n",
                    snr, 100 * valid / frames, pairs / frames, ratioMed / frames, q(err, .5), q(err, .95), q(srpErr, .5), q(srpErr, .95));
    }
}
