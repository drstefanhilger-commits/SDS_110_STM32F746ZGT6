/*
 * HardwareTimer.hpp  (Infrastructure/Timer)
 * Basic-/General-Purpose-Timer als periodische Interrupt-Quelle.
 * PSC/ARR werden aus Timer-Takt und Rate berechnet (16-Bit-ARR-tauglich).
 */
#pragma once
#include <cstdint>
#include <functional>
#include "stm32f7xx_hal.h"

namespace sds110 {

class HardwareTimer
{
public:
    HardwareTimer(TIM_TypeDef* tim, IRQn_Type irq, uint32_t irqPrio = 6);

    /// timerClockHz = 0 -> Timer-Takt wird aus der RCC-Konfiguration ermittelt
    bool init(float rateHz, uint32_t timerClockHz = 0);
    void start();
    void stop();

    void setCallback(std::function<void()> cb) { callback_ = std::move(cb); }
    void handleInterrupt();                        // aus TIMx_IRQHandler aufrufen

    float getRateHz() const { return rateHz_; }    // tatsächlich eingestellte Rate

private:
    static uint32_t timerClockFromRcc(TIM_TypeDef* tim);
    static void     enableClock(TIM_TypeDef* tim);
    static bool     is32Bit(TIM_TypeDef* tim) { return tim == TIM2 || tim == TIM5; }

    TIM_TypeDef*          tim_;
    IRQn_Type             irq_;
    uint32_t              irqPrio_;
    float                 rateHz_ = 0.0f;
    std::function<void()> callback_;
};

} // namespace sds110
