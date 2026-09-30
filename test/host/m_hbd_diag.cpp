/*
 * m_hbd_diag – Diagnose des HBD-Rauschbodens und der Harmonischen-SNR
 *
 * Bezug: Befund 7
 * Prüft/misst: Verteilung (magDb − noiseFloorDb) im Bereich 100..4000 Hz und SNR je Harmonischer
 *              für ein Szenario (sollte bei reinem Rauschen um 0 dB liegen, nicht bei +70 dB)
 * Aufruf: build/test_host/m_hbd_diag [Szenario 0..4, Standard 4 = Silence]
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml; static Spectrum sp0;
int main(int argc, char** argv)
{
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    SimParams p; p.scenario = static_cast<SimScenario>(argc > 1 ? std::atoi(argv[1]) : 4);
    sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init();
    FeatureVector fv{}; AcousticState st{};
    std::vector<float> d;                                        // magDb − floor, 100..4000 Hz
    const uint32_t k0 = static_cast<uint32_t>(100.0f / Feature_Extraction_Module_122::binHz(1));
    const uint32_t k1 = static_cast<uint32_t>(4000.0f / Feature_Extraction_Module_122::binHz(1));
    for (int i = 0; i < 300; ++i) {                              // ~9,6 s bei 32 ms je Frame
        const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp0, fv);
        ml.infer(feat.magnitude(), fv, st);
        if (i < 100) continue;                                   // Anlauf (AGC, Floor, HBD-Warmup)
        const auto& h = ml.hbd();
        for (uint32_t k = k0; k < k1; ++k) d.push_back(h.magDb[k] - h.noiseFloorDb[k]);
        if (i % 50 == 0) {
            std::printf("Frame %3d f0 %.1f Hz, SNR-Mittel %.1f dB, konsistent %d, DRONE %d | SNR je Harmonischer:",
                        i, h.f0Hz, h.globalSnrAvgDb, h.consistentBands, h.droneDetected);
            for (int k = 0; k < 8; ++k) std::printf(" %.1f", h.lastBandSnr[k]);
            std::printf("\n");
        }
    }
    std::sort(d.begin(), d.end());
    auto q = [&](double x) { return d[static_cast<size_t>(x * (d.size() - 1))]; };
    std::printf("magDb − Floor [dB]: 5%% %.1f  25%% %.1f  50%% %.1f  75%% %.1f  95%% %.1f  99%% %.1f\n",
                q(.05), q(.25), q(.5), q(.75), q(.95), q(.99));
    return 0;
}
