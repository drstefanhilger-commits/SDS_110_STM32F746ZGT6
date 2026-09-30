/*
 * DWT.hpp  (Infrastructure/Utils)
 * Zyklenzähler (DWT->CYCCNT) für Laufzeitmessungen.
 *
 * Cortex-M7 (STM32F7): Die DWT-Register sind per Software-Lock gesperrt.
 * Ohne Entsperren über DWT->LAR werden Schreibzugriffe der CPU auf
 * DWT->CTRL ignoriert -> CYCCNT läuft nicht, alle Zeiten sind 0.
 * (Zugriffe des Debuggers umgehen den Lock, deshalb klappt es manchmal
 *  nur mit aktivem SWV/Trace.)
 */
#pragma once
#include "stm32f7xx_hal.h"
#include <cstdint>

namespace sds110 {

class DWTTimer {
public:
    static DWTTimer& instance() { static DWTTimer inst; return inst; }

    uint32_t cycles() const { return DWT->CYCCNT; }
    float cyclesToUs(uint32_t c) const { return static_cast<float>(c) / cpuMHz; }
    float cyclesToNs(uint32_t c) const { return (static_cast<float>(c) * 1000.0f) / cpuMHz; }
    void  setCpuMHz(float mhz) { cpuMHz = mhz; }

    /// true, wenn CYCCNT tatsächlich zählt (Diagnose)
    bool isRunning() const { return running_; }

private:
    float cpuMHz  = 216.0f;
    bool  running_ = false;

    DWTTimer()
    {
        if (SystemCoreClock > 0) cpuMHz = SystemCoreClock / 1.0e6f;

        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   // Trace-Block einschalten
        DWT->LAR = 0xC5ACCE55;                            // Software-Lock lösen (Cortex-M7)
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;              // Zähler starten

        const uint32_t a = DWT->CYCCNT;
        __NOP(); __NOP(); __NOP(); __NOP();
        running_ = (DWT->CYCCNT != a);
    }
};

} // namespace sds110
