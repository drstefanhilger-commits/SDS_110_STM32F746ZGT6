/*
 * Sampling_Circuitry_116.hpp
 *
 * Synchrone Abtastschaltung 116 (Patent, Abschnitt 1, FIG. 2):
 *  ADAU7118 (PDM -> TDM-8, 32-bit Slots, 48 kHz) -> SAI (Master RX) -> DMA
 *  Ping-Pong -> Demux -> Microphone_Array_114::pushBlock().
 *
 * Migration aus SDS (Vereinigung der beiden Pfade):
 *  - adua7118Driver.c : Registermap, Enable-Pin PE3, I2C-Schreibsequenz
 *  - sai.c            : SAI Master-RX TDM-8 Konfiguration (FrameLength 256)
 *  - adau7118.c       : DMA-Half/Complete-Callbacks, Demux interleaved -> Kanal
 *  - dma.c            : DMA-IRQ-Freigabe
 *  Entfallen: TDM_Parser + SDS_RingBuffer (1 Frame à 8 Samples je IRQ) –
 *             ersetzt durch Blockübernahme von DMA_BLOCK_SAMPLES je Halbpuffer.
 *
 * Hardware-Handles (hsai, hi2c) kommen aus main.c / CubeMX.
 * Zeitreferenz: TimeBase::nowUs() (DWT, µs seit Start) minus Blockdauer -> Zeit des ersten
 * Samples im Block. Für Inter-Unit-Sync (Patent: GNSS / PTP, 10 µs) fehlt noch der UTC-Bezug.
 */
#pragma once
#include <cstdint>
#include "stm32f7xx_hal.h"
#include "SDS_110_Config.hpp"
#include "Microphone_Array_114.hpp"

namespace sds110 {

class Sampling_Circuitry_116 {
public:
    static Sampling_Circuitry_116& instance();

    /// Handles binden; Codec über I2C konfigurieren; SAI initialisieren
    bool init(SAI_HandleTypeDef* hsai, I2C_HandleTypeDef* hi2c);
    bool start();
    void stop();
    bool running() const { return running_; }
    /// SAI hat einen RX-DMA (HAL_SAI_MspInit). Ohne DMA würde HAL_SAI_Receive_DMA
    /// hdmarx = NULL dereferenzieren -> HardFault (Blocker 2).
    bool dmaReady() const { return hsai_ && hsai_->hdmarx; }

    /// aus HAL_SAI_RxHalfCpltCallback / HAL_SAI_RxCpltCallback
    void onRxHalf();
    void onRxComplete();
    void onError();

    uint32_t errorCount() const { return errors_; }

    /// Aufruf aus dem DMA-Interrupt, sobald 114 einen Hop fertiggestellt hat (weckt den
    /// ProcessingTask). Hier statt in 114, weil 114 in die Merkmalsversion eingeht.
    using HopReadyHook = void (*)(void* ctx);
    void setHopReadyHook(HopReadyHook h, void* ctx) { hookCtx_ = ctx; hook_ = h; }
    /// Ist-Abtastrate aus SAI-Kerneltakt und MCKDIV (nach init(), sonst 0)
    float sampleRateHz() const { return fsHz_; }

private:
    Sampling_Circuitry_116() = default;
    bool configureCodec();
    bool writeReg(uint8_t reg, uint8_t val);
    bool configureSaiClock();
    bool configureSai();
    void enablePin(bool on);
    uint64_t firstSampleUs() const;   // Zeit des ersten Samples des gerade fertigen Blocks
    void     notifyIfHopReady();       // nach pushBlock(): neuer Hop in 114 -> hook_

    SAI_HandleTypeDef* hsai_ = nullptr;
    I2C_HandleTypeDef* hi2c_ = nullptr;
    bool     running_ = false;
    uint32_t errors_  = 0;
    float    fsHz_    = 0.0f;
    uint32_t blockUs_ = DMA_BLOCK_SAMPLES * 1000000U / SAMPLE_RATE_HZ;   // nach init() aus Ist-Fs
    Microphone_Array_114& array_ = Microphone_Array_114::instance();
    HopReadyHook   hook_    = nullptr;
    void*          hookCtx_ = nullptr;
    const MicFrame* lastHop_ = nullptr;
    uint32_t       lastHopId_ = 0;

    static constexpr uint32_t HALF_WORDS = DMA_BLOCK_SAMPLES * NUM_MICS;
    static int32_t dmaBuffer_[2 * HALF_WORDS];   // Ping-Pong, interleaved TDM
};

} // namespace sds110
