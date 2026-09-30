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

#endif /* SDS_110_BOARD_H */
