/*
 * USBTask.cpp  (Infrastructure/Tasks)
 */
#include "USBTask.hpp"
#include "Infrastructure/Utils/TimeBase.hpp"
#include "Infrastructure/Utils/UtcClock.hpp"
#include "Infrastructure/Utils/SoundSpeed.hpp"
#include "Infrastructure/Utils/FeedbackCodec.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"
#include "Infrastructure/Utils/CommandAssembler.hpp"
#include "Harness/SimScenario.hpp"
#include <cstring>
#include <initializer_list>

// von usbd_cdc_if.c referenziert
volatile uint32_t usb_debug_counter = 0;

extern "C" void USBTask_OnReceive(uint8_t* buf, uint32_t len)
{
    sds110::USBTask::onUsbReceiveISR(buf, len);
}

namespace sds110 {

USBTask* volatile USBTask::active_ = nullptr;

// delayMs wird nicht benutzt (waitForWork blockiert auf der Queue)
USBTask::USBTask() : TaskBase(4096, 0, osPriorityNormal)
{
    // Queue im Konstruktor (main, nach osKernelInitialize), nicht erst im Task:
    // so geht kein Kommando verloren, das vor dem ersten Task-Lauf eintrifft.
    rxQueue_ = xQueueCreate(RX_QUEUE_LEN, sizeof(RxChunk));
    configASSERT(rxQueue_ != nullptr);
    active_ = this;
}

void USBTask::onUsbReceiveISR(const uint8_t* buf, uint32_t len)
{
    USBTask* self = active_;
    if (self == nullptr) return;                     // USBTask nicht gestartet

    // ganzes Paket weiterreichen: es kann mehrere Kommandos oder den Teil eines Kommandos
    // enthalten (Befund 32), zerlegt wird im Task (CommandAssembler)
    RxChunk c;
    c.rxUs = TimeBase::nowUs();                       // Empfangszeit für Typ 7 (UTC)
    c.len  = static_cast<uint8_t>((len > MAX_LENGTH) ? MAX_LENGTH : len);
    memcpy(c.data, buf, c.len);
    BaseType_t hpw = pdFALSE;
    if (xQueueSendFromISR(self->rxQueue_, &c, &hpw) != pdPASS)
        ++self->rxDropped_;
    portYIELD_FROM_ISR(hpw);
}

void USBTask::waitForWork()
{
    // schläft, bis ein Kommando kommt – keine CPU-Last im Leerlauf
    if (xQueueReceive(rxQueue_, &rx_, pdMS_TO_TICKS(IDLE_RESET_MS)) == pdTRUE) {
        rxValid_ = true;
        return;
    }
    // IDLE_RESET_MS ohne Kommando: angezeigte Bearbeitungszeit auf 0,
    // danach wieder ohne Timeout warten (Zähler bleibt = Aufwachvorgänge)
    dm_.setTaskStats(TaskId::Usb, freeStackBytes_, 0.0f, loopNr_);
    rxValid_ = (xQueueReceive(rxQueue_, &rx_, portMAX_DELAY) == pdTRUE);
}

void USBTask::runOnce()
{
    DWTTimer& dwt = DWTTimer::instance();
    const uint32_t t0 = dwt.cycles();
    if (rxValid_) handleChunk(rx_);
    // weitere, inzwischen eingetroffene Pakete gleich mit abarbeiten
    RxChunk c;
    while (xQueueReceive(rxQueue_, &c, 0) == pdTRUE)
        handleChunk(c);
    // eigene Messung statt reportStats(): execTimeCycles_/loopNr_ setzt TaskBase
    // erst nach runOnce(), das wäre die Zeit des vorigen Kommandos
    dm_.setTaskStats(TaskId::Usb, freeStackBytes_, cyclesToMs(dwt.cycles() - t0), loopNr_ + 1);
}

void USBTask::handleChunk(const RxChunk& c)
{
    asm_.push(c.data, c.len, c.rxUs);
    const uint8_t* p = nullptr;
    uint32_t n = 0;
    for (;;) {
        const CommandAssembler::Result r = asm_.next(p, n);
        if (r == CommandAssembler::Result::None) break;
        if (r == CommandAssembler::Result::Error) { handleErrorBytes(p, n); continue; }
        uint8_t cmd[MAX_LENGTH] = {};                // Handler lesen feste Offsets (bis 16 bzw. 52)
        memcpy(cmd, p, n);
        cmdRxUs_ = c.rxUs;
        handle(cmd);
    }
}

void USBTask::handle(const uint8_t* rx)
{
    if (!hasMagic(rx)) { handleError(rx); return; }
    switch (rx[4]) {
        case 1:  handleTimeSync(rx);   break;
        case 2:  handleModeChange(rx); break;
        case 3:  handleSimulation(rx); break;
        case 5:  handleSetUnitId(rx);  break;
        case 6:  handleSrpReference(rx); break;
        case 7:  handleSync(rx);       break;
        case 8:  handleFeedback(rx);   break;
        case 9:  handleAzimuthOffset(rx); break;
        case 10: handlePosition(rx);   break;
        default: handleError(rx);      break;
    }
}

void USBTask::resetCounters()
{
    for (TaskId t : { TaskId::Proc120, TaskId::Lcd, TaskId::Mic })
        dm_.setTaskStats(t, 0, 0.0f, 0);
}

void USBTask::handleTimeSync(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_CMD_LENGTH) { handleError(rx); return; }
    dm_.setSyncTimeDifference(payloadU32(rx));
    // entfernt: dm_.setId(rx[4]) – überschrieb die Unit-ID mit dem Kommandotyp (1)
}

void USBTask::handleModeChange(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_CMD_LENGTH) { handleError(rx); return; }
    const SDS_Mode mode = static_cast<SDS_Mode>(payloadU32(rx));
    if (dm_.getMode() != mode) {
        dm_.setMode(mode);
        resetCounters();
        // entfernt: dm_.setId(rx[0]) – überschrieb die Unit-ID mit 0xDE (Magic)
    }
}

void USBTask::handleSimulation(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_CMD_LENGTH) { handleError(rx); return; }
    const uint32_t v = payloadU32(rx);                  // 0 Mikrofone, 1 Standard, 2 + k Szenario k
    if (!simCommandValid(v)) { handleError(rx); return; }
    dm_.setSimulation(v);
    resetCounters();
    // entfernt: dm_.setId(rx[0]) – überschrieb die Unit-ID mit 0xDE (Magic)
}

void USBTask::handleSetUnitId(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_CMD_LENGTH) { handleError(rx); return; }
    dm_.setId(static_cast<uint16_t>(payloadU32(rx) & 0xFFFF));
}

void USBTask::handleSrpReference(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_CMD_LENGTH) { handleError(rx); return; }
    dm_.setSrpReference(payloadU32(rx) != 0);
}

void USBTask::handleSync(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_SYNC_CMD_LENGTH) { handleError(rx); return; }
    // UTC: 0 = keine Zeit (nur Temperatur); sonst plausibel (ab 2020), sonst Fehler
    const uint64_t utc = payloadU64(rx);
    UtcOffset o;
    const bool utcOk = utc == 0 || UtcClock::fromSync(utc, cmdRxUs_, TimeSource::PcUtc, o);
    if (utc != 0 && utcOk) dm_.setUtcOffset(o);
    // Temperatur: unbekannt oder außerhalb −40…+60 °C -> letzte Temperatur bleibt
    const int16_t centi = static_cast<int16_t>((rx[16] << 8) | rx[17]);
    float tC;
    const bool tempOk = SoundSpeed::decode(centi, tC);
    if (tempOk) dm_.setAirTemperature(tC);
    if (!utcOk || (!tempOk && centi != SoundSpeed::TEMP_UNKNOWN)) handleError(rx);
}

void USBTask::handleFeedback(const uint8_t* rx)
{
    TrackingFeedback fb;
    bool posValid = false;
    if (!FeedbackCodec::decode(rx, msgLen(rx), fb, posValid)) { handleError(rx); return; }
    dm_.setFeedback(fb, posValid, osKernelGetTickCount());
}

void USBTask::handleAzimuthOffset(const uint8_t* rx)
{
    if (msgLen(rx) != SDS_CMD_LENGTH) { handleError(rx); return; }
    float deg = 0.0f;
    if (!Azimuth::offsetFromCenti(static_cast<int32_t>(payloadU32(rx)), deg)) { handleError(rx); return; }
    dm_.setAzimuthOffset(deg);
}

void USBTask::handlePosition(const uint8_t* rx)
{
    LocalPosition p;
    if (!LocalPositionCodec::decode(rx, msgLen(rx), p)) { handleError(rx); return; }
    dm_.setPosition(p);
    USBDriver::sendPosition(dm_.getPosition(), dm_.getId());                  // Bestätigung (Id 6)
}

void USBTask::handleError(const uint8_t* rx)
{
    dm_.setErrorFlag(1);
    dm_.setErrorBuffer(rx, 16);
    resetCounters();
    dm_.setErrorCount(30);   // ~10 s Anzeige
}

void USBTask::handleErrorBytes(const uint8_t* p, uint32_t n)
{
    uint8_t b[16] = {};                              // Anzeige: die ersten 16 verworfenen Bytes
    memcpy(b, p, (n < sizeof(b)) ? n : sizeof(b));
    handleError(b);
}

bool USBTask::hasMagic(const uint8_t* rx)
{
    return rx[0] == 0xDE && rx[1] == 0xAD && rx[2] == 0xBE && rx[3] == 0xEF;
}

} // namespace sds110
