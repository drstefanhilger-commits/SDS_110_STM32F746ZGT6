/*
 * Machine_Learning_Module_124.hpp
 *
 * Modul 124 (Patent, Abschnitt 3, FIG. 4): Ausgang s(t) = (p_1 .. p_B), B = 64.
 * Zwei Quellen für s(t), umschaltbar über Ml124Mode (ML124_Config.hpp):
 *   - HBD – Harmonic Band Detector (klassisch), Brücke HBD -> s(t) siehe unten
 *   - MLP – trainiertes Modell aus ML_Test (ML124_Model_Data.hpp, AP5/AP6 des
 *     Trainingskonzepts), Eingang: FeatureVector der letzten CONTEXT Frames
 * Der HBD läuft in jeder Stufe (Gate, Diagnose); das MLP nur in Shadow und Ml.
 *
 * Brücke HBD -> s(t):
 *   1. je Patent-Band b: SNR_b = max|X| im Band − mittlerer Noise-Floor (dB)
 *      q_b = σ((SNR_b − HBD_BAND_SNR_DB) / HBD_SIGMOID_DB)
 *   2. Bänder, in die eine Harmonische h fällt:
 *      p_b = max(q_b, c_h · σ((SNR_h − θ_h) / HBD_SIGMOID_DB)), c_h = Konsistenz von h
 *      übrige Bänder: p_b = HBD_GATE_FLOOR · q_b  (< θ_sel, werden nicht selektiert)
 *
 * Für beide Quellen:
 *   3. Gate: offen, solange der HBD in den letzten HBD_HOLD_FRAMES Frames eine Drohne
 *      erkannt hat; sonst p_b *= HBD_GATE_FLOOR. Ohne HBD-Detektion bleibt damit jedes
 *      p_b < θ_sel -> SDS_Data::detected = false, kein UnitReport.
 *   4. Glättung über STATE_SMOOTH_FRAMES
 *
 * MLP: x = Merkmale der Frames t, t−1, …, t−CONTEXT+1 (neuester zuerst, am Anfang mit dem
 * ersten Frame aufgefüllt – wie stack_context() in ML_Test), x_n = (x − MEAN) / STD, dann
 * Dense-Schichten mit ReLU bzw. Sigmoid.
 */
#pragma once
#include "SDS_110_Config.hpp"
#include "Data_Interface_140/Candidate_Report_140.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"
#include "HBD.hpp"
#include "ML124_Config.hpp"
#include "ML124_Model_Data.hpp"

namespace sds110 {

class Machine_Learning_Module_124 {
public:
    static constexpr uint32_t ML_FEATURES = NUM_BANDS + MEL_BANDS + 1 + NUM_BANDS;   // 169
    static_assert(ml124model::NUM_FEATURES == ML_FEATURES, "Merkmalszahl Modell <-> 122");
    static_assert(ml124model::NUM_OUTPUTS == NUM_BANDS, "Ausgänge Modell <-> Bänder");
    static_assert(ml124model::NUM_INPUTS == ml124model::CONTEXT * ML_FEATURES, "Kontext");

    /// Abgleich HBD ↔ ML in der Stufe Shadow (seit init() bzw. setMode()).
    /// Selektion: p_b > THETA_SEL, Detektion: mindestens B_MIN selektierte Bänder (wie SDS_Data).
    struct ShadowStats {
        uint32_t frames = 0;
        uint32_t detectHbd = 0, detectMl = 0, detectAgree = 0;   // Frames
        uint32_t selHbd = 0, selMl = 0, selBoth = 0;             // Band-Frames
        float    sumAbsDiff = 0.0f;                              // Σ_t mean_b |p_hbd − p_ml|
        float detectAgreement() const { return frames ? static_cast<float>(detectAgree) / frames : 1.0f; }
        /// Jaccard-Index der selektierten Bänder (1 = identisch)
        float bandOverlap() const { const uint32_t u = selHbd + selMl - selBoth; return u ? static_cast<float>(selBoth) / u : 1.0f; }
        float meanAbsDiff() const { return frames ? sumAbsDiff / frames : 0.0f; }
    };

    bool init();
    /// mag: |X(t,k)| mit NUM_BINS Werten (122::magnitude()), features: FeatureVector aus 122
    bool infer(const float* mag, const FeatureVector& features, AcousticState& state);
    bool ready() const { return ready_; }

    /// Stufe umschalten (setzt Glättung und Schatten-Statistik zurück, HBD läuft weiter)
    void setMode(Ml124Mode m);
    Ml124Mode mode() const { return mode_; }

    // Diagnose (LCD / Logger / Host-Tests)
    const HBD_State&   hbd() const { return hbdState_; }
    HBD_Params&        params()    { return hbdParams_; }
    const ShadowStats& shadow() const { return shadow_; }
    /// letzte ungeglättete MLP-Ausgabe (vor Gate), nur in Shadow/Ml gültig
    const float*       mlRaw() const { return mlOut_; }

    /// MLP auf einem Eingangsvektor (Rohmerkmale, gestapelt, NUM_INPUTS Werte); für t_ml124.
    /// x wird dabei in place normiert.
    static void mlpForward(float* x, float* out);

private:
    struct Smoother {
        float    hist[STATE_SMOOTH_FRAMES][NUM_BANDS] = {};
        uint32_t idx = 0, count = 0;
        void reset() { idx = count = 0; }
        void apply(float* p);
    };

    void bandProbabilities(float* p) const;
    void applyGate(float* p) const;
    void pushContext(const FeatureVector& f);
    void updateShadow(const float* pHbd, const float* pMl);

    HBD_Params hbdParams_{};
    HBD_State  hbdState_{};
    bool      ready_ = false;
    Ml124Mode mode_  = ML124_MODE;
    uint32_t  frame_ = 0;
    uint32_t  sinceDetect_ = HBD_HOLD_FRAMES;      // Frames seit der letzten HBD-Detektion

    Smoother  smoothHbd_, smoothMl_;
    ShadowStats shadow_{};

    // MLP: Merkmalsverlauf (Ring, ctxHead_ = neuester), Eingang, Ausgabe
    float    ctx_[ml124model::CONTEXT][ML_FEATURES] = {};
    uint32_t ctxHead_ = 0, ctxCount_ = 0;
    float    x_[ml124model::NUM_INPUTS] = {};
    float    mlOut_[NUM_BANDS] = {};
};

} // namespace sds110
