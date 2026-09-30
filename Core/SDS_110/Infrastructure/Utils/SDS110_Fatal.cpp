/*
 * SDS110_Fatal.cpp  (Infrastructure/Utils) – siehe SDS110_Fatal.h
 * Keine RTOS-Aufrufe, kein snprintf (Stack, Reentranz): Text mit eigenen Hilfsfunktionen.
 * STM32F746ZGT6-Board: kein LCD -> Meldung über ITM/SWO (Port 0), LED_ERROR (PG4) an.
 */
#include "SDS110_Fatal.h"
#include "stm32f7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "Infrastructure/Driver/StatusLed.hpp"   // LED je Zielboard

namespace {

volatile bool g_inFatal = false;

struct Line {
    char s[56] = {};
    int  n = 0;
    Line& str(const char* t) { while (t && *t && n < 55) s[n++] = *t++; return *this; }
    Line& dec(uint32_t v)
    {
        char d[10]; int k = 0;
        do { d[k++] = char('0' + v % 10); v /= 10; } while (v && k < 10);
        while (k && n < 55) s[n++] = d[--k];
        return *this;
    }
    Line& hex(uint32_t v) { for (int i = 7; i >= 0 && n < 55; --i) s[n++] = "0123456789ABCDEF"[(v >> (4 * i)) & 0xF]; return *this; }
};

// ITM Port 0 (PrintfDriver): nur wenn ein Debugger/SWO-Viewer den Port freigegeben hat
void itmPrint(const char* t)
{
    if ((ITM->TCR & ITM_TCR_ITMENA_Msk) == 0 || (ITM->TER & 1UL) == 0) return;
    while (t && *t) ITM_SendChar(static_cast<uint32_t>(*t++));
}

[[noreturn]] void haltWith(const char* title, const Line& detail)
{
    taskDISABLE_INTERRUPTS();
    if (!g_inFatal) {                        // Fehler während der Anzeige: nur anhalten
        g_inFatal = true;
        sds110::led::setError(true);
        itmPrint(title); itmPrint(": "); itmPrint(detail.s); itmPrint("\r\n");
    }
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) __BKPT(0);   // Debugger: hier anhalten
    for (;;) {}
}

} // namespace

extern "C" void vAssertCalled(const char* file, int line)
{
    haltWith("ASSERT", Line().str(file).str(":").dec(static_cast<uint32_t>(line)));
}

extern "C" void SDS110_FatalStackOverflow(const char* taskName)
{
    haltWith("STACK OVERFLOW", Line().str("Task ").str(taskName));
}

extern "C" void SDS110_FatalMallocFailed(void)
{
    haltWith("HEAP VOLL", Line().str("configTOTAL_HEAP_SIZE ").dec(configTOTAL_HEAP_SIZE));
}

extern "C" void SDS110_FatalHardFault(void)
{
    // CFSR: Fehlerart (MemManage/Bus/Usage), HFSR: Eskalation, BFAR: Adresse bei präzisem Busfehler
    haltWith("HARDFAULT", Line().str("CFSR ").hex(SCB->CFSR).str(" HFSR ").hex(SCB->HFSR)
                                .str(" BFAR ").hex(SCB->BFAR));
}
