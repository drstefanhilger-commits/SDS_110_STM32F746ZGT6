/*
 * t_sound_speed – temperaturkorrigierte Schallgeschwindigkeit (FSL9 §5, A23)
 *
 * Bezug: Sync-Kommando USB Typ 7 mit Lufttemperatur (doc/ICD_SDS_PC_Monitor.md)
 * Prüft:
 *   1. SoundSpeed::fromTemperature: 0 °C 331,3 m/s, 20 °C 343,2 m/s, −40 °C 306,1, +60 °C 365,9
 *   2. SoundSpeed::decode: 0,01 °C, 0x8000 = unbekannt, außerhalb −40…+60 °C verworfen
 *   3. volle Kette (DroneStatic 30 dB, 6 Richtungen) bei −40, −20, 0, 20, 40, 60 °C, simulierte
 *      Luft und 126/128 mit derselben c: Peilung gültig, Median-Fehler ≤ 1°, Schnellpfad in jedem
 *      Frame (Lag-Fenster wächst bei Kälte bis 35), mit SRP-Referenz: SRP-Fehler ≤ 1°
 *   4. ohne Korrektur (126 mit 343 m/s) bei −40 °C: weniger gültige Paare / größeres Residuum als
 *      mit Korrektur (Verzögerungen bis 31,4 Samples liegen am Rand des Fensters ±30)
 * Aufruf: build/test_host/t_sound_speed
 */
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include "chain.hpp"
#include "Infrastructure/Utils/SoundSpeed.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }
static float median(std::vector<float> v) { if (v.empty()) return NAN; std::sort(v.begin(), v.end()); return v[v.size() / 2]; }

static Spectrum sp[NUM_MICS];
static Pre_Processor_118 pre; static Feature_Extraction_Module_122 feat; static Machine_Learning_Module_124 ml;
static Correlation_Processing_Module_126 corr;

struct Result { float azMed, srpMed, pairs, resMed; int frames, valid, direct, maxWin; };

// Luft mit airC, 126 rechnet mit estC
static Result run(float airC, float estC, bool srp)
{
    auto& arr = Microphone_Array_114::instance();
    auto& sim = Signal_Simulator::instance();
    corr.init(arr); corr.setSpeedOfSound(estC); corr.setSrpReference(srp);
    Result r{}; std::vector<float> az, srpE, res; float pairs = 0;
    FeatureVector fv{}; AcousticState st{}; ComponentSelection sel{}; Bearing b{};
    for (int a = 0; a < 360; a += 60) {
        SimParams p; p.scenario = SimScenario::DroneStatic; p.snr_db = 30.0f; p.azimuth_deg = static_cast<float>(a + 7);
        sim.init(p); sim.setSpeedOfSound(airC); g_fa.reset(); pre.init(); feat.init(); ml.init();
        for (int i = 0; i < 140; ++i) {
            const AnalysisFrame& f = nextAnalysisFrame(sim, arr, pre); feat.process(f, sp[REF_MIC], fv);
            for (uint32_t m = 0; m < NUM_MICS; ++m) if (m != REF_MIC) feat.computeSpectrum(f, m, sp[m]);
            ml.infer(feat.magnitude(), fv, st);
            if (i < 100) continue;
            corr.deriveSelection(st, sel);
            const bool ok = corr.estimateBearing(sp, sel, b);
            ++r.frames; r.direct += corr.lastWasDirect(); r.maxWin = std::max(r.maxWin, corr.lastWindowHalf());
            if (!ok) continue;
            ++r.valid; pairs += b.valid_pairs; res.push_back(b.residual * SAMPLE_RATE_HZ);
            float d = std::fabs(b.azimuth_deg - p.azimuth_deg); if (d > 180) d = 360 - d; az.push_back(d);
            float sa, sw, sr;
            if (srp && corr.srpScan(sa, sw, sr)) { float e = std::fabs(sa - p.azimuth_deg); if (e > 180) e = 360 - e; srpE.push_back(e); }
        }
    }
    r.azMed = median(az); r.srpMed = median(srpE); r.resMed = median(res); r.pairs = r.valid ? pairs / r.valid : 0;
    return r;
}

int main()
{
    // 1) Formel
    const float c0 = SoundSpeed::fromTemperature(0), c20 = SoundSpeed::fromTemperature(20);
    const float cm40 = SoundSpeed::fromTemperature(-40), c60 = SoundSpeed::fromTemperature(60);
    std::printf("c: 0 °C %.2f, 20 °C %.2f, −40 °C %.2f, +60 °C %.2f m/s\n", c0, c20, cm40, c60);
    check(std::fabs(c0 - 331.3f) < 0.01f && std::fabs(c20 - 343.2f) < 0.1f && std::fabs(cm40 - 306.1f) < 0.1f &&
          std::fabs(c60 - 365.9f) < 0.1f, "Formel c(T)");

    // 2) Wire-Wert
    float t = 99;
    check(SoundSpeed::decode(2000, t) && std::fabs(t - 20.0f) < 1e-4f, "decode: 2000 -> 20,00 °C");
    check(SoundSpeed::decode(-4000, t) && std::fabs(t + 40.0f) < 1e-4f, "decode: −4000 -> −40,00 °C");
    t = 99;
    check(!SoundSpeed::decode(SoundSpeed::TEMP_UNKNOWN, t) && !SoundSpeed::decode(-4001, t) &&
          !SoundSpeed::decode(6001, t) && t == 99, "decode: unbekannt und außerhalb −40…+60 °C verworfen");

    // 3) Kette mit Korrektur
    bool allOk = true;
    for (float T : { -40.0f, -20.0f, 0.0f, 20.0f, 40.0f, 60.0f }) {
        const float c = SoundSpeed::fromTemperature(T);
        const Result r = run(c, c, true), q = run(c, c, false);
        std::printf("%+4.0f °C c %.1f | gültig %d/%d, Paare %.1f, Residuum %.2f Sa, |Δaz| Median %.2f° (SRP %.2f°) | "
                    "Schnellpfad %d/%d, Fenster ±%d (SRP aus: ±%d, %d/%d)\n", T, c, r.valid, r.frames, r.pairs, r.resMed,
                    r.azMed, r.srpMed, r.direct, r.frames, r.maxWin, q.maxWin, q.direct, q.frames);
        allOk = allOk && r.valid == r.frames && r.azMed <= 1.0f && r.srpMed <= 1.0f && r.direct == r.frames && q.direct == q.frames;
    }
    check(allOk, "mit Korrektur −40…+60 °C: Peilung gültig, Fehler ≤ 1°, Schnellpfad in jedem Frame");

    // 4) ohne Korrektur bei −40 °C
    const Result withC = run(cm40, cm40, false), without = run(cm40, SPEED_OF_SOUND, false);
    std::printf("−40 °C ohne Korrektur: gültig %d/%d, Paare %.1f, Residuum %.2f Sa, |Δaz| %.2f° | mit: Paare %.1f, Residuum %.2f Sa, |Δaz| %.2f°\n",
                without.valid, without.frames, without.pairs, without.resMed, without.azMed, withC.pairs, withC.resMed, withC.azMed);
    check(withC.pairs > without.pairs || withC.resMed < without.resMed, "−40 °C: Korrektur verbessert Paare oder Residuum");
    return g_fail;
}
