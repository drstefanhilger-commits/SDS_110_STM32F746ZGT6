/*
 * main.cpp – ESP32-C3-Flasher für das SDS_110-Board (STM32F746ZGT6)
 *
 * Eigene, kleine STM32-Firmware: Sie schreibt die Brücken-Firmware (tools/esp32c3_bridge) über
 * USART3 in den ESP32-C3-MINI-1-N4. Die Images stecken im Flash des STM32 (esp_images.S, erzeugt
 * von mkimages.py). Nötig, weil USB auf dem Board nicht funktioniert und IO18/IO19 des ESP nicht
 * beschaltet sind; gebraucht wird nur der ST-LINK.
 *
 * Ablauf (README.md):
 *   1. Flasher per ST-LINK aufspielen, SW3 (BOOT, IO9 des ESP) gedrückt halten
 *   2. Der Flasher setzt den ESP über EN (PC13) zurück, bis er im Download-Modus antwortet
 *      (LED RUN blinkt). Verbunden: LED COMM an – SW3 loslassen.
 *   3. Images schreiben (COMM blinkt), jedes per MD5 prüfen, ESP neu starten
 *   4. Fertig: RUN leuchtet. Fehler: ERROR leuchtet, Meldung über SWO (ITM-Port 0).
 *   5. Wieder die SDS-Firmware aufspielen.
 *
 * Takt: HSI 16 MHz ohne PLL (unabhängig vom Quarz); USART3 ebenfalls aus HSI.
 */
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "EspRomLoader.hpp"
#include "stm32f7xx_hal.h"

// Pins wie Core/Inc/main.h der SDS-Firmware
#define EN_ESP_PORT GPIOC
#define EN_ESP_PIN  GPIO_PIN_13
#define LED_PORT    GPIOG
#define LED_RUN     GPIO_PIN_2
#define LED_COMM    GPIO_PIN_3
#define LED_ERROR   GPIO_PIN_4

struct EspImage {
    uint32_t    offset;
    const uint8_t* data;
    uint32_t    size;
    const char* name;
    char        md5[36];       // 32 Hex-Zeichen + Nullbytes
};
extern "C" const EspImage g_espImages[];
extern "C" const uint32_t g_espImageCount;
extern "C" const uint32_t g_espFlashSize;

namespace {

constexpr uint32_t ROM_BAUD   = 115200;   // ROM-Bootloader und Boot-Meldungen
constexpr uint32_t FLASH_BAUD = 460800;   // HSI 16 MHz, Oversampling 8: Fehler 0,6 %

UART_HandleTypeDef s_uart;

// ---------------------------------------------------------------- Ausgabe über SWO (ITM)
void log(const char* fmt, ...)
{
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    const int len = (n < 0) ? 0 : (n > static_cast<int>(sizeof(buf)) - 2 ? static_cast<int>(sizeof(buf)) - 2 : n);
    buf[len] = '\n';
    for (int i = 0; i <= len; ++i) ITM_SendChar(static_cast<uint32_t>(buf[i]));
}

void led(uint16_t pin, bool on) { HAL_GPIO_WritePin(LED_PORT, pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET); }

// ---------------------------------------------------------------- USART3 <-> ESP32-C3 UART0
void uartSetBaud(uint32_t baud)
{
    s_uart.Instance          = USART3;
    s_uart.Init.BaudRate     = baud;
    s_uart.Init.WordLength   = UART_WORDLENGTH_8B;
    s_uart.Init.StopBits     = UART_STOPBITS_1;
    s_uart.Init.Parity       = UART_PARITY_NONE;
    s_uart.Init.Mode         = UART_MODE_TX_RX;
    s_uart.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    s_uart.Init.OverSampling = UART_OVERSAMPLING_8;
    s_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    (void)HAL_UART_Init(&s_uart);
}

void uartClearErrors()
{
    USART3->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NCF | USART_ICR_PECF;
}

class Stm32Transport : public espflash::EspTransport {
public:
    void write(const uint8_t* d, size_t n) override
    {
        for (size_t i = 0; i < n; ++i) {
            while (!(USART3->ISR & USART_ISR_TXE)) {}
            USART3->TDR = d[i];
        }
    }
    int readByte(uint32_t timeoutMs) override
    {
        const uint32_t t0 = HAL_GetTick();
        for (;;) {
            const uint32_t isr = USART3->ISR;
            if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE)) uartClearErrors();
            if (isr & USART_ISR_RXNE) return static_cast<int>(USART3->RDR & 0xFF);
            if (HAL_GetTick() - t0 >= timeoutMs) return -1;
        }
    }
    void flushInput() override
    {
        while (USART3->ISR & USART_ISR_RXNE) (void)USART3->RDR;
        uartClearErrors();
    }
    void setBaud(uint32_t baud) override
    {
        while (!(USART3->ISR & USART_ISR_TC)) {}             // letztes Byte noch mit alter Baudrate
        uartSetBaud(baud);
    }
    void delayMs(uint32_t ms) override { HAL_Delay(ms); }
    uint32_t millis() override { return HAL_GetTick(); }
};

Stm32Transport s_link;
espflash::EspRomLoader s_loader(s_link);

// ---------------------------------------------------------------- ESP32-C3 steuern
void resetEsp()
{
    HAL_GPIO_WritePin(EN_ESP_PORT, EN_ESP_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    s_link.flushInput();
    HAL_GPIO_WritePin(EN_ESP_PORT, EN_ESP_PIN, GPIO_PIN_SET);
}

/// Boot-Meldung des ROM mitlesen (115200 Baud): 1 = Anwendung startet, 2 = Download-Modus, 0 = keine
int readBootMessage(char* out, size_t cap)
{
    size_t n = 0;
    const uint32_t t0 = HAL_GetTick();
    while (HAL_GetTick() - t0 < 400) {
        const int c = s_link.readByte(10);
        if (c >= 0 && n + 1 < cap) out[n++] = (c >= 0x20 && c < 0x7F) || c == '\n' ? static_cast<char>(c) : '.';
    }
    out[n] = '\0';
    if (std::strstr(out, "DOWNLOAD")) return 2;
    if (std::strstr(out, "boot:0x")) return 1;
    return 0;
}

[[noreturn]] void fail(const char* step, espflash::Result r)
{
    log("FEHLER bei %s: %s (ROM-Fehler 0x%02X, Befehl 0x%02X)", step, espflash::toString(r), s_loader.lastError(),
        s_loader.lastOp());
    log("STM32 neu starten (Reset) für einen neuen Versuch, dabei SW3 halten.");
    led(LED_RUN, false);
    led(LED_COMM, false);
    led(LED_ERROR, true);
    for (;;) __WFI();
}

void gpioInit()
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    HAL_GPIO_WritePin(EN_ESP_PORT, EN_ESP_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_RUN | LED_COMM | LED_ERROR, GPIO_PIN_RESET);
    GPIO_InitTypeDef g{};
    g.Pin   = EN_ESP_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(EN_ESP_PORT, &g);
    g.Pin = LED_RUN | LED_COMM | LED_ERROR;
    HAL_GPIO_Init(LED_PORT, &g);
}

} // namespace

extern "C" void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
    if (huart->Instance != USART3) return;
    RCC_PeriphCLKInitTypeDef clk{};
    clk.PeriphClockSelection = RCC_PERIPHCLK_USART3;
    clk.Usart3ClockSelection = RCC_USART3CLKSOURCE_HSI;
    (void)HAL_RCCEx_PeriphCLKConfig(&clk);
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitTypeDef g{};
    g.Pin       = GPIO_PIN_8 | GPIO_PIN_9;                   // PD8 TX -> RXD0, PD9 RX <- TXD0
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &g);
}

extern "C" void SysTick_Handler(void) { HAL_IncTick(); }

int main()
{
    HAL_Init();
    gpioInit();
    uartSetBaud(ROM_BAUD);

    log("SDS_110 ESP32-C3-Flasher: %lu Images", static_cast<unsigned long>(g_espImageCount));
    for (uint32_t i = 0; i < g_espImageCount; ++i)
        log("  0x%06lX %7lu B  %.32s  %s", static_cast<unsigned long>(g_espImages[i].offset),
            static_cast<unsigned long>(g_espImages[i].size), g_espImages[i].md5, g_espImages[i].name);

    // 1. Download-Modus: ESP zurücksetzen, bis er auf SYNC antwortet (SW3 gedrückt)
    log("SW3 (BOOT) gedrückt halten …");
    for (uint32_t attempt = 1;; ++attempt) {
        resetEsp();
        HAL_Delay(50);
        if (s_loader.sync(5) == espflash::Result::Ok) break;
        HAL_GPIO_TogglePin(LED_PORT, LED_RUN);
        if (attempt % 5 == 0) log("keine Antwort vom ESP32-C3 – SW3 gedrückt? (Versuch %lu)", static_cast<unsigned long>(attempt));
    }
    led(LED_RUN, false);
    led(LED_COMM, true);
    log("ESP32-C3 im Download-Modus – SW3 jetzt loslassen");

    // 2. Flash vorbereiten, schneller übertragen
    espflash::Result r;
    if ((r = s_loader.spiAttach()) != espflash::Result::Ok) fail("SPI_ATTACH", r);
    if ((r = s_loader.setFlashParams(g_espFlashSize)) != espflash::Result::Ok) fail("SPI_SET_PARAMS", r);
    if ((r = s_loader.changeBaud(FLASH_BAUD)) != espflash::Result::Ok) fail("CHANGE_BAUDRATE", r);

    // 3. Images schreiben und prüfen
    const uint32_t t0 = HAL_GetTick();
    for (uint32_t i = 0; i < g_espImageCount; ++i) {
        const EspImage& im = g_espImages[i];
        log("schreibe %s nach 0x%lX (%lu B) …", im.name, static_cast<unsigned long>(im.offset),
            static_cast<unsigned long>(im.size));
        uint32_t nextPct = 25;
        r = s_loader.writeImage(im.offset, im.data, im.size, im.md5, [&](uint32_t done, uint32_t total) {
            if ((done / espflash::EspRomLoader::BLOCK) % 8 == 0) HAL_GPIO_TogglePin(LED_PORT, LED_COMM);
            if (done * 100ull >= static_cast<uint64_t>(nextPct) * total && nextPct < 100) {
                log("  %lu %%", static_cast<unsigned long>(nextPct));
                nextPct += 25;
            }
        });
        if (r != espflash::Result::Ok) fail(im.name, r);
        log("  ok, MD5 bestätigt");
    }
    log("alle Images geschrieben in %lu s", static_cast<unsigned long>((HAL_GetTick() - t0) / 1000));

    // 4. ESP neu starten und an der ROM-Meldung prüfen, dass er die Anwendung startet
    s_link.setBaud(ROM_BAUD);
    char boot[200];
    for (int attempt = 0;; ++attempt) {
        resetEsp();
        const int b = readBootMessage(boot, sizeof(boot));
        if (b == 1) { log("ESP32-C3 startet die Brücke: %s", boot); break; }
        if (b == 0) { log("keine Boot-Meldung des ROM (eFuse UART_PRINT_CONTROL?) – vermutlich ok"); break; }
        led(LED_ERROR, attempt & 1);                         // Download-Modus: SW3 noch gedrückt
        if (attempt % 4 == 0) log("ESP32-C3 im Download-Modus – SW3 loslassen");
        HAL_Delay(500);
    }

    led(LED_ERROR, false);
    led(LED_COMM, false);
    led(LED_RUN, true);
    log("FERTIG. Jetzt wieder die SDS-Firmware aufspielen.");
    for (;;) __WFI();
}
