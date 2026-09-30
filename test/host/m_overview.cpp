/*
 * m_overview – Übersicht aller Simulator-Szenarien
 *
 * Bezug: Befunde 7, 8, 23, 24
 * Prüft/misst: je Szenario: HBD-DRONE-Anteil, detected, Bänder, f0, Score, Floor, Peilung; Empfindlichkeit DroneStatic 30..-6 dB
 * Aufruf: build/test_host/m_overview [Frames in 64-ms-Einheiten, Standard 120]
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cstdlib>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "ml_mode.hpp"
#include "Sensor_Unit_112/Pre_Processor_118.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"

using namespace sds110;
static Pre_Processor_118 pre;
static Feature_Extraction_Module_122 feat;
static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr;
static Spectrum spectra[NUM_MICS];

static const char* NAMES[] = { "DroneSweep", "DroneStatic", "SingleTone", "WindNoise", "Silence" };

static void run(SimScenario sc, float snrDb, float f0, int frames, int settle)
{
    auto& sim = Signal_Simulator::instance();
    auto& arr = Microphone_Array_114::instance();
    SimParams sp; sp.scenario = sc; sp.snr_db = snrDb; sp.f0_hz = f0;
    sim.init(sp); g_fa.reset(); pre.init(); feat.init(); ml.init(); corr.init(arr);

    int nHbd = 0, nSel = 0, nBear = 0, n = 0; double selBands = 0, azErr = 0, f0Sum = 0, score = 0, flr = 0;
    FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
    for (int i = 0; i < frames; ++i) {
        const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);
        feat.process(f, spectra[REF_MIC], fv);
        for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, spectra[m]);
        ml.infer(feat.magnitude(), fv, st);
        corr.deriveSelection(st, sel);
        corr.estimateBearing(spectra, sel, br);
        if (i < settle) continue;
        ++n;
        const auto& h = ml.hbd();
        uint32_t nb = 0; for (uint32_t b = 0; b < NUM_BANDS; ++b) if (st.p[b] > THETA_SEL) ++nb;
        nHbd += h.droneDetected; nSel += (nb >= B_MIN); selBands += nb; f0Sum += h.f0Hz; score += h.score;
        double fl = 0; for (uint32_t k = 0; k < NUM_BINS; ++k) fl += h.noiseFloorDb[k]; flr += fl / NUM_BINS;
        if (br.valid) { ++nBear; float e = br.azimuth_deg - sim.trueAzimuth(); while (e > 180) e -= 360; while (e < -180) e += 360; azErr += std::fabs(e); }
    }
    std::printf("%-11s SNR %5.1f dB | HBD DRONE %3d%% | sel>=B_MIN %3d%% | Bänder %4.1f | f0 %6.1f Hz | Score %6.2f | Floor %7.1f dB | Peilung %3d%% | |Δaz| %5.1f°\n",
                NAMES[(int)sc], snrDb, 100 * nHbd / n, 100 * nSel / n, selBands / n, f0Sum / n, score / n, flr / n,
                100 * nBear / n, nBear ? azErr / nBear : 0.0);
}

int main(int argc, char** argv)
{
    mlModeFromEnv(ml);
    const int frames = 2 * (argc > 1 ? std::atoi(argv[1]) : 120), settle = 60;   // Angaben in alten 64-ms-Frames
    for (int s = 0; s < 5; ++s) run((SimScenario)s, SIM_SNR_DB, SIM_F0_HZ, frames, settle);
    std::printf("-- Empfindlichkeit DroneStatic\n");
    for (float snr : { 30.f, 20.f, 10.f, 6.f, 3.f, 0.f, -3.f, -6.f }) run(SimScenario::DroneStatic, snr, SIM_F0_HZ, frames, settle);
    return 0;
}
