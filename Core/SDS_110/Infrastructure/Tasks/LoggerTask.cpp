/*
 * LoggerTask.cpp  (Infrastructure/Tasks)
 */
#include "LoggerTask.hpp"
#include "Infrastructure/Timer/HardwareTimer.hpp"
#include "Infrastructure/Driver/USBDriver.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"
#include "Infrastructure/Tasks/USBTask.hpp"   // usb_debug_counter
#include "Infrastructure/Driver/StatusLed.hpp"  // LEDs je Zielboard
#include "cmsis_os2.h"
#include <cstring>

namespace {
// TIM7: Basic-Timer auf APB1 (108 MHz bei 216 MHz SYSCLK / APB1 DIV4)
sds110::HardwareTimer loggerTimer(TIM7, TIM7_IRQn, 6);
}

extern "C" void TIM7_IRQHandler(void)
{
    loggerTimer.handleInterrupt();
}

namespace sds110 {

LoggerTask::LoggerTask()
    : TaskTimerBase("LoggerTask", 1024 /*Bytes*/, static_cast<UBaseType_t>(osPriorityLow))
{
    led::init();                                  // Discovery: LED1 (PI1); eigenes Board: MX_GPIO_Init
    const bool ok = loggerTimer.init(kRateHz);   // Timer-Takt aus RCC
    configASSERT(ok);
    attachTimer(&loggerTimer);
    setStatsId(TaskId::Logger);                   // Laufzeit erscheint im LCD als "Log"
}

void LoggerTask::onTask()
{
    // Ringpuffer leeren: mehrere Pakete je Takt, damit 10 Hz denselben
    // Durchsatz schaffen wie der alte 5-ms-LoggerTask (128 B je Aufruf).
    for (uint32_t i = 0; i < kMaxMsgPerTick; ++i)
    {
        if (pending_ == 0) {
            pending_ = logger_.read(buf_, sizeof(buf_));
            if (pending_ <= 0) { pending_ = 0; break; }   // Puffer leer
        }

        MessageData data{};
        memcpy(data.b, buf_, static_cast<size_t>(pending_));
        if (!USBDriver::sendMessage(kMsgId, 0, data))
            break;                  // USB belegt -> Paket bleibt in buf_, nächster Takt
        pending_ = 0;
    }

    // Standort (Id 6) jede Sekunde, auch der Grundwert: der PC sieht so, was das Board hat
    if (++positionTicks_ >= kPositionEveryTicks) {
        positionTicks_ = 0;
        SDS_Data& dm = SDS_Data::instance();
        LocalPosition p;
        if (dm.tryGetPosition(p)) USBDriver::sendPosition(p, dm.getId());
    }

    updateLeds();

    if (isOverrun()) {
        ++droppedTicks_;
        clearOverrun();
    }
}

void LoggerTask::updateLeds()
{
    // Herzschlag: 0,5 s an, 0,5 s aus
    if (++ledTicks_ >= static_cast<uint32_t>(kRateHz / 2.0f)) {
        ledTicks_ = 0;
        led::toggleRun();
    }
    const uint32_t rx = usb_debug_counter;
    if (rx != lastUsbRx_) {
        lastUsbRx_ = rx;
        led::toggleComm();
    }
    const SDS_Data& dm = SDS_Data::instance();
    const bool err = dm.getMlInitError() || dm.getMlRunError();
#if !SDS110_BOARD_DISCO
    led::setError(err);                           // Discovery: nur eine LED (Herzschlag)
#else
    (void)err;
#endif
}

} // namespace sds110
