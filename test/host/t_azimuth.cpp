/*
 * t_azimuth – Azimut-Konvention (FSL9 §6, FIG. 5; A28): 0° = Nord, im Uhrzeigersinn, Mikrofon 0 = Nord
 *
 * Prüft (Infrastructure/Utils/Azimuth.hpp):
 *   1. Umrechnung Array-System <-> Azimut, wrap360, diff (359° / 1° -> 2°)
 *   2. Geometrie: Quelle bei 0° (Nord) liegt in Richtung Mikrofon 0; bei 90° (Ost) in Richtung
 *      Mikrofon 6, bei 270° (West) Mikrofon 2 (Nummerierung gegen den Uhrzeigersinn, von oben)
 *   3. volle Kette (DroneStatic 30 dB): Quelle bei 0, 45, 90, …, 315° -> TDOA-LS- und SRP-Peilung
 *      liefern denselben Azimut (Median-Fehler ≤ 1°)
 *   4. Nordabgleich (USB Id 9): Offset i32 in 0,01° (±180,00°), 128 addiert ihn auf die Peilung
 *      (350° + 15° -> 5°, 10° − 20° -> 350°); außerhalb des Bereichs verworfen
 * Aufruf: build/test_host/t_azimuth
 */
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "chain.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
#include "Processing_Module_120/Localisation_Module_128/Localisation_Module_128.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

static Spectrum sp[NUM_MICS];
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr;

// Mikrofon, das am weitesten in Richtung des Azimuts liegt
static int micToward(float az)
{
    auto& arr = Microphone_Array_114::instance();
    float ux, uy; Azimuth::toArray(az, ux, uy);
    int best = 0; float bd = -1e9f;
    for (uint32_t m = 0; m < NUM_MICS; ++m) {
        const float d = arr.position(m).x * ux + arr.position(m).y * uy;
        if (d > bd) { bd = d; best = static_cast<int>(m); }
    }
    return best;
}

int main()
{
    // 1) Umrechnung
    float ux, uy; bool ok = true;
    for (float az = 0; az < 360; az += 7.5f) {
        Azimuth::toArray(az, ux, uy);
        ok = ok && Azimuth::diff(Azimuth::fromArray(ux, uy), az) < 1e-3f;
    }
    check(ok, "toArray/fromArray umkehrbar (0 … 352,5°)");
    check(Azimuth::wrap360(-90) == 270 && Azimuth::wrap360(360) == 0 && Azimuth::wrap360(725) == 5, "wrap360");
    check(std::fabs(Azimuth::diff(359, 1) - 2) < 1e-4f && std::fabs(Azimuth::diff(10, 190) - 180) < 1e-4f, "diff");

    // 2) Geometrie
    std::printf("Richtung -> Mikrofon: N %d, O %d, S %d, W %d\n", micToward(0), micToward(90), micToward(180), micToward(270));
    check(micToward(0) == 0, "0° (Nord) = Mikrofon 0");
    if (!Azimuth::MIC_NUMBERING_CLOCKWISE)
        check(micToward(90) == 6 && micToward(180) == 4 && micToward(270) == 2,
              "Nummerierung gegen den Uhrzeigersinn: Ost = Mikrofon 6, Süd = 4, West = 2");

    // 3) Kette
    auto& arr = Microphone_Array_114::instance();
    auto& sim = Signal_Simulator::instance();
    corr.init(arr); corr.setSrpReference(true);
    FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing b{};
    bool chainOk = true;
    for (int az = 0; az < 360; az += 45) {
        SimParams p; p.scenario = SimScenario::DroneStatic; p.snr_db = 30.0f; p.azimuth_deg = static_cast<float>(az);
        sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init();
        std::vector<float> e, es;
        for (int i = 0; i < 130; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp[REF_MIC], fv);
            for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
            ml.infer(feat.magnitude(), fv, st);
            if (i < 100) continue;
            corr.deriveSelection(st, sel);
            if (!corr.estimateBearing(sp, sel, b)) continue;
            e.push_back(Azimuth::diff(b.azimuth_deg, p.azimuth_deg));
            float sa, sw, sr;
            if (corr.srpScan(sa, sw, sr)) es.push_back(Azimuth::diff(sa, p.azimuth_deg));
        }
        std::sort(e.begin(), e.end()); std::sort(es.begin(), es.end());
        const float me = e.empty() ? 999 : e[e.size() / 2], ms = es.empty() ? 999 : es[es.size() / 2];
        std::printf("Quelle %3d° -> Peilung Median-Fehler %.2f° (SRP %.2f°), %zu Frames\n", az, me, ms, e.size());
        chainOk = chainOk && me <= 1.0f && ms <= 1.0f;
    }
    check(chainOk, "Kette: Peilung und SRP im Kompass-Azimut der Quelle");

    // 4) Nordabgleich
    float off = 0.0f;
    check(Azimuth::offsetFromCenti(1500, off) && std::fabs(off - 15.0f) < 1e-4f &&
          Azimuth::offsetFromCenti(-18000, off) && std::fabs(off + 180.0f) < 1e-4f,
          "Offset aus 0,01° (15,00°, −180,00°)");
    off = 7.0f;
    check(!Azimuth::offsetFromCenti(18001, off) && !Azimuth::offsetFromCenti(-18001, off) && off == 7.0f,
          "Offset außerhalb ±180° verworfen, alter Wert bleibt");
    Localisation_Module_128 loc;
    const Vec3 origin{};
    loc.init(&origin, 1);
    Bearing bc{}; bc.valid = true; bc.valid_pairs = NUM_MIC_PAIRS;
    CandidateLocation l{};
    bc.azimuth_deg = 350.0f; loc.setCalibration(15.0f, 1.0f); loc.fromBearing(bc, 1.0f, l);
    const bool w1 = std::fabs(l.azimuth_deg - 5.0f) < 1e-3f;
    bc.azimuth_deg = 10.0f;  loc.setCalibration(-20.0f, 1.0f); loc.fromBearing(bc, 1.0f, l);
    const bool w2 = std::fabs(l.azimuth_deg - 350.0f) < 1e-3f;
    check(w1 && w2, "128: Offset auf die Peilung, Ergebnis in [0, 360)");
    return g_fail;
}
