/*
 * SDS_110_Board.h – Board-Schalter, aus C (main.c) und C++ nutzbar.
 */
#ifndef SDS_110_BOARD_H
#define SDS_110_BOARD_H

/* Zielboard.
 * 0 = eigenes Board STM32F746ZGT6 (Schaltplan doc/Schematic.pdf)
 * 1 = STM32F746G-Discovery (Evaluierung, nur Simulation): dasselbe Image läuft dort (gleicher
 *     Chip F746, 25-MHz-Takt, gleiche Speichergrößen). Board-spezifische Peripherie des eigenen
 *     Boards wird NICHT initialisiert, weil deren Pins auf dem Discovery anders belegt sind:
 *     GPIOs (EN_ESP_CTRL PC13 = uSD-Erkennung, EN_ADA, LEDs PG2..4, MAG_INT), QUADSPI (PA1 = ETH),
 *     I2C2, USART1 (PA10 = OTG_FS_ID), USART3 (PD8/PD9 = SDRAM-Daten) – jeweils USER CODE ..._Init 0
 *     bzw. MX_GPIO_Init_1 in main.c. PC-Verbindung über USB-CDC an CN13 (USB FS) wie in SDS_110,
 *     Herzschlag auf LED1 (PI1, grün). LCD bleibt dunkel. */
#ifndef SDS110_BOARD_DISCO
#define SDS110_BOARD_DISCO 0
#endif

/* SAI1 / ADAU7118 (Mikrofonpfad 116) verwenden.
 * 0 = Hardwarefehler am SAI (30.09.2026): SAI1 wird weder in MX_SAI1_Init (main.c, USER CODE
 *     SAI1_Init 0) noch in 116 initialisiert, ADAU7118 bleibt aus (EN_ADA low). Die Firmware
 *     läuft nur mit dem Signal-Simulator; USB-Kommando Typ 3 = 0 (Mikrofone) wird ignoriert.
 * 1 = normaler Betrieb mit Mikrofonen. */
#ifndef SDS110_SAI_ENABLED
#define SDS110_SAI_ENABLED 0
#endif

/* Verbindung zum PC-Monitor.
 * 1 = USART1 (PA9 TX, PA10 RX) über den CP2102N-USB-UART-Wandler an USB-C #2 „DATA/USBDEVICE“
 *     (Schaltplan Seite 7/8; Jumper JM1/JM2 stecken). Der USB-OTG-FS-Port des STM32 (PA11/PA12)
 *     ist auf diesem Board nicht beschaltet – USB-CDC kann keinen COM-Port liefern.
 * 0 = USB-CDC (OTG FS) wie auf dem Discovery-Board.
 * Standard: 1 auf dem eigenen Board, 0 auf dem Discovery (dort liegt USART1-RX an PB7). */
#ifndef SDS110_LINK_UART
#if SDS110_BOARD_DISCO
#define SDS110_LINK_UART 0
#else
#define SDS110_LINK_UART 1
#endif
#endif
#if SDS110_BOARD_DISCO && SDS110_LINK_UART
#error "Discovery-Board: USART1-RX liegt an PB7, nicht PA10 – SDS110_LINK_UART 0 (USB-CDC an CN13) verwenden"
#endif
#if SDS110_BOARD_DISCO && SDS110_SAI_ENABLED
#error "Discovery-Board: ADAU7118 an SAI1/I2C2 gibt es dort nicht – nur Simulation (SDS110_SAI_ENABLED 0)"
#endif
/* UART für die PC-Verbindung bei SDS110_LINK_UART 1.
 * 3 = USART3 (PD8 TX, PD9 RX) zum ESP32-C3-MINI-1-N4 (Schaltplan Seite „ESP32-C3 Wi-Fi Module“,
 *     EN über PC13 EN_ESP_CTRL). Der ESP32-C3 reicht die Bytes per WLAN/TCP an den PC-Monitor
 *     durch (Firmware tools/esp32c3_bridge); Nachrichtenformat unverändert (ICD 2). Standard,
 *     weil USB auf diesem Board nicht funktioniert (01.10.2026).
 * 1 = USART1 (PA9/PA10) über den CP2102N, Kabel an USB-C #2. */
#ifndef SDS110_PC_UART
#define SDS110_PC_UART 3
#endif
#if SDS110_PC_UART != 1 && SDS110_PC_UART != 3
#error "SDS110_PC_UART: 1 (USART1/CP2102N) oder 3 (USART3/ESP32-C3)"
#endif
/* Baudrate USART1 <-> CP2102N; am PC-Monitor dieselbe Baudrate einstellen (bei USB-CDC war sie egal).
 * 921600: ~92 kB/s, reicht für DETECT (Reports, Logger); READ-Streaming (1,6 MB/s) passt nicht. */
#ifndef SDS110_UART_BAUD
#define SDS110_UART_BAUD 921600U
#endif
/* Baudrate USART3 <-> ESP32-C3; muss zu CONFIG_BRIDGE_UART_BAUD der Brücke passen.
 * 1 000 000 teilt beide Takte ohne Rest (USART3 54 MHz / 54, ESP32-C3 80 MHz / 80): ~100 kB/s,
 * Spitzenlast DETECT ~17 kB/s (Detect + UnitReport 5,5 kB/s, Logger bis 11,5 kB/s). */
#ifndef SDS110_ESP_UART_BAUD
#define SDS110_ESP_UART_BAUD 1000000U
#endif
/* Empfang von USART3 nach dem Start so lange verwerfen (ms): Das ROM des ESP32-C3 gibt beim Booten
 * Meldungen mit 115200 Baud auf UART0 aus; sie würden sonst als fehlerhafte Kommandos am LCD
 * erscheinen. Der ESP32-C3 startet mit EN_ESP_CTRL (PC13) in MX_GPIO_Init. */
#ifndef SDS110_ESP_BOOT_MS
#define SDS110_ESP_BOOT_MS 1500U
#endif

#endif /* SDS_110_BOARD_H */
