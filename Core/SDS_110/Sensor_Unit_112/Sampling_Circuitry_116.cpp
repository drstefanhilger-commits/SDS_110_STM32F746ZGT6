/*
 * Sampling_Circuitry_116.cpp
 */
#include "Sampling_Circuitry_116.hpp"
#include "ADAU7118_Registers.hpp"
#include "cmsis_os2.h"
#include "Infrastructure/Utils/TimeBase.hpp"

namespace sds110 {

// DMA-Puffer in SRAM2 (nicht cachebar, MPU-Region 0). Die Invalidierung in onRx*() bleibt als
// Absicherung, falls der Puffer einmal in gecachten Speicher wandert (dafür 32-Byte-Alignment).
SDS110_DMA_SECTION alignas(32) int32_t Sampling_Circuitry_116::dmaBuffer_[2 * HALF_WORDS];

Sampling_Circuitry_116& Sampling_Circuitry_116::instance()
{
    static Sampling_Circuitry_116 inst;
    return inst;
}

// ---------------------------------------------------------------- init
bool Sampling_Circuitry_116::init(SAI_HandleTypeDef* hsai, I2C_HandleTypeDef* hi2c)
{
    hsai_ = hsai;
    hi2c_ = hi2c;
    if (!hsai_ || !hi2c_) return false;

    enablePin(true);
    HAL_Delay(10);

    if (!configureCodec()) { ++errors_; return false; }
    if (!configureSai())   { ++errors_; return false; }
    return true;
}

void Sampling_Circuitry_116::enablePin(bool on)
{
    // Enable-Pin PE3 (aus adua7118Driver.c)
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef g{};
    g.Pin   = GPIO_PIN_3;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

bool Sampling_Circuitry_116::writeReg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(hi2c_, static_cast<uint16_t>(ADAU7118_I2C_ADDR_7B << 1),
                             reg, I2C_MEMADD_SIZE_8BIT, &val, 1,
                             ADAU7118_I2C_TIMEOUT_MS) == HAL_OK;
}

bool Sampling_Circuitry_116::configureCodec()
{
    for (const auto& rv : adau7118::INIT_SEQUENCE)
        if (!writeReg(rv.reg, rv.val)) return false;
    return true;
}

// SAI-Kerneltakt aus PLLI2S (Werte und Begründung: SAI_PLLI2S_* in SDS_110_Config.hpp).
// Bewusst hier und nicht in HAL_SAI_MspInit(): die ist CubeMX-generiert und stellt SAI1 auf
// PLLSAI (192 MHz -> 46,875 kHz bzw. 53,57 kHz). STM32F746ZGT6-Board: SAI1 Block A (PE4..PE6),
// PLLI2S ist sonst unbenutzt (USB-48 MHz kommt aus PLLQ); P und R übernimmt die HAL aus dem Register.
bool Sampling_Circuitry_116::configureSaiClock()
{
    const bool sai1 = (hsai_->Instance == SAI1_Block_A || hsai_->Instance == SAI1_Block_B);
    RCC_PeriphCLKInitTypeDef clk{};
    clk.PeriphClockSelection = sai1 ? RCC_PERIPHCLK_SAI1 : RCC_PERIPHCLK_SAI2;
    clk.Sai1ClockSelection   = RCC_SAI1CLKSOURCE_PLLI2S;
    clk.Sai2ClockSelection   = RCC_SAI2CLKSOURCE_PLLI2S;
    clk.PLLI2S.PLLI2SN       = SAI_PLLI2S_N;
    clk.PLLI2S.PLLI2SQ       = SAI_PLLI2S_Q;
    clk.PLLI2SDivQ           = SAI_PLLI2S_DIVQ;
    return HAL_RCCEx_PeriphCLKConfig(&clk) == HAL_OK;
}

bool Sampling_Circuitry_116::configureSai()
{
    if (!configureSaiClock()) { ++errors_; return false; }
    const uint32_t periph = (hsai_->Instance == SAI1_Block_A || hsai_->Instance == SAI1_Block_B)
                            ? RCC_PERIPHCLK_SAI1 : RCC_PERIPHCLK_SAI2;
    const uint32_t saiClk = HAL_RCCEx_GetPeriphCLKFreq(periph);
    if (saiClk == 0U) { ++errors_; return false; }    // sonst teilt HAL_SAI_Init durch 0

    // Aus sai.c: Master RX, PCM long, TDM-8 x 32 bit (Frame 256 bit)
    SAI_HandleTypeDef& h = *hsai_;
    h.Init.AudioMode      = SAI_MODEMASTER_RX;
    h.Init.Synchro        = SAI_ASYNCHRONOUS;
    h.Init.OutputDrive    = SAI_OUTPUTDRIVE_DISABLE;
    h.Init.NoDivider      = SAI_MASTERDIVIDER_ENABLE;
    h.Init.FIFOThreshold  = SAI_FIFOTHRESHOLD_1QF;
    h.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
    h.Init.Protocol       = SAI_FREE_PROTOCOL;
    h.Init.DataSize       = SAI_DATASIZE_32;
    h.Init.FirstBit       = SAI_FIRSTBIT_MSB;
    h.Init.ClockStrobing  = SAI_CLOCKSTROBING_FALLINGEDGE;
    h.Init.MonoStereoMode = SAI_STEREOMODE;
    h.Init.CompandingMode = SAI_NOCOMPANDING;

    h.FrameInit.FrameLength       = 32 * NUM_MICS;   // 256
    h.FrameInit.ActiveFrameLength = 32;
    h.FrameInit.FSDefinition      = SAI_FS_STARTFRAME;
    h.FrameInit.FSPolarity        = SAI_FS_ACTIVE_HIGH;
    h.FrameInit.FSOffset          = SAI_FS_FIRSTBIT;

    h.SlotInit.FirstBitOffset = 0;
    h.SlotInit.SlotSize       = SAI_SLOTSIZE_32B;
    h.SlotInit.SlotNumber     = NUM_MICS;
    h.SlotInit.SlotActive     = 0xFF;

    // GPIO/Clock-MSP kommt aus HAL_SAI_MspInit (CubeMX, stm32f7xx_hal_msp.c)
    if (HAL_SAI_Init(&h) != HAL_OK) return false;

    // Ist-Abtastrate: MCLK = SAI_CK / (2·MCKDIV) (MCKDIV 0 -> Teiler 1), MCLK = 256·Fs
    const uint32_t div = (h.Init.Mckdiv == 0U) ? 1U : 2U * h.Init.Mckdiv;
    fsHz_ = static_cast<float>(saiClk) / (static_cast<float>(div) * 256.0f);
    blockUs_ = static_cast<uint32_t>(DMA_BLOCK_SAMPLES * 1.0e6f / fsHz_ + 0.5f);
    const float err = (fsHz_ - static_cast<float>(SAMPLE_RATE_HZ)) / static_cast<float>(SAMPLE_RATE_HZ);
    if (err > SAI_FS_TOLERANCE || err < -SAI_FS_TOLERANCE) { ++errors_; return false; }
    return true;
}

// ---------------------------------------------------------------- run
bool Sampling_Circuitry_116::start()
{
    if (!dmaReady()) { ++errors_; return false; }   // Blocker 2: kein DMA eingerichtet
    if (HAL_SAI_Receive_DMA(hsai_, reinterpret_cast<uint8_t*>(dmaBuffer_),
                            2 * HALF_WORDS) != HAL_OK) {
        ++errors_;
        return false;
    }
    running_ = true;
    return true;
}

void Sampling_Circuitry_116::stop()
{
    if (hsai_) HAL_SAI_DMAStop(hsai_);
    running_ = false;
}

uint64_t Sampling_Circuitry_116::firstSampleUs() const
{
    // Der Halb-/Voll-Interrupt kommt, wenn der Block komplett ist; sein erstes Sample
    // lag eine Blockdauer früher (128 Samples / 47 991 Hz ≈ 2 667 µs)
    return TimeBase::nowUs() - blockUs_;
}

void Sampling_Circuitry_116::onRxHalf()
{
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(dmaBuffer_), sizeof(int32_t) * HALF_WORDS);
    array_.pushBlock(&dmaBuffer_[0], DMA_BLOCK_SAMPLES, firstSampleUs());
    notifyIfHopReady();
}

void Sampling_Circuitry_116::onRxComplete()
{
    SCB_InvalidateDCache_by_Addr(reinterpret_cast<uint32_t*>(&dmaBuffer_[HALF_WORDS]), sizeof(int32_t) * HALF_WORDS);
    array_.pushBlock(&dmaBuffer_[HALF_WORDS], DMA_BLOCK_SAMPLES, firstSampleUs());
    notifyIfHopReady();
}

void Sampling_Circuitry_116::notifyIfHopReady()
{
    const MicFrame* f = array_.latestFrame();
    if (!f || (f == lastHop_ && f->frame_id == lastHopId_)) return;
    lastHop_ = f; lastHopId_ = f->frame_id;
    if (hook_) hook_(hookCtx_);
}

void Sampling_Circuitry_116::onError()
{
    ++errors_;
    // DMA neu starten; HAL_SAI_ErrorCallback hat den Transfer bereits gestoppt
    if (running_) start();
}

} // namespace sds110

// ---------------------------------------------------------------- HAL hooks
// Genau eine Definition pro Projekt – die alten Hooks in adau7118.c
// (HAL_I2S_*) und stm32f7xx_it.c dürfen nicht parallel aktiv sein.
extern "C" {

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef* hsai)
{
    (void)hsai;
    sds110::Sampling_Circuitry_116::instance().onRxHalf();
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef* hsai)
{
    (void)hsai;
    sds110::Sampling_Circuitry_116::instance().onRxComplete();
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef* hsai)
{
    (void)hsai;
    sds110::Sampling_Circuitry_116::instance().onError();
}

} // extern "C"
