/*
 * m_bearing_f0 – Peilung über der Grundfrequenz f0 (Befund 26)
 *
 * Prüft/misst: DroneStatic, 12 Richtungen, SNR 20 dB, f0 = 120 … 1000 Hz: gültige Peilungen,
 * Median-, 95-%- und Größtfehler, Zahl grober Fehler (> 30°) von TDOA-LS; SRP. Befund 26 (400-mm-Array, Fenster ±61): TDOA-LS
 * mehrdeutig ab ~400 Hz (bei 650 Hz Median 105°).
 * Aufruf: build/test_host/m_bearing_f0 [SNR dB]
 */
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include "chain.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr; static Spectrum sp[NUM_MICS];
static float wrap(float e) { while (e > 180) e -= 360; while (e < -180) e += 360; return e; }

int main(int argc, char** argv)
{
    const float snr = argc > 1 ? static_cast<float>(atof(argv[1])) : 20.0f;
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    std::printf("DroneStatic, SNR %.0f dB, 12 Richtungen, Frames 40…119\n", snr);
    for (float f0 : { 120.f, 180.f, 250.f, 320.f, 400.f, 480.f, 560.f, 650.f, 800.f, 1000.f }) {
        int frames = 0, valid = 0, srpN = 0; std::vector<float> err, srpErr;
        for (int az = 0; az < 360; az += 30) {
            SimParams p; p.scenario = SimScenario::DroneStatic; p.azimuth_deg = static_cast<float>(az);
            p.snr_db = snr; p.f0_hz = f0;
            sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init(); corr.init(arr);
            FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
            for (int i = 0; i < 120; ++i) {
                const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);
                feat.process(f, sp[REF_MIC], fv);
                for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
                ml.infer(feat.magnitude(), fv, st); corr.deriveSelection(st, sel); corr.estimateBearing(sp, sel, br);
                float sa, spw, sr; const bool so = corr.srpScan(sa, spw, sr);
                if (i < 40) continue;
                ++frames;
                if (br.valid) { ++valid; err.push_back(std::fabs(wrap(br.azimuth_deg - p.azimuth_deg))); }
                if (so) { ++srpN; srpErr.push_back(std::fabs(wrap(sa - p.azimuth_deg))); }
            }
        }
        auto q = [](std::vector<float> v, double x) -> double { if (v.empty()) return NAN; std::sort(v.begin(), v.end()); return (double)v[(size_t)(x * (v.size() - 1))]; };
        const long gross = std::count_if(err.begin(), err.end(), [](float e) { return e > 30.0f; });
        std::printf("f0 %5.0f Hz | gültig %3d%% | TDOA-LS |Δaz| Median %6.2f° 95%% %6.2f° max %6.1f° grob(>30°) %3ld | SRP Median %6.2f° 95%% %6.2f°\n",
                    f0, 100 * valid / frames, q(err, .5), q(err, .95), q(err, 1.0), gross, q(srpErr, .5), q(srpErr, .95));
    }
    return 0;
}
