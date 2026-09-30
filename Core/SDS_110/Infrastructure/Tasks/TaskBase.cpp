/*
 * TaskBase.cpp  (Infrastructure/Tasks)
 */
#include "TaskBase.hpp"
#include "FreeRTOS.h"
#include "task.h"       // configASSERT -> taskDISABLE_INTERRUPTS

namespace sds110 {

TaskBase::TaskBase(uint32_t stackSize, uint32_t delayMs, osPriority_t prio)
    : stackSize_(stackSize), delayMs_(delayMs), priority_(prio) {}

bool TaskBase::start()
{
    if (taskHandle_ != nullptr) return true;
    osThreadAttr_t attr{};
    attr.stack_size = stackSize_;
    attr.priority   = priority_;
    taskHandle_ = osThreadNew(threadEntry, this, &attr);
    // NULL = FreeRTOS-Heap (configTOTAL_HEAP_SIZE) zu klein; wie TaskTimerBase anhalten
    configASSERT(taskHandle_ != nullptr);
    return taskHandle_ != nullptr;
}

void TaskBase::threadEntry(void* argument)
{
    TaskBase* self = static_cast<TaskBase*>(argument);
    DWTTimer& dwt = DWTTimer::instance();

    self->onStart();
    self->lastRunCycles_ = dwt.cycles();
    for (;;) {
        self->waitForWork();                       // Delay oder Ereignis
        const uint32_t t0 = dwt.cycles();
        self->jitterCycles_  = t0 - self->lastRunCycles_;
        self->lastRunCycles_ = t0;
        self->runOnce();
        self->execTimeCycles_ = dwt.cycles() - t0;
        self->freeStackBytes_ = osThreadGetStackSpace(self->taskHandle_);
        ++self->loopNr_;
    }
}

} // namespace sds110
