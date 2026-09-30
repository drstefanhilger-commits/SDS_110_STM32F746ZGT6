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
/* Baudrate USART1 <-> CP2102N; am PC-Monitor dieselbe Baudrate einstellen (bei USB-CDC war sie egal).
 * 921600: ~92 kB/s, reicht für DETECT (Reports, Logger); READ-Streaming (1,6 MB/s) passt nicht. */
#ifndef SDS110_UART_BAUD
#define SDS110_UART_BAUD 921600U
#endif

#endif /* SDS_110_BOARD_H */
