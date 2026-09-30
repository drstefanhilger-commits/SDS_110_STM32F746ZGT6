/*
 * LoggerTask.hpp  (Infrastructure/Tasks)
 * Ersetzt LoggerTask: leert den Logger-Ringpuffer und sendet ihn als
 * Message id 99 über USB. Getaktet durch TIM7 über TaskTimerBase.
 * STM32F746ZGT6-Board (kein LCD): bedient zusätzlich die Status-LEDs
 *   LED_RUN   (PG2) Herzschlag 1 Hz, solange die Tasks laufen
 *   LED_COMM  (PG3) wechselt bei USB-Empfang (usb_debug_counter) im 10-Hz-Takt
 *   LED_ERROR (PG4) an bei ML-Fehler (Init/Lauf); fatale Fehler: SDS110_Fatal
 */
#pragma once
#include <cstdint>
#include "Infrastructure/Timer/TaskTimerBase.hpp"
#include "Infrastructure/Utils/Logger.hpp"

namespace sds110 {

class LoggerTask : public TaskTimerBase
{
public:
    static LoggerTask& instance()
    {
        static LoggerTask inst;
        return inst;
    }

    static constexpr float    kRateHz        = 10.0f;  // Takt des Loggers
    static constexpr uint32_t kMaxMsgPerTick = 8;      // max. USB-Pakete je Takt (8 x 128 B)
    static constexpr uint32_t kMsgId         = 99;     // wie bisher
    static constexpr uint32_t kPositionEveryTicks = 10; // Standort (Id 6) jede Sekunde

protected:
    void onTask() override;

private:
    LoggerTask();
    LoggerTask(const LoggerTask&) = delete;
    LoggerTask& operator=(const LoggerTask&) = delete;

    Logger&  logger_ = Logger::instance();
    uint8_t  buf_[128];
    int      pending_ = 0;       // Bytes in buf_, die noch nicht gesendet wurden
    uint32_t droppedTicks_ = 0;  // Takte mit Overrun (für Diagnose)
    uint32_t positionTicks_ = 0;
    uint32_t ledTicks_ = 0;
    uint32_t lastUsbRx_ = 0;
    void updateLeds();
};

} // namespace sds110
