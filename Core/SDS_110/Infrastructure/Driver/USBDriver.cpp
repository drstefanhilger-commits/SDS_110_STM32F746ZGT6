/*
 * USBDriver.cpp  (Infrastructure/Driver)
 */
#include "USBDriver.hpp"
#include "Infrastructure/Utils/crc32.hpp"
#include "usbd_cdc_if.h"
#include "SDS_110_Board.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <cstring>

extern "C" USBD_HandleTypeDef hUsbDeviceFS;

#if SDS110_LINK_UART
// ---------------------------------------------------------------- Transport USART1 (CP2102N)
// STM32F746ZGT6-Board: der PC hängt über den CP2102N an USART1 (SDS_110_Board.h). Senden per
// Interrupt aus txBuf_ (HAL_UART_Transmit_IT), Empfang blockweise bis Leerlauf
// (HAL_UARTEx_ReceiveToIdle_IT) -> USBTask_OnReceive wie bei CDC_Receive_FS.
// USART1-IRQ Priorität 5 (= configMAX_SYSCALL_INTERRUPT_PRIORITY): kick() läuft im ISR oder im
// kritischen Abschnitt, der ihn maskiert. Der IRQ ist in CubeMX NICHT aktiviert (Handler hier).
extern "C" UART_HandleTypeDef huart1;
extern "C" void USBTask_OnReceive(uint8_t* buf, uint32_t len);
extern "C" volatile uint32_t usb_debug_counter;

namespace {
volatile bool s_uartTxBusy = false;
uint8_t s_uartRx[64];                              // ein Block wie ein USB-FS-Paket (USBTask::MAX_LENGTH)
void uartStartRx() { (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, s_uartRx, sizeof(s_uartRx)); }
}
#endif

namespace sds110 {

static_assert((USBDriver::TX_RING_SIZE & (USBDriver::TX_RING_SIZE - 1)) == 0, "TX_RING_SIZE muss Zweierpotenz sein");
static_assert(sizeof(SDS_MsgRead) <= USBDriver::TX_RING_SIZE, "Nachricht größer als Ringpuffer");

// STM32F746ZGT6: Ringpuffer im freien Teil von SRAM2 (.dma_nocache, nicht gecacht) – entlastet das
// knappe interne RAM (kein SDRAM); USB-FS ohne DMA, nur CPU-Zugriffe (memcpy)
SDS110_DMA_SECTION uint8_t USBDriver::ring_[TX_RING_SIZE];
uint8_t  USBDriver::txBuf_[TX_CHUNK];
volatile uint32_t USBDriver::head_      = 0;
volatile uint32_t USBDriver::tail_      = 0;
volatile uint32_t USBDriver::txDropped_ = 0;

#if SDS110_LINK_UART
void USBDriver::startLink()
{
    if (huart1.Init.BaudRate != SDS110_UART_BAUD) {   // CubeMX-Wert abweichend: Baudrate hier setzen
        huart1.Init.BaudRate = SDS110_UART_BAUD;
        (void)HAL_UART_Init(&huart1);
    }
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    uartStartRx();
}

bool USBDriver::linkReady()
{
    return huart1.gState != HAL_UART_STATE_RESET;     // UART ohne Verbindungszustand: immer senden
}
#else
void USBDriver::startLink() {}

// ---------------------------------------------------------------- Ringpuffer
bool USBDriver::linkReady()
{
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != nullptr;
}
#endif

bool USBDriver::enqueue(const uint8_t* buf, uint32_t len)
{
    const uint32_t used = head_ - tail_;
    if (len > TX_RING_SIZE - used) return false;

    const uint32_t pos   = head_ & (TX_RING_SIZE - 1);
    const uint32_t first = (len < TX_RING_SIZE - pos) ? len : TX_RING_SIZE - pos;
    std::memcpy(&ring_[pos], buf, first);
    std::memcpy(&ring_[0], buf + first, len - first);
    head_ = head_ + len;
    return true;
}

// Läuft entweder im OTG-FS-ISR (Priorität 5) oder in einem kritischen Abschnitt,
// der diesen ISR maskiert (configMAX_SYSCALL_INTERRUPT_PRIORITY = 5) – daher ohne Sperre.
void USBDriver::kick()
{
    if (!linkReady()) return;
#if SDS110_LINK_UART
    if (s_uartTxBusy) return;                           // Transfer läuft, TxCplt lädt nach
#else
    auto* hcdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceFS.pClassData);
    if (hcdc->TxState != 0U) return;                    // Transfer läuft, ISR lädt nach
#endif

    uint32_t n = head_ - tail_;
    if (n == 0) return;
    if (n > TX_CHUNK) n = TX_CHUNK;

    const uint32_t pos   = tail_ & (TX_RING_SIZE - 1);
    const uint32_t first = (n < TX_RING_SIZE - pos) ? n : TX_RING_SIZE - pos;
    std::memcpy(txBuf_, &ring_[pos], first);
    std::memcpy(txBuf_ + first, &ring_[0], n - first);
    tail_ = tail_ + n;

#if SDS110_LINK_UART
    s_uartTxBusy = true;
    if (HAL_UART_Transmit_IT(&huart1, txBuf_, static_cast<uint16_t>(n)) != HAL_OK) s_uartTxBusy = false;
#else
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txBuf_, n);
    USBD_CDC_TransmitPacket(&hUsbDeviceFS);
#endif
}

void USBDriver::onTransmitComplete()
{
    kick();
}

bool USBDriver::transmit(const void* buf, uint32_t len, uint32_t waitMs)
{
    const auto* p = static_cast<const uint8_t*>(buf);
    for (uint32_t waited = 0;; ++waited) {
        if (!linkReady()) break;                        // kein Host: nicht puffern (keine Altdaten)

        taskENTER_CRITICAL();
        const bool ok = enqueue(p, len);
        if (ok) kick();
        taskEXIT_CRITICAL();
        if (ok) return true;

        if (waited >= waitMs) break;
        osDelay(1);
    }
    ++txDropped_;
    return false;
}

// ---------------------------------------------------------------- Nachrichten
bool USBDriver::sendDetection(uint32_t timestamp, uint32_t micId, float azimuth, float distance, float confidence,
                              uint32_t waitMs)
{
    SDS_MsgDetect msg;
    msg.timestamp = timestamp;
    msg.mic       = micId;
    msg.azi       = azimuth;
    msg.distance  = distance;
    msg.conf      = confidence;
    msg.crc32     = CRC32::compute_no_crc(msg);
    return transmit(&msg, sizeof(msg), waitMs);
}

bool USBDriver::sendRead(uint32_t timestamp, uint32_t micNr, uint32_t blockNr, uint32_t hopNr, const MicFrame* frame,
                         uint32_t waitMs)
{
    SDS_MsgRead msg;
    msg.timestamp = timestamp;
    msg.micNr     = static_cast<uint8_t>(micNr);
    msg.blockNr   = static_cast<uint8_t>(blockNr);
    msg.hopNr     = static_cast<uint16_t>(hopNr);
    const uint32_t offset = blockNr * SDS_MSG_BUFFER_SIZE;
    constexpr float toPcm24 = static_cast<float>(1 << 23);
    for (uint32_t i = 0; i < SDS_MSG_BUFFER_SIZE; ++i) {
        const uint32_t idx = offset + i;
        if (frame && micNr < NUM_MICS && idx < HOP_SAMPLES)
            msg.data[i] = static_cast<uint32_t>(static_cast<int32_t>(frame->sample(micNr, idx) * toPcm24));   // Blockgleitkomma -> pcm24
        else
            msg.data[i] = 0;
    }
    msg.crc32 = CRC32::compute_no_crc(msg);
    return transmit(&msg, sizeof(msg), waitMs);
}

bool USBDriver::sendLogging(uint32_t timestamp, const uint8_t* src, int len, uint32_t waitMs)
{
#pragma pack(push, 1)
    struct SDS_MsgLog {
        uint32_t magic     = 0xDEADBEEF;
        uint8_t  id        = 3;
        uint8_t  len[3]    = {0x30, 0x00, 0x00};   // 48 Byte
        uint32_t timestamp = 0;
        uint8_t  data[32]  = {};
        uint32_t crc32     = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(SDS_MsgLog) == 48, "SDS_MsgLog must be 48 bytes");
    SDS_MsgLog msg;
    msg.timestamp = timestamp;
    const int n = (len < 32) ? len : 32;
    if (n > 0) memcpy(msg.data, src, n);
    msg.crc32 = CRC32::compute_no_crc(msg);
    return transmit(&msg, sizeof(msg), waitMs);
}

bool USBDriver::sendMessage(uint32_t id, uint32_t timestamp, const MessageData& data, uint32_t waitMs)
{
    Message msg;
    msg.len_id    = ((sizeof(Message) & 0x00FFFFFF) | (id << 24));
    msg.timestamp = timestamp;
    msg.data      = data;
    msg.crc32     = CRC32::compute_no_crc(msg);
    return transmit(&msg, sizeof(msg), waitMs);
}

bool USBDriver::sendPosition(const LocalPosition& pos, uint16_t unit, uint32_t waitMs)
{
    MessageData data{};
    LocalPositionCodec::encodeReport(pos, unit, data.b);
    return sendMessage(POSITION_MSG_ID, 0, data, waitMs);
}

} // namespace sds110

extern "C" void USBDriver_OnTransmitComplete(void)
{
    sds110::USBDriver::onTransmitComplete();
}

#if SDS110_LINK_UART
// ---------------------------------------------------------------- HAL-Hooks USART1
extern "C" {

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart != &huart1) return;
    s_uartTxBusy = false;
    sds110::USBDriver::onTransmitComplete();
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size)
{
    if (huart != &huart1) return;
    if (size > 0) {
        ++usb_debug_counter;                           // LED_COMM (LoggerTask)
        USBTask_OnReceive(s_uartRx, size);
    }
    uartStartRx();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    if (huart != &huart1) return;
    // Überlauf/Rahmenfehler: Empfang neu starten; ein abgebrochener Sendeblock ist verloren
    if (huart->gState == HAL_UART_STATE_READY) s_uartTxBusy = false;
    uartStartRx();
    sds110::USBDriver::onTransmitComplete();
}

} // extern "C"
#endif
