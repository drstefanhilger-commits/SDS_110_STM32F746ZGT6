/*
 * TaskBase.hpp  (Infrastructure/Tasks)
 * CMSIS-RTOS2-Task: onStart() -> Schleife { waitForWork(); runOnce(); }.
 * waitForWork() ist standardmäßig osDelay(delayMs) (periodisch); ereignis-
 * getriebene Tasks überschreiben es und blockieren z. B. auf einer Queue.
 * Gemessen wird nur runOnce(), die Wartezeit zählt nicht zur Laufzeit.
 * Unverändert aus SDS/Tasks/TaskBase, Namespace sds110; Monitoring-Werte
 * werden per reportStats(TaskId) in SDS_Data geschrieben.
 */
#pragma once
#include "cmsis_os2.h"
#include <cstdint>
#include "Infrastructure/Utils/DWT.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"

namespace sds110 {

class TaskBase {
public:
    bool start();                               // false: Task nicht angelegt (Heap)
    osThreadId_t handle() const { return taskHandle_; }

protected:
    TaskBase(uint32_t stackSize = 4096, uint32_t delayMs = 50, osPriority_t prio = osPriorityNormal);
    virtual ~TaskBase() = default;

    virtual void runOnce() = 0;
    virtual void waitForWork() { osDelay(delayMs_); }   // vor jeder Messung
    virtual void onStart() {}
    virtual void onExit() {}

    /// Monitoring in SDS_Data ablegen (am Ende von runOnce() aufrufen)
    void reportStats(TaskId id) const
    {
        SDS_Data::instance().setTaskStats(id, freeStackBytes_, cyclesToMs(execTimeCycles_), loopNr_);
    }
    static float cyclesToMs(uint32_t cycles) { return cycles / (SystemCoreClock / 1000.0f); }

    osThreadId_t taskHandle_ = nullptr;
    uint32_t     stackSize_;
    uint32_t     delayMs_;
    osPriority_t priority_;

    uint32_t lastRunCycles_  = 0;
    uint32_t execTimeCycles_ = 0;
    uint32_t jitterCycles_   = 0;
    uint32_t freeStackBytes_ = 0;
    uint32_t loopNr_         = 0;

private:
    static void threadEntry(void* argument);
};

} // namespace sds110
