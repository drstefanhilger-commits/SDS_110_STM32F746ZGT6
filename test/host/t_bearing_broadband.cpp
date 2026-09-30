/*
 * t_bearing_broadband – Peilung einer breitbandigen Quelle
 *
 * Bezug: Befund 23 (Vorzeichen), Befund 8
 * Prüft/misst: TDOA-LS-Peilung und SRP-Scan über 24 Richtungen (0..345°), alle Bänder selektiert; Kriterium: alle gültig, max. Fehler < 0,5°
 * Aufruf: make check  bzw.  build/test_host/t_bearing_broadband
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include "Harness/Signal_Simulator.hpp"
#include "chain.hpp"
#include "Sensor_Unit_112/Pre_Processor_118.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Correlation_Processing_Module_126 corr; static Spectrum sp[NUM_MICS];
static float wrap(float e) { while (e > 180) e -= 360; while (e < -180) e += 360; return e; }
int main()
{
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    AcousticState st{}; for (auto& v : st.p) v = 1.0f;
    ComponentSelection sel{}; Bearing br{}; FeatureVector fv{};
    float maxLs = 0, maxSrp = 0; int nLs = 0, nSrp = 0, total = 0;
    std::printf("  wahr |  TDOA-LS  Fehler Paare |   SRP    Fehler\n");
    for (int az = 0; az < 360; az += 15) {
        SimParams p; p.scenario = SimScenario::WindNoise; p.azimuth_deg = az; p.snr_db = 20;
        sim.init(p); g_fa.reset(); pre.init(); feat.init(); corr.init(arr); corr.deriveSelection(st, sel);
        for (int i = 0; i < 5; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);
            feat.process(f, sp[REF_MIC], fv);
            for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
        }
        ++total;
        corr.estimateBearing(sp, sel, br);
        float srpAz = 0, srpP = 0, srpR = 0; const bool srpOk = corr.srpScan(srpAz, srpP, srpR);
        const float eL = br.valid ? wrap(br.azimuth_deg - az) : NAN, eS = srpOk ? wrap(srpAz - az) : NAN;
        if (br.valid) { ++nLs; if (std::fabs(eL) > maxLs) maxLs = std::fabs(eL); }
        if (srpOk) { ++nSrp; if (std::fabs(eS) > maxSrp) maxSrp = std::fabs(eS); }
        std::printf("  %4d | %7.1f  %6.1f  %3d  | %6.1f  %6.1f\n", az, br.azimuth_deg, eL, br.valid_pairs, srpAz, eS);
    }
    std::printf("gültig: TDOA-LS %d/%d, max |Fehler| %.2f° | SRP %d/%d, max |Fehler| %.2f°\n", nLs, total, maxLs, nSrp, total, maxSrp);
    const bool ok = nLs == total && nSrp == total && maxLs < 0.5f && maxSrp < 0.5f;   // Kriterium: alle gültig, < 0,5°
    std::printf("%s\n", ok ? "OK" : "FEHLER");
    return ok ? 0 : 1;
}
