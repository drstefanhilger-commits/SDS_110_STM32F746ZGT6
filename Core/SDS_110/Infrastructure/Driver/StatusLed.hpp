/*
 * StatusLed.hpp  (Infrastructure/Driver)
 * Status-LEDs je Zielboard (SDS_110_Board.h):
 *   eigenes Board: RUN PG2, COMM PG3, ERROR PG4 (CubeMX-Labels, MX_GPIO_Init)
 *   Discovery:     nur LED1 PI1 (grün) – Herzschlag; fataler Fehler: dauerhaft an
 */
#pragma once
#include "SDS_110_Board.h"
#include "main.h"

namespace sds110::led {

inline void init()
{
#if SDS110_BOARD_DISCO
    __HAL_RCC_GPIOI_CLK_ENABLE();
    GPIO_InitTypeDef g{};
    g.Pin = GPIO_PIN_1; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOI, &g);
#endif
}

inline void toggleRun()
{
#if SDS110_BOARD_DISCO
    HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_1);
#else
    HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
#endif
}

inline void toggleComm()
{
#if !SDS110_BOARD_DISCO
    HAL_GPIO_TogglePin(LED_COMM_GPIO_Port, LED_COMM_Pin);
#endif
}

inline void setError(bool on)
{
#if SDS110_BOARD_DISCO
    if (on) HAL_GPIO_WritePin(GPIOI, GPIO_PIN_1, GPIO_PIN_SET);   // nur fatal (Herzschlag steht)
#else
    HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

} // namespace sds110::led
