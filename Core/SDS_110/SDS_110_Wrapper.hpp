/*
 * SDS_110_Wrapper.hpp
 *
 * C-Schnittstelle für main.c (STM32F746ZGT6-Board).
 * Zieht die Header-only-Treiber ein, die main.c direkt aufruft:
 *   PrintfDriver.h -> ITM/SWO für printf (USER CODE BEGIN SysInit)
 *   MPUDriver.h    -> SDS110_MPU_Config() (USER CODE BEGIN 1, vor Cache-Enable und HAL_Init)
 *
 * Gegenüber dem STM32F746G-Discovery-Aufbau (Repo SDS_110) entfallen LCD (LCDTask, LCDDriver)
 * und externes SDRAM (SDRAMDriver, Selbsttest): alle Puffer liegen im internen SRAM.
 */
#pragma once
#include "SDS_110_Board.h"             // SDS110_SAI_ENABLED (auch für main.c)

// Nur für main.c (C): Header-only-Treiber mit nicht-inline Definitionen.
// Aus C++-Dateien NICHT einziehen, sonst doppelte Definition beim Linken.
#ifndef __cplusplus
#include "Infrastructure/Driver/PrintfDriver.h"
#include "Infrastructure/Driver/MPUDriver.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void SDS110_Init(void);                  // Sensor Unit 112 + Processing Module 120
void SDS110_StartProcessingTask(void);   // Task um Processing_Module_120
void SDS110_StartUSBTask(void);
void SDS110_StartLoggerTask(void);       // Logger (USB Id 99) + Status-LEDs
void SDS110_UartSelfTest(void);          // nur SDS110_UART_SELFTEST: kehrt nicht zurück

#ifdef __cplusplus
}
#endif
