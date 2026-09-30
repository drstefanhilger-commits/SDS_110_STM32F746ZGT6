/*
 * SDS110_Fatal.h  (Infrastructure/Utils) – C-Schnittstelle
 *
 * Fatale Fehler anzeigen und anhalten: LED_ERROR an, Ursache als Text über ITM/SWO
 * (das STM32F746ZGT6-Board hat kein LCD), danach Breakpoint (Debugger angeschlossen)
 * bzw. Endlosschleife mit gesperrten Interrupts.
 * Aufrufer: configASSERT (FreeRTOSConfig.h), vApplicationStackOverflowHook,
 * vApplicationMallocFailedHook (freertos.c), HardFault_Handler (stm32f7xx_it.c).
 * Bei einem Fehler während der Meldung wird nur angehalten.
 */
#ifndef SDS110_FATAL_H
#define SDS110_FATAL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

void vAssertCalled(const char* file, int line);
void SDS110_FatalStackOverflow(const char* taskName);
void SDS110_FatalMallocFailed(void);
void SDS110_FatalHardFault(void);

#ifdef __cplusplus
}
#endif
#endif /* SDS110_FATAL_H */
