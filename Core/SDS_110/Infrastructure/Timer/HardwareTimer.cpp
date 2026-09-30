#include "HardwareTimer.hpp"

namespace sds110 {

HardwareTimer::HardwareTimer(TIM_TypeDef* tim, IRQn_Type irq, uint32_t irqPrio)
    : tim_(tim), irq_(irq), irqPrio_(irqPrio)
{
}

void HardwareTimer::enableClock(TIM_TypeDef* t)
{
    if      (t == TIM2)  __HAL_RCC_TIM2_CLK_ENABLE();
    else if (t == TIM3)  __HAL_RCC_TIM3_CLK_ENABLE();
    else if (t == TIM4)  __HAL_RCC_TIM4_CLK_ENABLE();
    else if (t == TIM5)  __HAL_RCC_TIM5_CLK_ENABLE();
    else if (t == TIM6)  __HAL_RCC_TIM6_CLK_ENABLE();
    else if (t == TIM7)  __HAL_RCC_TIM7_CLK_ENABLE();
    else if (t == TIM8)  __HAL_RCC_TIM8_CLK_ENABLE();
    else if (t == TIM12) __HAL_RCC_TIM12_CLK_ENABLE();
    else if (t == TIM13) __HAL_RCC_TIM13_CLK_ENABLE();
    else if (t == TIM14) __HAL_RCC_TIM14_CLK_ENABLE();
}

// Timer-Takt = PCLKx, bzw. 2*PCLKx wenn APBx-Prescaler != 1 (RM0385, Clock tree)
uint32_t HardwareTimer::timerClockFromRcc(TIM_TypeDef* t)
{
    const bool apb2 = (t == TIM1 || t == TIM8 || t == TIM9 || t == TIM10 || t == TIM11);
    if (apb2) {
        const uint32_t pclk = HAL_RCC_GetPCLK2Freq();
        return ((RCC->CFGR & RCC_CFGR_PPRE2) == 0) ? pclk : 2u * pclk;
    }
    const uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    return ((RCC->CFGR & RCC_CFGR_PPRE1) == 0) ? pclk : 2u * pclk;
}

bool HardwareTimer::init(float rateHz, uint32_t timerClockHz)
{
    if (rateHz <= 0.0f) return false;
    if (timerClockHz == 0) timerClockHz = timerClockFromRcc(tim_);

    enableClock(tim_);
    tim_->CR1 = 0;                                   // Timer aus

    // Gesamtteiler = Takt / Rate, auf PSC (16 Bit) und ARR (16/32 Bit) verteilen
    const uint64_t ticks  = static_cast<uint64_t>(timerClockHz / rateHz + 0.5f);
    const uint64_t arrMax = is32Bit(tim_) ? 0xFFFFFFFFull : 0xFFFFull;
    uint64_t psc = (ticks + arrMax) / (arrMax + 1);  // ceil(ticks / (arrMax+1))
    if (psc == 0) psc = 1;
    if (psc > 0x10000) return false;                 // Rate zu klein für diesen Timer
    const uint64_t arr = ticks / psc;
    if (arr < 2) return false;                       // Rate zu groß

    tim_->PSC = static_cast<uint32_t>(psc - 1);
    tim_->ARR = static_cast<uint32_t>(arr - 1);
    rateHz_   = static_cast<float>(timerClockHz) / static_cast<float>(psc * arr);

    // PSC übernehmen, ohne dabei einen Update-Interrupt auszulösen
    tim_->CR1 |= TIM_CR1_URS;
    tim_->EGR  = TIM_EGR_UG;
    tim_->SR   = 0;

    tim_->DIER |= TIM_DIER_UIE;

    // Priorität muss >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5) sein,
    // da die ISR FreeRTOS-FromISR-API benutzt
    NVIC_ClearPendingIRQ(irq_);
    NVIC_SetPriority(irq_, irqPrio_);
    NVIC_EnableIRQ(irq_);
    return true;
}

void HardwareTimer::start()
{
    tim_->CNT = 0;
    tim_->SR  = 0;
    tim_->CR1 |= TIM_CR1_CEN;
}

void HardwareTimer::stop()
{
    tim_->CR1 &= ~TIM_CR1_CEN;
}

void HardwareTimer::handleInterrupt()
{
    if ((tim_->SR & TIM_SR_UIF) == 0) return;
    tim_->SR = ~TIM_SR_UIF;     // rc_w0: nur UIF löschen, KEIN Read-Modify-Write
    __DSB();                    // Löschen wirksam vor ISR-Ende (M7: sonst Doppel-Eintritt)

    if (callback_) callback_();
}

} // namespace sds110
