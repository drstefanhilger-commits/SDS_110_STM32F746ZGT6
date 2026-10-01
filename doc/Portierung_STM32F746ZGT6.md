# Portierung SDS_110 → SDS_110_STM32F746ZGT6

Stand 30.09.2026. Quelle: Repo `SDS_110` (STM32F746G-Discovery), Ziel: eigenes Board mit
STM32F746ZGT6 (Schaltplan `doc/Schematic.pdf`).

## 1. Analyse des Skeletons

| | SDS_110 (Discovery) | SDS_110_STM32F746ZGT6 |
|---|---|---|
| MCU | STM32F746NGH6 (TFBGA216) | STM32F746ZGT6 (LQFP144) |
| RAM | 320 kB intern + **8 MB SDRAM** (FMC) | **nur 320 kB intern** (DTCM 64 + SRAM1 240 + SRAM2 16) |
| Flash | 1 MB | 1 MB (+ 8 MB QSPI W25Q64, nicht genutzt) |
| Anzeige | LCD 480×272 (LTDC, LCDTask) | **kein LCD** – 3 LEDs: RUN PG2, COMM PG3, ERROR PG4 |
| Mikrofone | ADAU7118 über SAI2 Block A, I2C1 | ADAU7118 über **SAI1 Block A** (FS PE4, SCK PE5, SD PE6), **I2C2** (PB10/PB11), EN_ADA PE3 |
| DMA | – | DMA2 Stream 1 Kanal 0 (SAI1_A), zirkulär, Wort |
| USB | OTG FS CDC (PLLSAI 48 MHz) | OTG FS CDC (48 MHz aus PLLQ = 432/9) |
| Sonstiges | SD, ETH, DCMI, … | QSPI, RTC (LSI), USART1, USART3 (ESP32-C3), MAG_INT PG8, SWO PB3 |
| Takt | 216 MHz, HSE 25 MHz | identisch (PLLM 25, PLLN 432) |
| Projekt | C++-Projekt, DSP-Lib, SDS-Include-Pfade | C-Projekt, ohne DSP-Lib, TIM1..10 nur initialisiert |

Der CubeMX-Rahmen (Takt, SAI1+DMA, I2C2, USB-CDC, FreeRTOS, TIM6-Zeitbasis) passt zur
Firmware. Das eigentliche Hindernis ist der Speicher: SDS_110 belegte **304 kB internes RAM
+ 391 kB SDRAM** (gemessen mit `wsl/Makefile`), das Ziel hat 320 kB insgesamt.

## 2. Portierte Teile

Übernommen: `Core/SDS_110` komplett (Module 112–130, Harness, Infrastructure), DSP-Bibliothek
`Drivers/CMSIS/Lib/GCC/libarm_cortexM7lfsp_math.a` samt Headern, Host-Tests `test/host`,
Kommandozeilen-Build `wsl/Makefile`.

Entfallen: `LCDTask`, `LCDDriver`, `Font8x12`, `SDRAMDriver`, `SDRAMSelfTest` (+ Test).
`TaskId::Lcd` bleibt im Modell, damit das USB-Format der Task-Statistik unverändert bleibt.

Anpassungen an die Hardware:

* `SDS_110_Wrapper.cpp`: `hsai_BlockA1` / `hi2c2` statt `hsai_BlockA2` / `hi2c1`; kein
  SDRAM-Selbsttest; `SDS110_StartDisplayTask()` entfällt.
* `MPUDriver.h`: nur Region 0 (SRAM2, nicht cachebar, DMA-Puffer); SDRAM-Regionen entfallen.
* `SDS110_Fatal.cpp`: statt LCD-Meldung LED_ERROR an und Text über ITM/SWO (Port 0).
* `LoggerTask`: zusätzlich Status-LEDs – RUN blinkt 1 Hz, COMM wechselt bei USB-Empfang,
  ERROR bei ML-Init-/Laufzeitfehler.
* `Sampling_Circuitry_116`: unverändert lauffähig für SAI1 (Kerneltakt aus PLLI2S, 47 991 Hz).
* CubeMX-Dateien nur in `USER CODE`-Abschnitten geändert: `main.c` (MPU, ITM, `SDS110_Init`,
  Task-Start), `freertos.c` (Hooks), `stm32f7xx_it.c` (HardFault), `usbd_cdc_if.c`
  (Empfang/Sendeende an USBTask/USBDriver). FreeRTOS-Parameter (Heap 32 kB, Stack-Prüfung 2,
  Malloc-/Idle-Hook, FPU, Task-Tags) zusätzlich in der `.ioc`, damit eine Neugenerierung sie behält.
* Linker `STM32F746ZGTX_FLASH.ld`: RAM 304 kB + `RAM_NC` 16 kB (SRAM2) mit `.dma_nocache`.
* `.project`/`.cproject`: C++-Nature, Include-Pfade `Core/SDS_110/…`, C++-Defines,
  G++-Linker mit `arm_cortexM7lfsp_math`, printf mit float.

## 3. Speicher: von 703 kB auf 309 kB (intern + SRAM2 + SDRAM)

| Maßnahme | Einsparung | Wirkung auf Ergebnisse |
|---|---|---|
| Hops als 16-Bit-Blockgleitkomma (je Kanal und DMA-Block ein Zweierexponent) | 148 → 75 kB | Rohdaten mit \|pcm24\| < 2^15 exakt; sonst < 2^-15 des Blockmaximums |
| 118 in place im Hop, Frame_Assembler hält Zeiger statt eigener Slots (99 kB) | 99 kB | keine |
| Spektren nur bis Bin 352 (126 nutzt nur Bänder bis 4080 Hz) | 131 → 22,5 kB | keine (bitgleich) |
| Gemeinsamer DSP-Arbeitsspeicher `DspScratch.hpp` (118, 122 FFT, 126 IFFT) | 38 kB | keine (bitgleich) |
| Mel-Filterbank dicht gespeichert | 15 kB | keine (bitgleich) |
| FFT-Twiddles im Flash statt RAM-Kopie (`SDS110_FFT_TABLES_IN_RAM 0`) | 32 kB | bitgleich, FFT etwas langsamer |
| (Hop-Pool: 3 Puffer statt 3 Hop-Puffer + 2 Fenster-Slots, s. o.) | – | Zeitreserve kleiner, siehe 4.1 |
| USB-Sende-Ring in den freien Teil von SRAM2 | 8 kB | keine |
| FreeRTOS-Heap 48 → 32 kB (kein LCDTask) | 16 kB | keine |

Ergebnis (`make -C wsl PREFIX=arm-none-eabi-`): RAM 292,6 kB / 304 kB (inkl. 4 kB
Mindest-Heap/-Stack), RAM_NC 16 / 16 kB, Flash 453 kB / 1 MB.

Größte Objekte: Hop-Pool 73 kB, 122 38 kB, 124 34 kB, DSP-Scratch 32 kB, FreeRTOS-Heap 32 kB,
Spektren 22 kB, 126 13,5 kB, Simulator 11 kB.

Host-Tests (`make -C test/host check`): alle Prüfungen bestanden. Messungen gegenüber dem
Discovery-Stand (m_overview, m_bearing_drone, m_selection, m_confidence): Abweichungen höchstens
0,01° in der Peilung, 0,1 dB im Noise-Floor, Detektionsraten identisch.

## 4. Offene Punkte / Risiken

1. **Echtzeit am Board messen.** Mit nur 3 Hop-Puffern muss 120 die Spektren eines Frames
   innerhalb eines Hops (32 ms) nach dessen Ende berechnet haben, sonst verwirft 114 Blöcke
   (`droppedFrames`, Lücke in `frame_id`). Die Twiddles im Flash verlängern 122 etwas
   (Discovery: 1,69 ms je Mikrofon mit Twiddles im RAM). Stufenzeiten kommen weiter über USB
   (`setStageTimes`/`setDiagTimes`).
2. **ADAU7118-I2C-Adresse** (`ADAU7118_I2C_ADDR_7B = 0x4B`) mit Datenblatt und Beschaltung
   (ADDR/CONFIG an 3V3) prüfen; I2C2 teilt sich den Bus mit dem Magnetometer.
3. **Merkmalsversion**: Der Quelltext-Hash der Merkmalskette (114/118/Frame_Assembler/122)
   weicht vom Modell ab (`t_ml124` meldet einen Hinweis). Standardstufe ist „Schatten“ (HBD
   entscheidet); vor der Stufe `Ml` mit Merkmalen dieser Kette neu trainieren. Das
   Merkmalswerkzeug `tools/features` wurde nicht portiert (PC-Werkzeug, Repo SDS_110).
4. Kein LCD: Anzeige der Statuswerte nur über den PC-Monitor (USB) und SWO.
5. Der Ordner `Debug/` enthält noch die Build-Ausgabe des leeren Skeletons; CubeIDE erzeugt die
   Makefiles beim nächsten Build neu. `STM32F746ZGTX_RAM.ld` (Ausführung aus RAM) ist für diese
   Firmware zu klein und nicht angepasst.
6. Nicht genutzt: QSPI-Flash, RTC-Zeit, USART1/USART3 (ESP32-C3), Magnetometer (MAG_INT),
   TIM1…TIM10 außer TIM7 (LoggerTask).

## 5. Betrieb nur mit Simulator (SAI-Hardwarefehler, 30.09.2026)

Schalter `SDS110_SAI_ENABLED` in `Core/SDS_110/SDS_110_Board.h`, Standard **0**:

* `MX_SAI1_Init()` kehrt sofort zurück (`USER CODE BEGIN SAI1_Init 0` in `main.c`, übersteht
  CubeMX-Neugenerierung); `HAL_SAI_MspInit` läuft nie, SAI1-IRQ und DMA bleiben ungenutzt.
* 116 (`Sampling_Circuitry_116::init`) wird nicht aufgerufen: kein I2C-Zugriff auf den ADAU7118,
  EN_ADA bleibt low.
* Der ProcessingTask läuft immer mit dem Signal-Simulator (Standard: Szenario DroneSweep).
  USB-Kommando Typ 3 wählt weiter das Szenario (z. B. DroneStatic, FlyBy); Typ 3 = 0 (Mikrofone)
  wird ignoriert und einmal als Fehlermeldung „SAI aus: nur Simulation“ gemeldet.

Nach der Reparatur `SDS110_SAI_ENABLED` auf 1 setzen.

## 6. PC-Verbindung über USART1 / CP2102N (30.09.2026)

Befund am Board: RUN-LED (blau, Herzschlag des LoggerTask) blinkt, am PC erscheint aber kein
COM-Port der Firmware. Ursache laut Schaltplan (Seite 7 „power“, Seite 8 „PROG“): USB-C #2
„DATA/USBDEVICE“ führt über USBLC6 auf den **CP2102N** (U21, USB-UART-Wandler). Dessen TXD/RXD gehen
über die Jumper **JM1/JM2** auf **USART1** (PA10 RX, PA9 TX). Der USB-OTG-FS-Port des STM32
(PA11/PA12) ist nicht beschaltet – die USB-CDC-Firmware des Discovery-Boards kann hier keinen
COM-Port liefern.

Änderung (Schalter `SDS110_LINK_UART` in `SDS_110_Board.h`, Standard 1):

* `USBDriver` sendet über USART1 (`HAL_UART_Transmit_IT` aus dem Ringpuffer), Empfang per
  `HAL_UARTEx_ReceiveToIdle_IT` an `USBTask_OnReceive` – Nachrichtenformat (ICD) unverändert.
* USART1-IRQ (Priorität 5) und `USART1_IRQHandler` in `USBDriver.cpp`; in CubeMX den USART1-IRQ
  **nicht** aktivieren (sonst doppelter Handler).
* Baudrate **921600** (`SDS110_UART_BAUD`, auch in `.ioc`/`main.c`). **Am PC-Monitor 921600 Baud,
  8N1, ohne Flusssteuerung einstellen** – bei USB-CDC war die Baudrate egal. READ-Streaming
  (1,6 MB/s) passt nicht über die UART; DETECT/CALIBRATE (Reports, Logger) schon.
* Am PC erscheint der Port als „Silicon Labs CP210x USB to UART Bridge (COMx)“ (Treiber von
  Silicon Labs); die COM-Nummer vergibt Windows neu – ggf. im Geräte-Manager auf COM5 umstellen.
* Jumper JM1 und JM2 müssen gesteckt sein.

### Test der Verbindung (01.10.2026)

Am PC (Python 3, `pip install pyserial`, PC-Monitor vorher schließen):

```
python test/pc/uart_link_test.py --list            # Port „CP210x“ suchen
python test/pc/uart_link_test.py --port COM5
```

Das Skript erkennt die laufende Firmware selbst:

1. **Selbsttest-Firmware** (`SDS110_UART_SELFTEST 1` in `SDS_110_Board.h`, neu bauen und flashen):
   läuft ohne RTOS und ohne SDS-Module direkt auf den USART1-Registern
   (`Infrastructure/Driver/UartSelfTest.cpp`). Sie sendet beim Start
   `UART-SELFTEST start baud=… ist=… brr=… pclk2=…` (eingestellte und tatsächliche Baudrate), dann
   jede Sekunde einen Herzschlag mit Zählern (Bytes, Kommandos, CRC-Fehler, Überlauf `ore`,
   Rahmenfehler `fe`) und beantwortet jedes Kommando mit `ECHO id=.. len=.. crc=OK|BAD c=<crc>`.
   LED_RUN blinkt 1 Hz, LED_COMM wechselt je Kommando, LED_ERROR leuchtet nach einem Überlauf oder
   Rahmenfehler. Damit lassen sich Jumper, CP2102N, COM-Port und Baudrate getrennt von
   USBDriver/USBTask prüfen. Danach den Schalter wieder auf 0 setzen.
2. **Normale Firmware**: Standort Id 6 kommt jede Sekunde; das Skript sendet Id 10 mit zufälligen
   Koordinaten und erwartet sofort Id 6 mit denselben Werten. Zum Schluss setzt es den Standort auf
   den Ursprung zurück (`--keep-position` verhindert das).

Je Runde drei Varianten: ein Kommando je Schreibvorgang, ein Kommando auf zwei Schreibvorgänge
verteilt (5 ms Pause), zwei Kommandos in einem Schreibvorgang. Geprüft werden außerdem CRC-Fehler und
übersprungene Bytes im Empfangsstrom. Exit-Code 0 = bestanden. Ohne Hardware:
`--simulate selftest` bzw. `--simulate firmware`. Die Logik des Selbsttests prüft der Host-Test
`t_uart_selftest`.

| Befund des Skripts | wahrscheinliche Ursache |
| --- | --- |
| „nichts empfangen“ | falscher Port, Jumper JM1/JM2 offen, Firmware nicht geflasht, Board ohne Versorgung |
| nur Müll, keine gültigen Nachrichten | Baudrate falsch (PC und `SDS110_UART_BAUD` vergleichen, Startmeldung `ist=`) |
| Senden geht, Rundlauf scheitert | RX-Pfad PA10 ← CP2102N-TXD (Jumper JM1/JM2), TX/RX vertauscht |
| `ore` > 0 | Board liest zu langsam (Überlauf) |

## 7. Test auf dem STM32F746G-Discovery (nur Simulation)

Schalter `SDS110_BOARD_DISCO` in `Core/SDS_110/SDS_110_Board.h` auf **1** setzen und neu bauen
(Kommandozeile: `make -C wsl PREFIX=arm-none-eabi- OPT="-O0 -g3 -DSDS110_BOARD_DISCO=1"`).
Das Image läuft auf dem Discovery, weil Chip (F746), 25-MHz-Takt, Flash und RAM gleich sind.

* PC-Verbindung: USB-CDC an **CN13 (USB FS)** wie im Projekt SDS_110 (`SDS110_LINK_UART` folgt
  automatisch auf 0; die Kombination Discovery + UART bricht den Build ab).
* Nicht initialisiert (Pins auf dem Discovery anders belegt): MX_GPIO_Init (EN_ESP_CTRL PC13 =
  uSD-Erkennung, EN_ADA, LEDs PG2..4, MAG_INT), QUADSPI (PA1 = ETH-Takt), I2C2, USART1
  (PA10 = OTG_FS_ID), USART3 (PD8/PD9 = SDRAM-Daten); SAI ohnehin aus.
* LED1 (PI1, grün): Herzschlag 1 Hz, dauerhaft an bei fatalem Fehler. LCD bleibt dunkel.
* Flashen mit dem ST-LINK des Discovery; die CubeIDE-Debugkonfiguration nennt STM32F746ZGTx –
  derselbe Chip, eine Gehäuse-Warnung kann übergangen werden.
* Für das eigene Board den Schalter wieder auf 0 setzen.
