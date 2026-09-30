/*
 * TaskTimerBase.hpp  (Infrastructure/Timer)
 * FreeRTOS-Task, der von einem HardwareTimer per Task-Notification getaktet wird.
 * Ablauf: Konstruktor (nur Parameter) -> attachTimer() -> start() (Task anlegen + Timer an)
 */
#pragma once
#include <atomic>
#include <cstdint>
#include "FreeRTOS.h"
#include "task.h"
#include "HardwareTimer.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"   // TaskId, TaskStats

namespace sds110 {

class TaskTimerBase
{
public:
    void attachTimer(HardwareTimer* timer) { timer_ = timer; }
    bool start();
    void stop();

    // Overrun = Timer hat erneut ausgelöst, bevor onTask() fertig war
    bool     isOverrun()       const { return overrunFlag_.load(); }   // sticky
    void     clearOverrun()          { overrunFlag_.store(false); }
    uint32_t overrunCount()    const { return overrunCount_.load(); }

    // Zeitbudget je Zyklus in µs (Default: volle Periode)
    float    getAllowedTimeUs() const { return allowedTimeUs_; }
    void     setAllowedTimeUs(float us) { allowedTimeUs_ = us; }
    bool     isBudgetExceeded() const { return budgetExceeded_.load(); }  // sticky

    float    lastExecUs()  const;
    float    maxExecUs()   const;
    uint32_t loopCount()   const { return loopNr_; }
    uint32_t freeStackBytes() const;

    TaskHandle_t handle() const { return taskHandle_; }

protected:
    TaskTimerBase(const char* name, uint32_t stackBytes, UBaseType_t priority);
    virtual ~TaskTimerBase() = default;

    virtual void onTask() = 0;

    /// Nach jedem Zyklus Laufzeit/Stack/Zähler unter dieser TaskId in SDS_Data ablegen
    void setStatsId(TaskId id) { statsId_ = id; reportStats_ = true; }

private:
    static void taskEntry(void* arg);
    void taskLoop();
    void onTimerISR();

    const char*   name_;
    uint32_t      stackBytes_;
    UBaseType_t   priority_;

    HardwareTimer* timer_      = nullptr;
    TaskHandle_t   taskHandle_ = nullptr;
    float          allowedTimeUs_ = 0.0f;
    uint32_t       allowedCycles_ = 0;
    TaskId         statsId_       = TaskId::Count;
    bool           reportStats_   = false;

    std::atomic<bool>     taskRunning_{false};
    std::atomic<bool>     overrunFlag_{false};
    std::atomic<bool>     budgetExceeded_{false};
    std::atomic<uint32_t> overrunCount_{0};

    volatile uint32_t lastExecCycles_ = 0;
    volatile uint32_t maxExecCycles_  = 0;
    volatile uint32_t loopNr_         = 0;
};

} // namespace sds110
