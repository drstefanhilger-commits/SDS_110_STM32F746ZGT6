/*
 * MPUDriver.h  (Infrastructure/Driver)
 *
 * MPU-Konfiguration für SDS_110. Aufruf aus main.c, USER CODE BEGIN 1
 * (vor SCB_EnableICache/DCache und HAL_Init):
 *
 *     SDS110_MPU_Config();
 *
 * Die CubeMX-Funktion MPU_Config() bleibt unbenutzt (leer oder auskommentiert).
 * Nur aus main.c (C) einbinden – Header-only-Definition.
 *
 * Regionen (STM32F746ZGT6-Board, kein externes SDRAM, kein LCD):
 *   0  SRAM2    0x2004C000, 16 kB   – Normal, nicht cachebar: DMA-Puffer (.dma_nocache, SDS110_DMA_SECTION)
 *      SRAM1 (0x20010000, 240 kB) ist nicht abgedeckt -> Standard-Speicherkarte (Write-Back,
 *      Write-Allocate). DTCM (0x20000000, 64 kB) ist architekturbedingt nie gecacht.
 *   Die SDRAM-Regionen des Discovery-Boards (DSP-Puffer, LTDC-Framebuffer) entfallen.
 */
#pragma once
#include "stm32f7xx_hal.h"

static inline void SDS110_MPU_ConfigRegion(uint8_t number, uint32_t base, uint8_t size,
                                           uint8_t cacheable, uint8_t bufferable, uint8_t tex)
{
    MPU_Region_InitTypeDef r = {0};
    r.Enable           = MPU_REGION_ENABLE;
    r.Number           = number;
    r.BaseAddress      = base;
    r.Size             = size;
    r.SubRegionDisable = 0x00;
    r.TypeExtField     = tex;
    r.AccessPermission = MPU_REGION_FULL_ACCESS;
    r.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
    r.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
    r.IsCacheable      = cacheable;
    r.IsBufferable     = bufferable;
    HAL_MPU_ConfigRegion(&r);
}

static inline void SDS110_MPU_Config(void)
{
    HAL_MPU_Disable();

    /* 0: SRAM2 Normal, nicht cachebar (TEX=1, C=0, B=0) – DMA-Puffer ohne Cache-Pflege */
    SDS110_MPU_ConfigRegion(MPU_REGION_NUMBER0, 0x2004C000, MPU_REGION_SIZE_16KB,
                            MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE, MPU_TEX_LEVEL1);

    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}
