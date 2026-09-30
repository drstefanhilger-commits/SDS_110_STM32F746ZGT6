/*
 * Pre_Processor_118.hpp
 *
 * Vorverarbeitung 118 (Patent, Abschnitt 1, FIG. 2), in-place je Mikrofon und Hop (32 ms):
 *   1. Bandpass 80 Hz – 8 kHz   : Butterworth 2. Ordnung HPF + LPF (Biquad DF2T, CMSIS)
 *   2. Adaptive Rauschunterdrückung : Rauschboden-Verfolgung (RMS-Minimum) und
 *                                     Wiener-artige Verstärkung pro Hop
 *   3. AGC                      : Hop-RMS auf AGC_TARGET_RMS regeln, Attack/Release
 *   NS · AGC wird als lineare Rampe vom Wert des vorigen Hops angewendet (kein Sprung
 *   an der Hop-Grenze, die in der Mitte der überlappenden Analyse-Frames liegt).
 *
 * Reihenfolge Bandpass -> NS -> AGC, damit AGC nicht auf Rauschen hochregelt.
 * Alle Zustände pro Kanal; das Frame ist frei von Blockgrenzen-Artefakten,
 * da die Biquad-Zustände über Frames hinweg erhalten bleiben.
 *
 * Migration aus SDS: nur SDS_BANDPASS_LOW/HIGH (800–3000 Hz) als Konstanten –
 * kein lauffähiger Code vorhanden. Grenzen jetzt aus SDS_110_Config (80–8000 Hz).
 */
#pragma once
#include "arm_math.h"
#include "SDS_110_Config.hpp"
#include "Microphone_Array_114.hpp"

namespace sds110 {

class Pre_Processor_118 {
public:
    void init();
    /// Hop in place verarbeiten: je Kanal aus dem Blockgleitkomma (Rohdaten) in einen float-
    /// Zwischenpuffer (DspScratch), Bandpass/NS/AGC wie bisher, Ergebnis zurück als Blockgleitkomma
    /// (frame.processed = true). Der Frame_Assembler verwendet den Hop danach ohne Kopie.
    void process(MicFrame& frame);

    /// Diagnose: aktuelle AGC-Verstärkung / Rauschboden je Kanal
    float gain(uint32_t ch) const { return gain_[ch]; }
    /// Gesamtverstärkung (NS · AGC) am Ende des letzten Hops auf Kanal ch (Rampe vom Wert des
    /// vorigen Hops aus). Pegel nach 118 geteilt durch diesen Wert = Pegel vor der Regelung
    /// (128-Distanz); im eingeschwungenen Zustand exakt, während einer Pegeländerung genähert.
    float appliedGain(uint32_t ch) const { return applied_[ch]; }
    /// Verstärkung in der Mitte des aktuellen Analyse-Frames (= Ende des vorletzten Hops);
    /// dort gewichtet das Hann-Fenster (122) am stärksten -> Bezug für den Pegel in 120.
    float frameCenterGain(uint32_t ch) const { return prevApplied_[ch]; }
    float noiseFloor(uint32_t ch) const { return noiseRms_[ch]; }

    void enableBandpass(bool on) { bandpassOn_ = on; }
    void enableNoiseSuppression(bool on) { nsOn_ = on; }
    void enableAgc(bool on) { agcOn_ = on; }

private:
    void bandpass(uint32_t ch, float* x, uint32_t n);
    float noiseGain(uint32_t ch, float rms);   // NS-Verstärkung, führt den Rauschboden nach
    float agcGain(uint32_t ch, float rms);     // AGC-Verstärkung (geglättet)
    static float rms(const float* x, uint32_t n);
    static void  designHighpass(float fc, float fs, float* coeffs);
    static void  designLowpass (float fc, float fs, float* coeffs);

    // Biquad-Kaskade: 2 Stufen (HPF, LPF) je Kanal, 5 Koeffizienten je Stufe
    float coeffs_[2 * 5];
    float state_[NUM_MICS][2 * 2];
    arm_biquad_cascade_df2T_instance_f32 iir_[NUM_MICS];

    float aAttack_ = 0.0f, aRelease_ = 0.0f, aFloor_ = 0.0f;   // Glättung je Hop aus *_TAU_S
    float gain_[NUM_MICS];
    float noiseRms_[NUM_MICS];
    float applied_[NUM_MICS];
    float prevApplied_[NUM_MICS];
    bool  bandpassOn_ = true;
    bool  nsOn_       = true;
    bool  agcOn_      = true;
};

} // namespace sds110
