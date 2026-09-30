/*
 * t_sim_flyby – Simulator: Auswahl über USB Id 3 und Szenario FlyBy (Harness/SimScenario.hpp)
 *
 * Prüft:
 *   1. Wert von USB Id 3 -> Szenario: 0 Mikrofone, 1 SIM_SCENARIO_ID, 2 + k Szenario k, sonst ungültig
 *   2. FlyBy-Geometrie (Standard 15 m/s, 30 m, Ost-Kurs): Mitte des Flugs 30 m bei 0° (Nord),
 *      Anfang 48 m bei 308,7°; nach 5 s Pause, nach 10 s nächster Durchgang mit um 45° gedrehtem Kurs;
 *      die Bahn liegt um den lokalen Ursprung: Einheit bei [0, −50, 0] -> kürzester Abstand 80 m
 *   3. Zeitablauf im Simulator: Quelle 5 s aktiv, 5 s Pause, wiederholt (je Hop 32 ms)
 *   4. volle Kette (118 -> 126): Peilung während des Flugs folgt der Quelle (Median ≤ 3°, 95 % ≤ 10°);
 *      HBD erkennt die Drohne im Flug, in der zweiten Hälfte der Pause nicht (ab dem 2. Durchgang;
 *      beim Kaltstart braucht HBD ~3 s)
 * Aufruf: build/test_host/t_sim_flyby
 */
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "chain.hpp"
#include "Harness/SimScenario.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static Spectrum sp[NUM_MICS];
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr;

int main()
{
    // 1. Auswahl
    SimScenario s;
    check(!simScenarioFromCommand(0, s) && simCommandValid(0), "0 = Mikrofone (gültig, kein Szenario)");
    check(simScenarioFromCommand(1, s) && s == static_cast<SimScenario>(SIM_SCENARIO_ID), "1 = Standardszenario SIM_SCENARIO_ID");
    bool all = true;
    for (uint32_t k = 0; k < SIM_NUM_SCENARIOS; ++k)
        all = all && simScenarioFromCommand(2 + k, s) && static_cast<uint32_t>(s) == k;
    check(all && simScenarioFromCommand(7, s) && s == SimScenario::FlyBy, "2 + k = Szenario k (7 = FlyBy)");
    check(!simCommandValid(8) && !simCommandValid(0xFFFFFFFFu), "8 und größer ungültig");

    // 2. Geometrie (Einheit im Ursprung)
    SimParams p; p.scenario = SimScenario::FlyBy;
    float az, d;
    const float origin[3] = { 0.0f, 0.0f, 0.0f };
    check(Signal_Simulator::flyByPosition(p, 2.5f, 0, origin, az, d) && near(d, 30.0f, 0.01f) && Azimuth::diff(az, 0.0f) < 0.01f,
          "Mitte des Flugs: 30 m bei 0° (Nord)");
    check(Signal_Simulator::flyByPosition(p, 0.0f, 0, origin, az, d) && near(d, 48.02f, 0.05f) && near(az, 308.66f, 0.05f),
          "Anfang: 48,0 m bei 308,7°");
    Signal_Simulator::flyByPosition(p, 4.99f, 0, origin, az, d);
    check(near(az, 51.34f, 0.3f), "Ende: 51,3° (Flug nach Ost, nördlich der Einheit)");
    check(!Signal_Simulator::flyByPosition(p, 5.0f, 0, origin, az, d) && !Signal_Simulator::flyByPosition(p, 9.9f, 0, origin, az, d), "5 … 10 s: Pause");
    check(Signal_Simulator::flyByPosition(p, 2.5f, 1, origin, az, d) && Azimuth::diff(az, 45.0f) < 0.01f, "Durchgang 2: Kurs um 45° gedreht");
    // Einheit 50 m südlich des Ursprungs: Bahn bleibt, Abstand wächst
    const float south[3] = { 0.0f, -50.0f, 0.0f };
    check(Signal_Simulator::flyByPosition(p, 2.5f, 0, south, az, d) && near(d, 80.0f, 0.01f) && Azimuth::diff(az, 0.0f) < 0.01f,
          "Einheit bei [0, −50, 0]: Mitte des Flugs 80 m bei 0°");
    check(Signal_Simulator::flyByPosition(p, 0.0f, 0, south, az, d) && near(d, 88.35f, 0.05f) && near(az, 334.89f, 0.05f),
          "Einheit bei [0, −50, 0]: Anfang 88,4 m bei 334,9°");
    const float high[3] = { 0.0f, 0.0f, 40.0f };
    check(Signal_Simulator::flyByPosition(p, 2.5f, 0, high, az, d) && near(d, 50.0f, 0.01f), "Höhenunterschied 40 m geht in die Distanz ein");

    // 3. + 4. Zeitablauf und Kette über zwei Durchgänge (20 s)
    auto& sim = Signal_Simulator::instance(); auto& arr = Microphone_Array_114::instance();
    sim.init(p); g_fa.reset(); pre.init(); feat.init(); ml.init(); corr.init(arr);
    const uint32_t hops = static_cast<uint32_t>(20.0f / HOP_S);
    uint32_t activeHops = 0, edges = 0; bool lastActive = true;
    std::vector<float> err; uint32_t flyFrames = 0, flyDet = 0, quietFrames = 0, quietDet = 0;
    FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing br{};
    for (uint32_t i = 0; i < hops; ++i) {
        const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre);   // erzeugt je Aufruf einen oder zwei Hops
        const float tAbs = static_cast<float>(f.frame_id + 1) * HOP_S;   // Zeit am Frame-Ende
        const float t = std::fmod(tAbs, 10.0f);
        const bool warm = tAbs > 10.0f;                        // HBD ab dem 2. Durchgang (Kaltstart ~3 s)
        if (sim.sourceActive()) ++activeHops;
        if (sim.sourceActive() != lastActive) { ++edges; lastActive = sim.sourceActive(); }
        feat.process(f, sp[REF_MIC], fv);
        for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
        ml.infer(feat.magnitude(), fv, st); corr.deriveSelection(st, sel); corr.estimateBearing(sp, sel, br);
        if (t > 1.0f && t < 4.9f && sim.sourceActive()) {      // Flug, nach dem Einschwingen von 118/HBD
            if (warm) { ++flyFrames; if (ml.hbd().droneDetected) ++flyDet; }
            if (br.valid) err.push_back(Azimuth::diff(br.azimuth_deg, sim.trueAzimuth()));
        }
        if (warm && t > 7.5f) { ++quietFrames; if (ml.hbd().droneDetected) ++quietDet; }
    }
    std::printf("aktive Hops %u von %u, Wechsel %u\n", activeHops, hops, edges);
    check(near(static_cast<float>(activeHops) / hops, 0.5f, 0.02f) && edges == 3, "Quelle 5 s aktiv, 5 s Pause, wiederholt");
    std::sort(err.begin(), err.end());
    const float med = err.empty() ? 999.f : err[err.size() / 2], p95 = err.empty() ? 999.f : err[(err.size() - 1) * 95 / 100];
    std::printf("Flug: %zu gültige Peilungen, |Δaz| Median %.2f°, 95 %% %.2f°; 2. Durchgang: HBD im Flug %u/%u, in der Pause %u/%u\n",
                err.size(), med, p95, flyDet, flyFrames, quietDet, quietFrames);
    check(err.size() >= 200 && med <= 3.0f && p95 <= 10.0f, "Peilung folgt dem Überflug (Median ≤ 3°, 95 % ≤ 10°)");
    check(flyFrames > 0 && flyDet >= flyFrames * 9 / 10 && quietDet == 0, "HBD: Drohne im Flug erkannt, in der Pause nicht");

    std::printf(g_fail ? "=== FEHLER\n" : "=== alle Prüfungen bestanden\n");
    return g_fail;
}
