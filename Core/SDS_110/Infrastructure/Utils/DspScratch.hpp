/*
 * DspScratch.hpp  (Infrastructure/Utils)
 *
 * Gemeinsamer Arbeitsspeicher der Rechenstufen im ProcessingTask (STM32F746ZGT6: 320 kB SRAM,
 * kein SDRAM). Die Stufen laufen je Hop/Frame strikt nacheinander im selben Task und brauchen
 * ihre großen Zwischenpuffer nur innerhalb eines Aufrufs – sie teilen sich deshalb diesen Bereich:
 *   118  Kanalpuffer (HOP_SAMPLES)                        Pre_Processor_118::process
 *   122  FFT-Eingang buf_ [0, N_FFT), FFT-Ausgang fftOut_ [N_FFT, 2·N_FFT)
 *   126  gepacktes Spektrum spec_ [0, N_FFT), Korrelation corr_ [N_FFT, 2·N_FFT)
 * Kein Inhalt überlebt den Aufruf der jeweiligen Stufe. Ersetzt 32 + 32 + 6 kB getrennter Puffer
 * durch 32 kB. Nicht aus anderen Tasks oder Interrupts verwenden.
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"

namespace sds110 {

constexpr uint32_t DSP_SCRATCH_FLOATS = 2 * N_FFT;

inline float* dspScratch()
{
    alignas(32) static float s[DSP_SCRATCH_FLOATS];
    return s;
}

} // namespace sds110
