/*
 * SDS_110_Config.hpp
 * Implementierungsparameter des bevorzugten Ausführungsbeispiels
 * (Patent, Abschnitte 1–10; Werte in [ ] sind noch zu bestätigen).
 */
#pragma once
#include <cstdint>
#include "SDS_110_Board.h"   // SDS110_SAI_ENABLED

namespace sds110 {

// --- 1. Systemarchitektur ---------------------------------------------
constexpr uint32_t NUM_UNITS        = 1;      // N Sensor Units (Patent: N >= 3)
constexpr uint32_t NUM_MICS         = 8;      // M pro Unit (Oktagon)
constexpr uint32_t SAMPLE_RATE_HZ   = 48000;
constexpr float    BANDPASS_LO_HZ   = 80.0f;
constexpr float    BANDPASS_HI_HZ   = 8000.0f;

// --- 2. Framing / STFT --------------------------------------------------
constexpr uint32_t FRAME_MS         = 64;
constexpr uint32_t FRAME_OVERLAP_PC = 50;
constexpr uint32_t N_FFT            = 4096;
constexpr uint32_t NUM_BINS         = N_FFT / 2 + 1;
constexpr uint32_t NUM_BANDS        = 64;     // B
constexpr float    BAND_WIDTH_HZ    = 62.5f;  // Δf
constexpr float    BAND_LO_HZ       = 80.0f;
constexpr float    BAND_HI_HZ       = 4000.0f;
constexpr uint32_t REF_MIC          = 0;      // Referenzkanal (Patent: Mikrofon [1])
/// Gespeicherte Spektrum-Bins je Mikrofon (Spectrum, 122 -> 126). 126 korreliert nur Bins der
/// Bänder (BAND_LO_HZ … BAND_LO_HZ + NUM_BANDS·BAND_WIDTH_HZ = 4080 Hz -> Bin < 349); darüber
/// liegende Bins werden nie selektiert. STM32F746ZGT6 (kein SDRAM): 8 × 2,8 kB statt 8 × 16 kB,
/// Ergebnisse der Korrelation unverändert. 122 berechnet |X| (124, Merkmale) weiter über alle Bins.
constexpr uint32_t SPECTRUM_BINS    = 352;
static_assert((BAND_LO_HZ + NUM_BANDS * BAND_WIDTH_HZ) / (static_cast<float>(SAMPLE_RATE_HZ) / N_FFT) + 0.5f
              <= static_cast<float>(SPECTRUM_BINS), "SPECTRUM_BINS muss alle Band-Bins enthalten");
static_assert(SPECTRUM_BINS < NUM_BINS - 1, "Nyquist-Bin wird nicht gespeichert");
constexpr uint32_t FRAME_SAMPLES    = SAMPLE_RATE_HZ * FRAME_MS / 1000;   // 3072
constexpr uint32_t HOP_SAMPLES      = FRAME_SAMPLES * (100 - FRAME_OVERLAP_PC) / 100; // 1536
static_assert(FRAME_SAMPLES % HOP_SAMPLES == 0, "Frame muss ein Vielfaches des Hops sein");
/// Analyse-Takt: 114 liefert Hops (32 ms), Frame_Assembler setzt je Hop einen Frame (64 ms,
/// 50 % Überlappung) zusammen -> 118 läuft je Hop, 122..126 je Frame, beide mit dieser Rate.
constexpr float    HOP_S            = static_cast<float>(HOP_SAMPLES) / SAMPLE_RATE_HZ;   // 0,032 s
/// Dauer in Sekunden -> Anzahl Hops/Frames (für Haltezeiten, Fenster usw.)
constexpr uint32_t framesFor(float seconds) { return static_cast<uint32_t>(seconds / HOP_S + 0.5f); }

// --- 114 / 116 Hardware ---------------------------------------------
constexpr float    MIC_RADIUS_M     = 0.10f;  // Oktagon-Radius 100 mm, Durchmesser 200 mm (FSL9 §1)
constexpr uint32_t DMA_BLOCK_SAMPLES = 128;   // Samples pro Mic je DMA-Halbpuffer
static_assert(HOP_SAMPLES % DMA_BLOCK_SAMPLES == 0, "114: DMA-Block darf nicht über eine Hop-Grenze reichen");
/// Hop-Pool (114, Blockgleitkomma, je ~25 kB): 1 DMA-Schreibpuffer + bis zu 2 im Analysefenster.
/// Der ältere Hop wird frei, sobald 122 die Spektren eines Frames berechnet hat (120) – das muss
/// innerhalb eines Hops (32 ms) nach dessen Ende geschehen, sonst verwirft 114 Blöcke (gezählt).
constexpr uint32_t NUM_MIC_FRAMES   = 3;
/// Rohformat im DMA-Puffer: ADAU7118 sendet 24-bit PCM MSB-first im 32-bit-TDM-Slot, der SAI
/// liest den ganzen Slot (DataSize 32) -> int32 = pcm24 << 8 (linksbündig, Bits 7..0 = 0).
/// Normierung auf [-1, 1): raw / 2^31  (entspricht (raw >> 8) / 2^23).
constexpr float    PCM_RAW_FULL_SCALE = 2147483648.0f;
constexpr uint8_t  ADAU7118_I2C_ADDR_7B = 0x4B; // alt: 0x3A in adau7118.c – prüfen! (ZGT6-Board: I2C2, ADDR/CONFIG an 3V3)
constexpr uint32_t ADAU7118_I2C_TIMEOUT_MS = 100;
/// SAI-Kerneltakt aus PLLI2S (1 MHz Eingang, PLLM = 25 fest wegen 216 MHz SYSCLK):
/// 344 MHz / 7 / 1 = 49,142857 MHz -> HAL: MCKDIV 2 -> Fs = 49,142857 MHz / 1024 = 47 991 Hz (-186 ppm).
/// Optimum aller zulässigen Einstellungen; exakt 48 kHz ist mit 1 MHz PLL-Eingang nicht möglich.
/// PLLSAI (CubeMX: 192 MHz -> 46,875 kHz) wird für SAI1 nicht verwendet; 116 schaltet SAI1 auf PLLI2S.
constexpr uint32_t SAI_PLLI2S_N      = 344;
constexpr uint32_t SAI_PLLI2S_Q      = 7;
constexpr uint32_t SAI_PLLI2S_DIVQ   = 1;
constexpr float    SAI_FS_TOLERANCE  = 1e-3f;   // max. relative Abweichung der Ist-Abtastrate
/// DMA-Puffer in SRAM2, per MPU nicht cachebar (Linker: .dma_nocache in RAM_NC, 16 kB)
#define SDS110_DMA_SECTION   __attribute__((section(".dma_nocache")))

// --- 118 Pre-Processing -------------------------------------------------
constexpr float    AGC_TARGET_RMS   = 0.1f;   // Zielpegel (float, Vollaussteuerung = 1)
constexpr float    AGC_MAX_GAIN     = 32.0f;  // +30 dB
constexpr float    AGC_MIN_GAIN     = 0.05f;  // -26 dB
// Zeitkonstanten in Sekunden; 118 rechnet sie je Hop in Glättungsfaktoren um (1 − exp(−HOP_S/τ)).
// Werte entsprechen den bisherigen Faktoren je 64-ms-Frame (0,30 / 0,05 / 0,02).
constexpr float    AGC_ATTACK_TAU_S = 0.179f; // Pegel steigt -> schnell runterregeln
constexpr float    AGC_RELEASE_TAU_S= 1.248f; // Pegel fällt -> langsam hochregeln
constexpr float    NS_FLOOR_TAU_S   = 3.168f; // Rauschboden-Nachführung (langsam)
constexpr float    NS_MAX_ATTEN     = 0.25f;  // maximale Dämpfung (-12 dB) bei reinem Rauschen

// --- 122 Feature Extraction ---------------------------------------------
constexpr uint32_t MEL_BANDS        = 40;     // wie altes Modell
constexpr float    MEL_LO_HZ        = 80.0f;
constexpr float    MEL_HI_HZ        = 8000.0f;
constexpr uint32_t MEL_MAX_BINS_PER_BAND = 128; // sparse Filterbank; oberstes Band (8 kHz) ~110 Bins
constexpr uint32_t AM_HISTORY_FRAMES = framesFor(0.5f);   // 16 Frames bei 32 ms Hop

// --- 3. ML ------------------------------------------------------------
constexpr uint32_t STATE_SMOOTH_FRAMES = framesFor(0.19f); // gleitender Mittelwert ~0,19 s (6 Frames)

// --- 4. Selektion / Gewichtung -----------------------------------------
constexpr float    THETA_SEL        = 0.5f;   // Selektionsschwelle
constexpr uint32_t B_MIN            = 3;      // min. selektierte Bänder
constexpr float    WEIGHT_GAMMA     = 1.0f;   // g(p)=p^γ, γ=1 bevorzugt

// --- 5. GCC-PHAT / TDOA -------------------------------------------------
constexpr float    PEAK_RATIO_MIN   = 1.5f;
constexpr float    PEAK_RATIO_MAX   = 20.0f;  // Obergrenze (Gewicht in 128::solve), wenn kein Nebenmaximum im Fenster
constexpr float    SPEED_OF_SOUND   = 343.0f; // wird temperaturkorrigiert

// --- 5b. Intra-Unit-Peilung (126, erlaubt: Korrelation, kein Beamforming) -
constexpr uint32_t NUM_MIC_PAIRS    = NUM_MICS * (NUM_MICS - 1) / 2;   // 28
constexpr float    BEARING_MIN_PAIRS_FRACTION = 0.5f;                  // min. Anteil gültiger Paare
// Referenz-Peilung SRP-PHAT (Harness/Vergleich): Scan über die gespeicherten Paarkorrelationen
constexpr uint32_t SRP_MAX_LAG      = 32;     // Samples, > Arraydurchmesser/c*fs*1.1 (0,2 m -> 30,8)
// 126: Peak-Suche braucht ±(maxLag+1), maxLag = ⌊Durchmesser/c·fs·1,1⌋; der Schnellpfad rechnet ±SRP_MAX_LAG
static_assert(2.0f * MIC_RADIUS_M / SPEED_OF_SOUND * SAMPLE_RATE_HZ * 1.1f + 1.0f <= SRP_MAX_LAG,
              "SRP_MAX_LAG zu klein für den Arraydurchmesser");
constexpr uint32_t SRP_AZ_STEPS     = 360;    // 1° Raster
constexpr bool     SRP_REFERENCE_ENABLED = true;

// --- 6. Lokalisation ----------------------------------------------------
constexpr uint32_t MIN_PAIRS        = 3;
constexpr uint32_t LS_ITERATIONS    = 8;      // Gauss-Newton
// Einzel-Unit-Fallback (Legacy 1/r-Pegelmodell aus DistanceEstimator; kein Patentbestandteil)
constexpr bool     SINGLE_UNIT_LEVEL_DISTANCE = true;
constexpr float    LEVEL_DIST_K_REF = 100.0f;  // r = K / (A + eps)
constexpr float    LEVEL_DIST_EPS   = 1e-3f;
// Konfidenz eines Kandidaten (128): (Paare / max. Paare) · 1 / (1 + (Residuum / Referenz)^2),
// Residuum in Samples (TDOA). Host-Test, Median: Drohne 30 dB 0,85 · 0 dB 3,9 · Einzelton 9 ·
// Stille (Zufallspeilung) 32 Samples -> Konfidenz ≈ 0,96 / 0,51 / 0,14 / 0,01.
constexpr float    CONF_RESIDUAL_REF_SAMPLES = 4.0f;

// --- Harness: Simulation ohne Mikrofone (USB Typ 3 = 1) -----------------
constexpr uint8_t  SIM_SCENARIO_ID  = 0;      // 0 DroneSweep, 1 DroneStatic, 2 SingleTone, 3 WindNoise, 4 Silence
constexpr float    SIM_F0_HZ        = 180.0f;
constexpr float    SIM_SNR_DB       = 20.0f;

// --- 124 HBD -> s(t) ----------------------------------------------------
constexpr float    HBD_BAND_SNR_DB  = 8.0f;   // Band-SNR, bei dem p_b = 0,5
constexpr float    HBD_SIGMOID_DB   = 3.0f;   // Steilheit der Sigmoid (dB)
constexpr float    HBD_GATE_FLOOR   = 0.3f;   // Faktor für Bänder ohne Harmonische bzw. bei geschlossenem Gate
constexpr uint32_t HBD_HOLD_FRAMES  = framesFor(1.0f);   // Gate offen bis 1 s nach der letzten HBD-Detektion
// HBD-Zeitkonstanten (HBD_InitParams_48k rechnet sie in Werte je Frame um)
constexpr float    HBD_FLOOR_TAU_S      = 1.034f;  // Noise-Floor-EMA (bisher 0,94 je 64-ms-Frame)
constexpr float    HBD_FLOOR_RISE_DB_S  = 0.156f;  // max. Anstieg an Peaks (bisher 0,01 dB je 64-ms-Frame, ~2 min)
constexpr float    HBD_WARMUP_S         = 3.0f;    // keine Entscheidung nach dem Start
constexpr float    HBD_CONSISTENCY_S    = 0.32f;   // Konsistenz-Gedächtnis (bisher 5 Frames à 64 ms)

// --- 10. Feedback -------------------------------------------------------
constexpr float    THETA_REF        = 0.6f;
constexpr float    THETA_LOW        = 0.3f;
constexpr float    TDOA_WINDOW_S    = 2e-3f;

} // namespace sds110
