/*
 * m_longrun – Langzeitverhalten des HBD-Rauschbodens
 *
 * Bezug: Befund 7 (und 17)
 * Prüft/misst: DRONE-Anteil je 10 s über ~5 min bei stehender Drohne (wann wird der Ton als Rauschen gelernt?)
 * Aufruf: build/test_host/m_longrun [maxRiseDbPerFrame, -1 = Projektwert] [SNR dB]  (Laufzeit einige Minuten)
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cstdlib>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "Sensor_Unit_112/Pre_Processor_118.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml; static Spectrum sp0;
int main(int argc, char** argv)
{
    const float rise = atof(argv[1]); const float snr = argc > 2 ? atof(argv[2]) : 20.f;
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    SimParams p; p.scenario = SimScenario::DroneStatic; p.snr_db = snr; sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init();
    if (rise >= 0) ml.params().noiseFloor.maxRiseDbPerFrame = rise;
    FeatureVector fv{}; AcousticState st{};
    std::printf("rise %.3f dB/Frame, SNR %.0f dB – DRONE-Anteil je 10 s:", rise, snr);
    int det = 0;
    for (int i = 1; i <= 312 * 30; ++i) {   // ~5 min bei 31,25 Frames/s
        const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp0, fv);
        ml.infer(feat.magnitude(), fv, st); det += ml.hbd().droneDetected;
        if (i % 312 == 0) { std::printf(" %d", 100 * det / 312); det = 0; }
    }
    std::printf("\n");
}
