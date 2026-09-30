/*
 * SDS_110_Board.h – Board-Schalter, aus C (main.c) und C++ nutzbar.
 */
#ifndef SDS_110_BOARD_H
#define SDS_110_BOARD_H

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
 * 0 = USB-CDC (OTG FS) wie auf dem Discovery-Board. */
#ifndef SDS110_LINK_UART
#define SDS110_LINK_UART 1
#endif
/* Baudrate USART1 <-> CP2102N; am PC-Monitor dieselbe Baudrate einstellen (bei USB-CDC war sie egal).
 * 921600: ~92 kB/s, reicht für DETECT (Reports, Logger); READ-Streaming (1,6 MB/s) passt nicht. */
#ifndef SDS110_UART_BAUD
#define SDS110_UART_BAUD 921600U
#endif

#endif /* SDS_110_BOARD_H */
