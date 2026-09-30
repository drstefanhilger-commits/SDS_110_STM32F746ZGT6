/*
 * FftTables.hpp  (Infrastructure/Utils)
 *
 * Twiddle-Tabellen der CMSIS-RFFT (N_FFT = 4096) im internen RAM statt im Flash (Rechenlast,
 * Board-Messung 28.09.2026: 1,69 ms je Mikrofon für Fenster + FFT). Die Tabellen der Bibliothek
 * liegen im Flash (AXIM, 7 Wartezyklen); die Radix-8-Stufen lesen sie mit großen Schritten, der
 * 4-KB-D-Cache hält sie nicht. Kopiert werden die beiden Twiddle-Tabellen (2 × 16 KB, gemeinsam
 * für 122 und 126), die Bitumkehr-Tabelle wird sequentiell gelesen und bleibt im Flash.
 * Ergebnisse bitgleich (dieselben Werte, nur an anderer Adresse).
 *
 * Schalter SDS110_FFT_TABLES_IN_RAM (DspOptimize.hpp); Host-Tests: ohne Wirkung (Shim-FFT).
 */
#pragma once
#include "arm_math.h"
#include "Infrastructure/Utils/DspOptimize.hpp"
#include <cstring>

namespace sds110 {

inline void fftTablesToRam(arm_rfft_fast_instance_f32& s)
{
#if defined(__arm__) && SDS110_FFT_TABLES_IN_RAM
    static float s_twCfft[4096];            // twiddleCoef_2048
    static float s_twRfft[4096];            // twiddleCoef_rfft_4096
    static bool  s_ready = false;
    if (s.fftLenRFFT != 4096) return;
    if (!s_ready) {
        std::memcpy(s_twCfft, s.Sint.pTwiddle, sizeof(s_twCfft));
        std::memcpy(s_twRfft, s.pTwiddleRFFT, sizeof(s_twRfft));
        s_ready = true;
    }
    s.Sint.pTwiddle = s_twCfft;
    s.pTwiddleRFFT  = s_twRfft;
#else
    (void)s;
#endif
}

} // namespace sds110
