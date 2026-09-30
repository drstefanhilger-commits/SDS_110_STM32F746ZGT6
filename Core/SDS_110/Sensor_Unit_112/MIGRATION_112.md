# Migration Sensor_Unit_112 (114 + 116)

## Herkunft
| Neu                                | Aus SDS                                              |
|------------------------------------|------------------------------------------------------|
| Microphone_Array_114.{hpp,cpp}     | Model/SDS_MicrophoneBuffer, SDS_Params (Geometrie)   |
| Sampling_Circuitry_116.{hpp,cpp}   | ADUA7118/adua7118Driver.c, sai.c, adau7118.c, dma.c  |
| ADAU7118_Registers.hpp             | ADUA7118/adua7118Driver.c (REG_*)                    |
| entfällt                           | TDM_Parser.*, SDS_SAI_A.*, Utils/SDS_RingBuffer.*    |

## Offene Punkte (vor dem ersten Test klären)
Stand 25.09.2026 – Details und Commits in `doc/Analyse_Befunde.md`.
1. **I2C-Adresse**: 0x4B (adua7118Driver.c) vs. 0x3A (adau7118.c) → `ADAU7118_I2C_ADDR_7B` in Config.
   *Offen (Befund 4, zurückgestellt).*
2. **SAI-Block**: sai.c nutzt SAI1_Block_A (PE4/5/6), main.c (CubeMX) SAI2_Block_A/B.
   `init()` bekommt das Handle übergeben; MSP (GPIO/Clock) muss in CubeMX zum gewählten Block passen.
   *Offen (Befunde 2 und 3, zurückgestellt): SAI2 hat in CubeMX keinen DMA; `.ioc` ist das DISCO-Preset.*
   *Erledigt (Befund 5): SAI-Takt kommt jetzt aus PLLI2S (47 991 Hz), `configureSaiClock()` für SAI1 und SAI2.*
3. **Registerwerte** in ADAU7118_Registers.hpp gegen Datenblatt prüfen (zwei Sequenzen im alten Code widersprachen sich).
   *Offen (Befund 4, zurückgestellt): Tabelle passt vermutlich nicht zur Registerbelegung des Linux-Treibers.*
4. ~~**Linker**: Sektion `.sdram_data` anlegen~~ – *erledigt*: `.sdram_data` (NOLOAD) existiert. Die Puffer von 114
   enthalten seit Befund 17 je einen Hop (3 × 49 kB statt 3 × 98 kB).
5. ~~**Doppelte HAL-Callbacks**~~ – *erledigt*: `HAL_SAI_Rx*Callback` / `HAL_SAI_ErrorCallback` gibt es nur noch in
   Sampling_Circuitry_116.cpp (`Core/SDS` ist entfernt).
6. ~~**Zeitbasis**: Tick-basiert (1 ms)~~ – *erledigt (Befund 16)*: `TimeBase::nowUs()` (DWT, µs), Zeitstempel für das
   erste Sample des Blocks. Weiterhin offen: UTC-Bezug (GNSS-PPS/PTP) für die Inter-Unit-Synchronisation.

Weitere Änderungen seit der Migration: Rohdaten-Skalierung 2⁻³¹ (Befund 6), DMA-Puffer in `.dma_nocache` (Befund 21),
114 liefert Hops, die Überlappung entsteht im `Frame_Assembler` hinter 118 (Befund 17).

## Aufruf aus main.c / Wrapper
`SDS110_Init()` (SDS_110_Wrapper.cpp) prüft zuerst das SDRAM (Befund 14) und ruft dann
`Processing_Module_120::instance().init(&hsai_BlockA2, &hi2c1)`, das `Sensor_Unit_112::init()` aufruft.
`start()` erfolgt im ProcessingTask, sobald die Simulation per USB (Typ 3) abgeschaltet wird.
