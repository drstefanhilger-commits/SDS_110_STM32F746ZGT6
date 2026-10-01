/*
 * UartSelfTest.cpp  (Infrastructure/Driver)
 *
 * Selbsttest der PC-Verbindung USART1 <-> CP2102N <-> PC, ohne RTOS und ohne SDS-Module
 * (SDS110_UART_SELFTEST = 1 in SDS_110_Board.h). main.c ruft SDS110_UartSelfTest() nach der
 * Peripherie-Initialisierung auf (USER CODE BEGIN 2); die Funktion kehrt nicht zurück.
 *
 * Zweck: Hardware und Baudrate prüfen, getrennt von USBDriver/USBTask (Jumper JM1/JM2, CP2102N,
 * COM-Port, 921600 Baud 8N1). Gegenstück am PC: test/pc/uart_link_test.py (erkennt den
 * Selbsttest am Herzschlag). Logik in Infrastructure/Utils/UartSelfTestCore.hpp.
 *
 *   - Polling direkt auf den USART1-Registern (kein IRQ: USART1_IRQHandler gehört USBDriver);
 *     Senden aus einem Ringpuffer Byte für Byte, damit der Empfang nie blockiert
 *   - Überlauf (ORE), Rahmenfehler (FE) und Rauschen (NE) werden gezählt und gelöscht
 *   - LED_RUN blinkt mit dem Herzschlag (1 Hz), LED_COMM wechselt bei jedem Kommando,
 *     LED_ERROR leuchtet nach dem ersten ORE/FE/NE
 */
#include "SDS_110_Board.h"
#include "main.h"
#include "Infrastructure/Utils/UartSelfTestCore.hpp"

#if SDS110_UART_SELFTEST

namespace {

constexpr uint32_t TX_RING = 2048;                 // Zweierpotenz; > 10 Antworten à 144 Byte
uint8_t  s_tx[TX_RING];
uint32_t s_txHead = 0, s_txTail = 0, s_txDropped = 0;

void txPush(const uint8_t* d, uint32_t n)
{
    if (n > TX_RING - (s_txHead - s_txTail)) { ++s_txDropped; return; }   // ganze Nachricht oder keine
    for (uint32_t i = 0; i < n; ++i) s_tx[(s_txHead + i) & (TX_RING - 1)] = d[i];
    s_txHead += n;
}

sds110::UartSelfTestCore s_core;                   // statisch: CRC-Tabelle 1 KB nicht auf dem Stack

void sendText(const char* text)
{
    uint8_t msg[sds110::UartSelfTestCore::MSG_LEN];
    s_core.buildLog(msg, text);
    txPush(msg, sizeof(msg));
}

} // namespace

extern "C" void SDS110_UartSelfTest(void)
{
    USART_TypeDef* const u = USART1;
    uint32_t ore = 0, fe = 0;

    u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;
    {
        char text[sds110::UartSelfTestCore::TEXT_LEN];
        const uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();
        const uint32_t brr   = u->BRR;
        std::snprintf(text, sizeof(text), "UART-SELFTEST start baud=%lu ist=%lu brr=%lu pclk2=%lu",
                      static_cast<unsigned long>(SDS110_UART_BAUD),
                      static_cast<unsigned long>(brr ? pclk2 / brr : 0),   // Oversampling 16
                      static_cast<unsigned long>(brr), static_cast<unsigned long>(pclk2));
        sendText(text);
    }

    uint32_t nextBeat = HAL_GetTick();
    for (;;) {
        const uint32_t isr = u->ISR;

        if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) {
            if (isr & USART_ISR_ORE) ++ore;
            if (isr & (USART_ISR_FE | USART_ISR_NE)) ++fe;
            u->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF;
            HAL_GPIO_WritePin(LED_ERROR_GPIO_Port, LED_ERROR_Pin, GPIO_PIN_SET);
        }

        if (isr & USART_ISR_RXNE) {
            const uint8_t b = static_cast<uint8_t>(u->RDR);
            const uint32_t before = s_core.commands();
            s_core.onRx(&b, 1, static_cast<uint64_t>(HAL_GetTick()) * 1000u, sendText);
            if (s_core.commands() != before) HAL_GPIO_TogglePin(LED_COMM_GPIO_Port, LED_COMM_Pin);
        }

        if ((isr & USART_ISR_TXE) && s_txHead != s_txTail) {
            u->TDR = s_tx[s_txTail & (TX_RING - 1)];
            ++s_txTail;
        }

        const uint32_t now = HAL_GetTick();
        if (static_cast<int32_t>(now - nextBeat) >= 0) {
            nextBeat += 1000;
            char text[sds110::UartSelfTestCore::TEXT_LEN];
            s_core.heartbeat(text, sizeof(text), now, ore, fe, s_txDropped);
            sendText(text);
            HAL_GPIO_TogglePin(LED_RUN_GPIO_Port, LED_RUN_Pin);
        }
    }
}

#endif // SDS110_UART_SELFTEST
