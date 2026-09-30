/*
 * t_feedback – Feedback der Tracking-Einheit (FSL9 §10, Anspruch 8), USB-Kommando Id 8
 *
 * Prüft:
 *   1. FeedbackCodec: ŝ (4 Bit je Band), Azimut 0,01°, Distanz 0,1 m, Flags; falsche Länge und
 *      Azimut ≥ 360° werden verworfen
 *   2. Output_Interface_130::pollFeedback: neues Feedback -> true; unverändert -> false; älter als
 *      2 s -> einmal true mit valid = false (Zurücksetzen, Befund 34), danach false
 *   3. 126: Band mit ŝ_b = 0,8 wird schon bei p_b = 0,4 selektiert (θ = 0,3), Gewicht × 1,8;
 *      nach dem Zurücksetzen wieder θ_sel = 0,5
 * Aufruf: build/test_host/t_feedback
 */
#include <cstdio>
#include <cmath>
#include "Infrastructure/Utils/FeedbackCodec.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"
#include "Processing_Module_120/Output_Interface_130/Output_Interface_130.hpp"
#include "Processing_Module_120/Correlation_Processing_Module_126/Correlation_Processing_Module_126.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

// wie PC_Monitor app/tracking/feedback.py
static void encode(uint8_t* m, const float* s, float az, float r, uint8_t flags)
{
    const uint8_t head[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x08, 0x00, 0x00, 0x34};
    for (int i = 0; i < 8; ++i) m[i] = head[i];
    for (int i = 0; i < 32; ++i) {
        auto q = [&](int b) { int v = static_cast<int>(std::lround(s[b] * 15.0f)); return static_cast<uint8_t>(v < 0 ? 0 : v > 15 ? 15 : v); };
        m[8 + i] = static_cast<uint8_t>((q(2 * i) << 4) | q(2 * i + 1));
    }
    const uint16_t a = static_cast<uint16_t>(std::lround(az * 100.0f)), d = static_cast<uint16_t>(std::lround(r * 10.0f));
    m[40] = a >> 8; m[41] = a & 0xFF; m[42] = d >> 8; m[43] = d & 0xFF; m[44] = flags;
    for (int i = 45; i < 52; ++i) m[i] = 0;
}

int main()
{
    // 1) Codec
    float s[NUM_BANDS] = {}; s[3] = 0.8f; s[4] = 0.2f; s[10] = 1.0f; s[63] = 0.6f;
    uint8_t m[52]; encode(m, s, 123.45f, 87.6f, 0x03);
    TrackingFeedback fb; bool pos = false;
    check(FeedbackCodec::decode(m, 52, fb, pos), "decode");
    check(std::fabs(fb.ref_state[3] - 12.0f / 15) < 1e-6f && std::fabs(fb.ref_state[4] - 3.0f / 15) < 1e-6f &&
          fb.ref_state[10] == 1.0f && std::fabs(fb.ref_state[63] - 9.0f / 15) < 1e-6f && fb.ref_state[0] == 0.0f,
          "ŝ je Band (4 Bit, Band 2i oben)");
    check(std::fabs(fb.pred_azimuth_deg - 123.45f) < 1e-3f && std::fabs(fb.pred_distance_m - 87.6f) < 1e-3f &&
          fb.valid && pos, "Vorhersage und Flags");
    check(!FeedbackCodec::decode(m, 51, fb, pos), "falsche Länge verworfen");
    // Bytes vom PC-Monitor (app/tracking/feedback.py build_feedback, gleiche Werte) inkl. CRC
    const char* pcHex = "deadbeef08000034000c300000f000000000000000000000000000000000000000000000000000093039036c0300000074002685";
    uint8_t pc[52];
    for (int i = 0; i < 52; ++i) { unsigned v; std::sscanf(pcHex + 2 * i, "%2x", &v); pc[i] = static_cast<uint8_t>(v); }
    bool samePayload = true;
    for (int i = 0; i < 48; ++i) samePayload = samePayload && pc[i] == m[i];
    TrackingFeedback fpc; bool ppc = false;
    check(samePayload && FeedbackCodec::decode(pc, 52, fpc, ppc) && fpc.valid && ppc &&
          std::fabs(fpc.pred_azimuth_deg - 123.45f) < 1e-3f, "Bytes des PC-Monitors gleich und dekodierbar");
    uint8_t bad[52]; encode(bad, s, 0, 0, 1); bad[40] = 0x8C; bad[41] = 0xA0;   // 36000 = 360,00°
    check(!FeedbackCodec::decode(bad, 52, fb, pos), "Azimut 360,00° verworfen");

    // 2) Abholen mit Ablauf
    auto& dm = SDS_Data::instance();
    Output_Interface_130 out; out.init();
    TrackingFeedback got;
    check(!out.pollFeedback(got, 0), "ohne Feedback: nichts");
    FeedbackCodec::decode(m, 52, fb, pos);
    dm.setFeedback(fb, pos, 1000);
    check(out.pollFeedback(got, 1010) && got.valid && got.ref_state[10] == 1.0f, "neues Feedback übernommen");
    check(!out.pollFeedback(got, 2500), "unverändert und jünger als 2 s: nichts");
    check(out.pollFeedback(got, 3001) && !got.valid, "älter als 2 s: zurücksetzen (valid = false)");
    check(!out.pollFeedback(got, 5000), "danach nichts mehr");

    // 3) Wirkung in 126
    Correlation_Processing_Module_126 corr; corr.init(Microphone_Array_114::instance());
    AcousticState st{}; ComponentSelection sel{};
    st.p[3] = 0.4f; st.p[20] = 0.9f; st.p[21] = 0.9f; st.p[22] = 0.9f;
    uint32_t k0, k1; Feature_Extraction_Module_122::bandBins(3, k0, k1);
    corr.deriveSelection(st, sel);
    const bool before = sel.selected[k0];
    FeedbackCodec::decode(m, 52, fb, pos);
    corr.applyFeedback(fb);
    corr.deriveSelection(st, sel);
    const bool with = sel.selected[k0];
    const float w = sel.weight[k0];
    corr.applyFeedback(TrackingFeedback{});
    corr.deriveSelection(st, sel);
    std::printf("Band 3 (p = 0,4, ŝ = 0,8): ohne Feedback %d, mit %d (Gewicht %.2f), zurückgesetzt %d\n",
                before, with, w, sel.selected[k0]);
    check(!before && with && !sel.selected[k0], "θ = 0,3 für ŝ_b > 0,6, nach Zurücksetzen wieder 0,5");
    check(std::fabs(w - 0.4f * (1.0f + 12.0f / 15)) < 1e-4f, "Gewicht p · (1 + ŝ_b)");
    return g_fail;
}
