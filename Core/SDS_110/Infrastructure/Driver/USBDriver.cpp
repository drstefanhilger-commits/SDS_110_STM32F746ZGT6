/*
 * USBDriver.cpp  (Infrastructure/Driver)
 */
#include "USBDriver.hpp"
#include "Infrastructure/Utils/crc32.hpp"
#include "usbd_cdc_if.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include <cstring>

extern "C" USBD_HandleTypeDef hUsbDeviceFS;

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

// ---------------------------------------------------------------- Ringpuffer
bool USBDriver::linkReady()
{
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != nullptr;
}

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
    auto* hcdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceFS.pClassData);
    if (hcdc->TxState != 0U) return;                    // Transfer läuft, ISR lädt nach

    uint32_t n = head_ - tail_;
    if (n == 0) return;
    if (n > TX_CHUNK) n = TX_CHUNK;

    const uint32_t pos   = tail_ & (TX_RING_SIZE - 1);
    const uint32_t first = (n < TX_RING_SIZE - pos) ? n : TX_RING_SIZE - pos;
    std::memcpy(txBuf_, &ring_[pos], first);
    std::memcpy(txBuf_ + first, &ring_[0], n - first);
    tail_ = tail_ + n;

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txBuf_, n);
    USBD_CDC_TransmitPacket(&hUsbDeviceFS);
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
