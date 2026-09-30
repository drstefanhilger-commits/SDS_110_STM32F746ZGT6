/*
 * Microphone_Array_114.hpp
 *
 * Mikrofonarray 114 (Patent, Abschnitt 1, FIG. 1/2):
 *  - Geometrie: M = 8 IM69D130 im regelmäßigen Oktagon, Radius MIC_RADIUS_M
 *  - Hop-Puffer: Pool aus NUM_MIC_FRAMES (FREE -> WRITING -> READY -> READING),
 *    je Puffer HOP_SAMPLES (32 ms) pro Mikrofon als 16-Bit-Blockgleitkomma (MicFrame), ohne
 *    Überlappung. 118 verarbeitet den Hop in place, der Frame_Assembler hält die Hops des
 *    Analysefensters bis zu ihrer Freigabe (STM32F746ZGT6: kein SDRAM, alles im internen RAM).
 *    Die 50-%-Überlappung der Analyse-Frames entsteht erst hinter 118 im Frame_Assembler,
 *    damit 118 (IIR-Filter, NS, AGC) jedes Sample genau einmal verarbeitet.
 *
 * Migration aus SDS:
 *  - Model/SDS_Params.hpp      : SDS_MIC_POSITIONS, SDS_MIC_RADIUS
 *  - Model/SDS_MicrophoneBuffer: UnifiedMicBuffer, Zustandsautomat, Mutex
 *  Geändert: Blockweise Übernahme aus dem DMA-Puffer (pushBlock) statt
 *  Sample-für-Sample (pushSample); raw-int32-Kopie entfällt (nur noch im
 *  DMA-Ping-Pong-Puffer von 116). Frames liegen im internen RAM.
 */
#pragma once
#include <cstdint>
#include <cmath>
#include "cmsis_os2.h"
#include "SDS_110_Config.hpp"

namespace sds110 {

struct Vec3 { float x, y, z; };

enum class FrameState : uint8_t { Free = 0, Writing, Ready, Reading };

// STM32F746ZGT6-Board (kein SDRAM, 320 kB SRAM): Hop-Daten als 16-Bit-Blockgleitkomma.
// Je Kanal und DMA-Block (DMA_BLOCK_SAMPLES Samples) ein Zweierexponent: Wert = q · 2^exp.
//  - Rohdaten (114, pushBlock): q = raw >> e mit dem kleinsten e, bei dem der Block in int16 passt.
//    ADAU7118 liefert pcm24 << 8; solange |pcm24| < 2^15 im Block (leise Signale), ist e = 8 und
//    die Umwandlung verlustfrei (Wert bitgleich zur bisherigen float-Normierung raw / 2^31).
//    Lautere Blöcke behalten 16 Bit relativ zum Blockmaximum (≥ 90 dB Dynamik im Block).
//  - Nach 118 (Pre_Processor_118::process, in place): Exponent aus dem Blockmaximum, q gerundet.
// Ein Hop belegt so ~25 kB statt 49 kB (float); 114-Puffer und Analysefenster teilen sich den Pool
// (Frame_Assembler hält Zeiger, keine eigene Kopie).
constexpr uint32_t BLOCKS_PER_HOP = HOP_SAMPLES / DMA_BLOCK_SAMPLES;   // 12
// Befund 42: Zeilen auffüllen, damit der Kanalabstand kein Vielfaches von 1 KB ist
// (1536 · 2 Byte = 3 KB; mit 16 Werten Füllung 3104 Byte). Genutzt werden nur [0, HOP_SAMPLES).
constexpr uint32_t MIC_ROW_PAD = 16;

struct MicFrame {
    FrameState state;
    bool       processed;                       // false: Rohdaten (114), true: nach 118
    uint32_t   frame_id;                        // fortlaufend; nach verworfenen Blöcken eine Nummer übersprungen
    uint64_t   time_utc_us;                     // Zeitreferenz des ersten Samples
    uint32_t   writeIndex;                      // 0 .. HOP_SAMPLES
    int8_t     exp[NUM_MICS][BLOCKS_PER_HOP];   // Wert = q · 2^exp
    int16_t    q[NUM_MICS][HOP_SAMPLES + MIC_ROW_PAD];

    /// Einzelwert (normalisiert, [-1, 1) bei Rohdaten)
    float sample(uint32_t ch, uint32_t i) const
    { return static_cast<float>(q[ch][i]) * std::ldexp(1.0f, exp[ch][i / DMA_BLOCK_SAMPLES]); }
    /// Kanal ch komplett nach dst (HOP_SAMPLES Werte)
    void decode(uint32_t ch, float* dst) const
    {
        const int16_t* src = q[ch];
        for (uint32_t b = 0; b < BLOCKS_PER_HOP; ++b) {
            const float scale = std::ldexp(1.0f, exp[ch][b]);      // Zweierpotenz: exakt
            for (uint32_t i = b * DMA_BLOCK_SAMPLES; i < (b + 1) * DMA_BLOCK_SAMPLES; ++i)
                dst[i] = static_cast<float>(src[i]) * scale;
        }
    }
    /// Kanal ch aus src (HOP_SAMPLES float-Werte) blockweise mit eigenem Exponenten ablegen
    void encode(uint32_t ch, const float* src);
    /// Block aus Rohdaten (interleaved s*NUM_MICS + ch, pcm24 << 8) ab Sample idx ablegen
    void encodeRawBlock(const int32_t* interleaved, uint32_t idx, uint32_t n);
};
static_assert(HOP_SAMPLES % DMA_BLOCK_SAMPLES == 0, "114: Hop = ganze Zahl von DMA-Blöcken");
static_assert((sizeof(int16_t) * (HOP_SAMPLES + MIC_ROW_PAD)) % 1024 != 0, "114: Kanalabstand kein Vielfaches von 1 KB (Befund 42)");

class Microphone_Array_114 {
public:
    static Microphone_Array_114& instance();

    // --- Geometrie ---------------------------------------------------
    const Vec3& position(uint32_t mic) const { return pos_[mic]; }
    const Vec3* positions() const { return pos_; }

    // --- Schreibseite (116, ISR-Kontext) -----------------------------
    /// Block von DMA_BLOCK_SAMPLES Samples je Mikrofon, interleaved (s*M + ch),
    /// 24-bit PCM linksbündig in int32 (pcm24 << 8, siehe PCM_RAW_FULL_SCALE).
    /// Schließt bei vollem Frame ab und wechselt Puffer.
    void pushBlock(const int32_t* interleaved, uint32_t samplesPerMic, uint64_t time_utc_us);

    // --- Leseseite (118/122, Task-Kontext) ---------------------------
    MicFrame* acquireReadable();     // READY -> READING, nullptr wenn keiner
    void      release(MicFrame* f);  // READING -> FREE
    const MicFrame* latestFrame() const { return latest_; }

    // --- Statistik ---------------------------------------------------
    uint32_t droppedFrames() const { return dropped_; }

private:
    Microphone_Array_114();
    MicFrame* acquireFree();         // FREE -> WRITING (ISR-sicher: ohne Mutex)

    Vec3       pos_[NUM_MICS];
    MicFrame*  active_   = nullptr;  // aktueller Schreibpuffer
    MicFrame*  latest_   = nullptr;  // zuletzt fertiggestellter Frame
    uint32_t   nextId_   = 0;
    uint32_t   dropped_  = 0;
    bool       dropping_ = false;    // Befund 27: seit dem letzten geschriebenen Block wird verworfen
    osMutexId_t mutex_   = nullptr;  // schützt Leseseite

    static MicFrame frames_[NUM_MIC_FRAMES];
};

} // namespace sds110
