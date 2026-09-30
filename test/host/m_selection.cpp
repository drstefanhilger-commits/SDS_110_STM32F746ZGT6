/*
 * m_selection – Band-Selektion, Detektion und Reports
 *
 * Bezug: Befund 24
 * Prüft/misst: detected, HBD, selektierte Bänder, Anteil Bänder mit Harmonischer, Peilung, Reports (Peilung gültig und detected)
 * Aufruf: build/test_host/m_selection [Frames in 64-ms-Einheiten] [noise = nur Rauschszenarien]
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdlib>
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
static const char* NAMES[] = { "DroneSweep", "DroneStatic", "SingleTone", "WindNoise", "Silence" };
static float wrap(float e) { while (e > 180) e -= 360; while (e < -180) e += 360; return e; }
static int g_frames = 260;
static void run(SimScenario sc, float snr)
{
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    int n = 0, det = 0, hbd = 0, rep = 0, bv = 0; double nb = 0, hit = 0, hitN = 0; std::vector<float> err;
    for (int az = 0; az < 360; az += 60) {
        SimParams p; p.scenario = sc; p.snr_db = snr; p.azimuth_deg = az; sim.init(p); g_fa.reset();
        pre.init(); feat.init(); ml.init(); corr.init(arr);
        FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
        for (int i = 0; i < g_frames; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp[REF_MIC], fv);
            for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
            ml.infer(feat.magnitude(), fv, st); corr.deriveSelection(st, sel); corr.estimateBearing(sp, sel, br);
            if (i < 100) continue;          // 50 alte 64-ms-Frames = 100 Frames à 32 ms
            ++n; hbd += ml.hbd().droneDetected;
            uint32_t k = 0; for (uint32_t b = 0; b < NUM_BANDS; ++b) if (st.p[b] > THETA_SEL) {
                ++k; const float lo = BAND_LO_HZ + b * BAND_WIDTH_HZ, hi = lo + BAND_WIDTH_HZ; bool h = false;
                for (int q = 1; q <= p.harmonics; ++q) { const float fq = p.f0_hz * q; if (fq >= lo - 10 && fq < hi + 10) h = true; }
                hit += h; hitN += 1; }
            const bool d = k >= B_MIN; det += d; nb += k;
            if (br.valid) { ++bv; if (d) { ++rep; err.push_back(std::fabs(wrap(br.azimuth_deg - sim.trueAzimuth()))); } }
        }
    }
    std::sort(err.begin(), err.end());
    const bool drone = sc == SimScenario::DroneSweep || sc == SimScenario::DroneStatic;
    std::printf("%-11s %5.1f dB | detected %3d%% | HBD %5.1f%% | Bänder %4.1f | Harmonische %s | Peilung %3d%% | Report %3d%% | |Δaz| Med %5.2f°\n",
                NAMES[(int)sc], snr, 100 * det / n, 100.0 * hbd / n, nb / n,
                drone ? (hitN ? (std::to_string((int)(100 * hit / hitN)) + "%").c_str() : "  - ") : "  - ",
                100 * bv / n, 100 * rep / n, err.empty() ? NAN : err[err.size() / 2]);
}
int main(int argc, char** argv)
{
    mlModeFromEnv(ml);
    if (argc > 1) g_frames = 2 * atoi(argv[1]);   // Angabe in alten 64-ms-Frames
    if (argc > 2) { for (int s = 2; s < 5; ++s) run((SimScenario)s, 20.f); return 0; }
    for (int s = 0; s < 5; ++s) run((SimScenario)s, 20.f);
    for (float snr : { 10.f, 3.f, 0.f }) run(SimScenario::DroneStatic, snr);
}
