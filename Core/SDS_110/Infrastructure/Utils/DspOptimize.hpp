/*
 * DspOptimize.hpp  (Infrastructure/Utils)
 *
 * Als ERSTES #include in den rechenintensiven Modulen (Simulator, 118, 122, 124, 126): übersetzt
 * sie auf dem Board auch im Debug-Build (-O0) mit -O2. Im Debug-Build lag der ProcessingTask
 * sonst bei ~380 ms je 32-ms-Hop. Diese Dateien lassen sich dann nur eingeschränkt
 * schrittweise debuggen; mit -DSDS110_DSP_NO_OPT gilt wieder die Optimierung des Builds.
 * Host-Tests (x86) bleiben unverändert. Die Zeile geht nicht in die Merkmalsversion ein
 * (tools/features/Makefile filtert sie).
 */
#pragma once
#if defined(__arm__) && defined(__GNUC__) && !defined(__clang__) && !defined(SDS110_DSP_NO_OPT)
#pragma GCC optimize ("O2")
#endif

// FFT-Twiddle-Tabellen beim Start ins interne RAM kopieren (FftTables.hpp); 0 = Flash.
// STM32F746ZGT6 (kein SDRAM, 320 kB SRAM): Standard 0 – die 32 kB fehlen sonst für die Hop-Puffer;
// die FFT liest die Twiddles dann über AXIM/ART aus dem Flash (etwas langsamer, Ergebnisse bitgleich).
#ifndef SDS110_FFT_TABLES_IN_RAM
#define SDS110_FFT_TABLES_IN_RAM 0
#endif

// Das Pragma schaltet im Debug-Build (-O0) das Inlining NICHT ein: kleine Hilfsfunktionen und
// Lambdas in inneren Schleifen bleiben Funktionsaufrufe (am Board gemessen: Simulator 13,5 ms
// je Hop). SDS110_FORCE_INLINE erzwingt es auch unter -O0.
#if defined(__GNUC__)
#define SDS110_FORCE_INLINE inline __attribute__((always_inline))
#else
#define SDS110_FORCE_INLINE inline
#endif
