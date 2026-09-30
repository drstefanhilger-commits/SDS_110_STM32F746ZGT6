#include "TaskTimerBase.hpp"
#include "Infrastructure/Utils/DWT.hpp"

namespace sds110 {

TaskTimerBase::TaskTimerBase(const char* name, uint32_t stackBytes, UBaseType_t priority)
    : name_(name), stackBytes_(stackBytes), priority_(priority)
{
    // Kein xTaskCreate hier: der Task könnte sonst laufen, bevor die
    // abgeleitete Klasse fertig konstruiert ist (pure virtual call).
    (void)DWTTimer::instance();          // stellt sicher, dass CYCCNT läuft
}

bool TaskTimerBase::start()
{
    if (timer_ == nullptr) return false;

    if (taskHandle_ == nullptr) {
        const BaseType_t ok = xTaskCreate(taskEntry, name_,
                                          static_cast<configSTACK_DEPTH_TYPE>(stackBytes_ / sizeof(StackType_t)),
                                          this, priority_, &taskHandle_);
        configASSERT(ok == pdPASS);
        if (ok != pdPASS) { taskHandle_ = nullptr; return false; }
    }

    // Budget: Default = eine Timer-Periode
    const float periodUs = 1.0e6f / timer_->getRateHz();
    if (allowedTimeUs_ <= 0.0f || allowedTimeUs_ > periodUs) allowedTimeUs_ = periodUs;
    allowedCycles_ = static_cast<uint32_t>(allowedTimeUs_ * (SystemCoreClock / 1.0e6f));

    overrunFlag_.store(false);
    budgetExceeded_.store(false);
    overrunCount_.store(0);
    maxExecCycles_ = 0;

    // Callback erst setzen, wenn taskHandle_ gültig ist
    timer_->setCallback([this]() { onTimerISR(); });
    timer_->start();
    return true;
}

void TaskTimerBase::stop()
{
    if (timer_) timer_->stop();
}

void TaskTimerBase::onTimerISR()
{
    if (taskHandle_ == nullptr) return;

    if (taskRunning_.load()) {           // Vorgänger-Zyklus noch aktiv
        overrunFlag_.store(true);
        overrunCount_.fetch_add(1);
    }

    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(taskHandle_, &woken);
    portYIELD_FROM_ISR(woken);
}

void TaskTimerBase::taskEntry(void* arg)
{
    static_cast<TaskTimerBase*>(arg)->taskLoop();
}

void TaskTimerBase::taskLoop()
{
    for (;;)
    {
        const uint32_t pending = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (pending > 1) {                // Ticks verschluckt, während Task nicht lief
            overrunFlag_.store(true);
            overrunCount_.fetch_add(pending - 1);
        }

        taskRunning_.store(true);
        const uint32_t t0 = DWT->CYCCNT;
        onTask();
        const uint32_t dt = DWT->CYCCNT - t0;   // Überlauf-sicher (unsigned)
        taskRunning_.store(false);

        lastExecCycles_ = dt;
        if (dt > maxExecCycles_) maxExecCycles_ = dt;
        if (dt > allowedCycles_) budgetExceeded_.store(true);
        ++loopNr_;

        // aktuelle Verarbeitungszeit (dieser Zyklus) in ms -> SDS_Data -> LCD "Log"-Zeile
        if (reportStats_)
            SDS_Data::instance().setTaskStats(statsId_, freeStackBytes(),
                                              DWTTimer::instance().cyclesToUs(dt) / 1000.0f,
                                              loopNr_);
    }
}

float TaskTimerBase::lastExecUs() const { return DWTTimer::instance().cyclesToUs(lastExecCycles_); }
float TaskTimerBase::maxExecUs()  const { return DWTTimer::instance().cyclesToUs(maxExecCycles_); }

uint32_t TaskTimerBase::freeStackBytes() const
{
    return taskHandle_ ? uxTaskGetStackHighWaterMark(taskHandle_) * sizeof(StackType_t) : 0;
}

} // namespace sds110
