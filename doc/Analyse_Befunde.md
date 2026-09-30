# SDS_110 – Befunde der Code-Analyse (25.09.2026)

Status: **nicht bearbeiten** = bewusst zurückgestellt, **offen** = zu bearbeiten,
**bearbeitet** = geändert (Datum, Commit; Testumfang steht dabei).

## Übersicht bearbeitet

| Punkt | Thema | Datum | Commit |
|---|---|---|---|
| 11 | USB-Senden über TX-Ringpuffer | 25.09.2026 | 15a87ab |
| 7 | HBD-Rauschboden / Normierung | 25.09.2026 | c8be312 |
| 23 | Peilung 180° verdreht (Vorzeichen Fernfeldmodell) | 25.09.2026 | 187441b |
| 8 | Peak-Ratio-Test (lokale Nebenmaxima statt ±3 Samples) | 25.09.2026 | 82c8fcc |
| 24 | Band-Selektion 124: nur Harmonische, Gate mit HBD-Haltezeit, HBD-Anlaufzeit | 25.09.2026 | b4a18f4 |
| 13 | FreeRTOS-Heap 64 KB, Prüfung bei Task-Anlage, Hooks halten an | 25.09.2026 | 8d3644c |
| 12 | Kommandolänge: Vorlagen auf 16, gemeinsame Konstante | 25.09.2026 | 7207038 |
| 10 | UnitReport: nsel = tatsächlich gesendete Bänder | 25.09.2026 | d572820 |
| 9 | Distanz-Pegel vor NS/AGC (angewendete Verstärkung herausrechnen) | 25.09.2026 | b81255c |
| 6 | Rohdaten-Skalierung 2⁻³¹ (24 Bit linksbündig), Simulator im Hardwareformat | 25.09.2026 | ad18673 |
| 5 | SAI-Takt aus PLLI2S (47 991 Hz statt 53 571 Hz), Prüfung der Ist-Abtastrate | 25.09.2026 | f7d4a56 |
| 14 | SDRAM-Selbsttest vor der ersten Nutzung, am D-Cache vorbei | 25.09.2026 | 8a1240d |
| 15 | Wirkungsloses SDRAM-Kommando in MX_DMA2D_Init entfernt | 25.09.2026 | c673854 |
| 16 | Zeitstempel µs-genau (DWT) und für das erste Sample des Blocks | 25.09.2026 | ea1e5b7 |
| 17 | 50-%-Überlappung: Hops in 114/118, Analysefenster (Frame_Assembler), Zeitkonstanten in Sekunden | 25.09.2026 | 05b3d3d |
| 18 | Einheitliche Konfidenz aus Paaren und Residuum in Samples (128) | 25.09.2026 | e0ca865 |
| 19 | Logger: threadsicher, Überlaufschutz, max. 255 Zeichen | 25.09.2026 | 7da4097 |
| 21 | SRAM1 gecacht, DMA-Puffer in nicht cachebarem SRAM2 (Normal statt Strongly-ordered) | 25.09.2026 | 0621e76 |
| 22 | Migrationsnotizen und READMEs auf aktuellen Stand | 25.09.2026 | 9941759 |
| 36 | Veraltetes HBD_ML_Model_Data.hpp ersetzt (mit 20) | 27.09.2026 | 5017f27 |
| 28 | Task-Takt an den 32-ms-Hop gekoppelt (HopClock, Warten auf Hop) | 27.09.2026 | 84897b2 |
| 39 | LTDC-Pins GPIO_SPEED_FREQ_HIGH (Bild zitterte) | 27.09.2026 | 604b34d |

## Blocker (Hardware-Pfad)

1. **ProcessingTask nicht gestartet** – `SDS110_StartProcessingTask()` ist in `Core/Src/main.c:270` auskommentiert; weder Verarbeitung noch Simulator laufen.
   Status: **bearbeitet (27.09.2026)** – Task eingeschaltet, läuft im Simulationsbetrieb (Standard `simulation = 1`); gebaut, auf dem Board nicht getestet. Vorher abgesichert: Befund 29 (Simulationsflag bei Sperr-Timeout) und der HardFault aus Blocker 2. Der Hardware-Pfad bleibt wegen Blocker 2–4 ohne Funktion.
2. **SAI2 ohne DMA** – `HAL_SAI_MspInit` richtet keinen DMA ein, es gibt keine DMA-IRQ-Handler. `HAL_SAI_Receive_DMA` dereferenziert `hsai->hdmarx` ohne Prüfung (`stm32f7xx_hal_sai.c:1494`) → HardFault, sobald die Simulation per USB abgeschaltet wird.
   Status: **nicht bearbeiten** (DMA fehlt weiter). Seit 27.09.2026 abgefangen: `Sampling_Circuitry_116::start()` startet ohne `hdmarx` nicht (`dmaReady()`), `ProcessingTask` meldet „116: SAI ohne DMA“ auf dem LCD statt HardFault.
3. **CubeMX-Konfiguration passt nicht zur eigenen Platine** – `.ioc` ist das Preset STM32F746G-DISCO (ETH, LTDC, DCMI, ULPI). Laut `STM32F746_PINS.txt` nutzt die Platine SAI1 (PE4/PE5), I2C2 (PB10/PB11), PE3 als Enable; der Code nutzt SAI2_A und I2C1. Auf dem DISCO ist PE3 der FAULT-Ausgang des STMPS2151 (`OTG_HS_OverCurrent`) und wird von `Sampling_Circuitry_116.cpp:37` als Push-Pull auf High getrieben.
   Status: **nicht bearbeiten**
   - Hinweis (Befund 39): Bei der neuen CubeMX-Konfiguration für die eigene Platine die LTDC-Pins wieder auf `GPIO_SPEED_FREQ_HIGH` setzen; die CubeMX-Vorgabe `LOW` lässt das Bild zittern.
4. **ADAU7118-Registertabelle vermutlich falsch** – `ADAU7118_Registers.hpp` (POWER, PLL_CTRL, MODE_CTRL …) passt weder zur Tabelle in `ADUA_Design.md` noch zur Registerbelegung des Linux-Treibers (0x00–0x03 IDs nur lesbar, 0x04 ENABLES, 0x05 DEC_RATIO_CLK_MAP, 0x06 HPF, 0x07/0x08 SPT_CTRL1/2, 0x11 DRIVE, 0x12 RESET). Schreibzugriffe auf nur lesbare Register werden per ACK bestätigt → `init()` meldet Erfolg ohne Wirkung. I2C-Adresse (0x4B oder 0x3A) offen. Designdoku §5 falsch: der ADAU7118 ist an der seriellen Schnittstelle Slave (SAI als Master ist korrekt).
   Status: **nicht bearbeiten**
5. **Abtastrate vermutlich ≈ 53,6 kHz statt 48 kHz** – SAI2 bekommt 192 MHz aus PLLSAI, nach HAL-Formel MCKDIV = 7. Nur berechnet, am FSYNC messen. Abhilfe: SAI2 aus PLLI2S takten (≈ 49,152 MHz), da PLLSAI auch USB (48 MHz) und LTDC versorgt.
   Status: **bearbeitet (25.09.2026, Commit f7d4a56)** – gerechnet und gebaut, auf dem Board nicht gemessen.
   - Bestätigt mit der HAL-Formel dieses Projekts (`stm32f7xx_hal_sai.c:508`): 192 MHz → MCKDIV 7 → 53 571 Hz (+11,6 %).
   - Durchrechnung aller zulässigen Einstellungen (PLLM = 25 fest, weil 216 MHz SYSCLK aus 25 MHz HSE nur mit M = 25 geht → 1 MHz PLL-Eingang): exakt 48 kHz ist nicht erreichbar. PLLSAI mit USB = 48 MHz exakt: bestenfalls 46 875 Hz (−2,3 %). PLLI2S N = 344, Q = 7, DivQ = 1 → 49,143 MHz → MCKDIV 2 → **47 991 Hz (−186 ppm)**, das Optimum (entspricht den ST-Audio-Beispielen).
   - `Sampling_Circuitry_116::configureSaiClock()`: stellt vor `HAL_SAI_Init` den SAI-Takt (SAI1 oder SAI2, je nach Handle) auf PLLI2S mit `SAI_PLLI2S_N/Q/DIVQ` aus `SDS_110_Config.hpp`. Bewusst im eigenen Code statt in `PeriphCommonClock_Config()` (CubeMX-generiert). Läuft nach `MX_SPDIFRX_Init()` und überschreibt dessen PLLI2S-Einstellung (SPDIFRX wird nicht genutzt).
   - Nach `HAL_SAI_Init` wird die Ist-Abtastrate aus SAI-Kerneltakt und MCKDIV berechnet (`sampleRateHz()`); Abweichung > 0,1 % (`SAI_FS_TOLERANCE`) → Fehler, `init()` liefert false.
   - Empfehlung für CubeMX (Punkt 3): SAI-Taktquelle auf PLLI2S (N 344, Q 7) stellen und SPDIFRX deaktivieren, damit Clock-Tree und Code übereinstimmen. Am Board: FSYNC messen (erwartet 47,991 kHz, BCLK 12,286 MHz).
6. **Rohdaten um Faktor 256 falsch skaliert** – 24-Bit-Samples kommen linksbündig im 32-Bit-Slot; `Microphone_Array_114.cpp:64` skaliert mit 2⁻²³ statt 2⁻³¹. Der Simulator schreibt rechtsbündig und verdeckt den Fehler.
   Status: **bearbeitet (25.09.2026, Commit ad18673)** – im Host-Test geprüft, auf dem Board nicht getestet.
   - `SDS_110_Config.hpp`: `PCM_RAW_FULL_SCALE = 2^31` mit Beschreibung des Rohformats (pcm24 << 8).
   - `Microphone_Array_114::pushBlock()`: Skalierung 1/2^31 statt 1/2^23.
   - `Signal_Simulator`: schreibt jetzt wie die Hardware linksbündig (`pcm24 << 8`), sonst wäre er nach der Korrektur um Faktor 256 zu leise.
   - Test `pushBlock()` mit Rohwerten im Hardwareformat (±Vollaussteuerung, ±0,5/−0,25, 1 LSB, 0): vorher Faktor 256 zu groß (Vollaussteuerung → ±256), nachher alle exakt. Regression: Selektions-, Peil- und Distanztest liefern identische Ausgabe wie vor der Änderung.
   - `USBDriver::sendRead()` (READ-Modus) rechnet float wie bisher auf 24 Bit rechtsbündig zurück; das Wire-Format zum PC bleibt gleich, die Werte stimmen jetzt auch mit echter Hardware.
   - Auf der Hardware zu bestätigen (mit den Punkten 2–4): Liegen die 24 Bit tatsächlich in Bits 31…8 des Slots? Das hängt von der ADAU7118-Konfiguration (Datenbreite, Verzögerung im Slot) und vom SAI-Rahmen (`FSOffset`, `FirstBitOffset`) ab.

## Signalverarbeitung

7. **HBD meldet immer „DRONE“ (berechnet)** – Rauschboden auf max. −60 dB begrenzt (`HBD.cpp:16`), Betragsspektrum aber unnormiert (Rauschbins nach AGC ≈ +10 dB) → Boden klebt bei −60 dB, SNR ≈ 70 dB überall, alle p_b ≈ 1, alle Bänder selektiert, Fehlalarm auch bei Wind/Einzelton. Abhilfe: Spektrum durch Σw normieren (1536 für Hann/3072; `windowGain` ist vorgesehen, aber ungenutzt). Danach beachten: der gleitende Mittelwert (0,94) lernt einen stehenden Drohnenton in ≈ 1 s als Rauschen.
   Status: **bearbeitet (25.09.2026, Commit c8be312)** – im Host-Test geprüft, auf dem Board nicht getestet – `HBD.cpp/.hpp`: Spektrum in dBFS normiert (`windowGain = 2/Σw`), Floor-Grenzen −140…0 dBFS, Start-Floor aus dem spektralen Umgebungsmittel, EMA je Bin; schmale Peaks (> Umgebungsmittel + 10 dB) heben den Floor nur um 0,01 dB/Frame (stehender Drohnenton wird erst nach ~2 min gelernt). Host-Test (Simulator, SNR 20 dB, 300 Frames): DRONE bei Drohne 100 %, Einzelton 3 %, Wind 0 %, Stille 0 % (vorher überall 100 %); Empfindlichkeit 81 % bei 10 dB, 38 % bei 3 dB.
   Nachtrag: Die zu großzügige Band-Selektion in 124 (`SDS_Data::detected` bei Stille 22 %, Einzelton 54 %) ist in Punkt 24 behoben.
8. **Peak-Ratio-Test in 126 zu streng** – als zweiter Peak zählt jeder Wert außerhalb ±3 Samples statt des zweiten lokalen Maximums (`Correlation_Processing_Module_126.cpp:116`). Bei schmalbandigen Harmonischen < 1,5 kHz ist die Hauptkeule viel breiter → Ratio ≈ 1,1 < 1,5 → keine Peilung. Im Host-Test bestätigt: bei Drohnensignal 0 % gültige Peilungen, auch bei SNR 30 dB.
   Status: **bearbeitet (25.09.2026, Commit 82c8fcc)** – im Host-Test geprüft, auf dem Board nicht getestet. `crossCorrelate()`: Hauptkeule = vom Maximum aus nach beiden Seiten, solange die Korrelation fällt; zweiter Peak = höchstes lokales Maximum außerhalb der Hauptkeule. Maximum am Fensterrand wird verworfen. Ohne Nebenmaximum wird die Ratio auf `PEAK_RATIO_MAX = 20` begrenzt (Gewicht in `128::solve()`). `PEAK_EXCLUDE_S` entfernt. Host-Test Drohne (volle Kette 118–126, 12 Richtungen, SNR 30…0 dB): gültige Peilungen vorher 0 %, nachher 99 / 97 / 86 / 75 / 56 / 33 %; Fehler-Median 0,5–1,7°, 95 %-Quantil 1,8–6,0°. Breitband-Test aus Punkt 23 unverändert (max. 0,08°). Offen: `srpScan()` berechnet seine Peak-Ratio (nur Debug-Anzeige) noch mit dem direkten Nachbarn des Maximums.
9. **Distanzschätzung misst die AGC** – `levelA` wird nach NS/AGC pro Kanal berechnet (`Processing_Module_120.cpp:84`); r = K/A beschreibt die Verstärkung, nicht den Abstand.
   Status: **bearbeitet (25.09.2026, Commit b81255c)** – im Host-Test geprüft, auf dem Board nicht getestet.
   - `Pre_Processor_118`: `noiseSuppress()` und `agc()` geben die angewendete Verstärkung zurück; `appliedGain(ch)` = NS · AGC des letzten Frames. Zugriff über `Sensor_Unit_112::preprocessor()`.
   - `Processing_Module_120`: `levelA` wird durch `appliedGain(REF_MIC)` geteilt → Pegel vor der Regelung.
   - Host-Test (Drohne, SNR 20 dB, Simulator-Pegel ∝ 1/r, 10…200 m): Verhältnis r_geschätzt/r_wahr vorher 0,073 / 0,042 / 0,024 / 0,024 / 0,024 (AGC regelt unterhalb ~50 m, darüber an der Obergrenze 32), nachher 0,525 / 0,521 / 0,528 / 0,523 / 0,522 (konstant, < 1 % Schwankung).
   - Offen: `LEVEL_DIST_K_REF = 100` ist nicht kalibriert (Simulator: Faktor ≈ 1,9 zu klein; der Simulator-Quellpegel ist willkürlich, daher nicht daraus übernommen). K mit realer Drohne in bekanntem Abstand bestimmen. Das Feld `level` im UnitReport hat damit eine andere Skala als vorher – falls der PC-Monitor es auswertet, dort anpassen.
10. **UnitReport inkonsistent** – `num_selected` wird ungekürzt (bis 64) gesendet, serialisiert werden max. 56 Einträge (`Output_Interface_130.cpp:50`).
    Status: **bearbeitet (25.09.2026, Commit d572820)** – im Host-Test geprüft, auf dem Board nicht getestet.
    - `Output_Interface_130::send()`: Feld `nsel` enthält die tatsächlich gesendete Anzahl n = min(num_selected, 56); die 56 folgt aus `(sizeof(MessageData) − 16) / 2`.
    - Host-Test Serialisierung (0/3/8/56/57/64 Bänder): vorher bei 57 und 64 Bändern `nsel` = 57/64, d. h. der PC hätte 130 bzw. 144 von 128 Byte gelesen; nachher `nsel` = 56 und Bandliste vollständig innerhalb der 128 Byte.
    - Hinweis: Seit Punkt 24 werden nur noch Bänder mit Harmonischen selektiert (≤ 8 bzw. 3 als Rückfall), mehr als 56 kommen praktisch nicht mehr vor.

## USB und RTOS

11. **USB-Senden fehlerhaft** – `CDC_Transmit_FS` speichert nur den Zeiger, gesendet wird aus Stack-Variablen (OTG-FS füllt den FIFO später im Interrupt). `send()` schickt zwei Nachrichten direkt hintereinander → UnitReport (id 4) meist `USBD_BUSY`. READ-Modus sendet 192 × 532 Byte ohne BUSY-Behandlung. Mehrere Tasks senden ohne Sperre. Abhilfe: TX-Queue + ein Sende-Task mit statischem Puffer, Warten auf `TransmitCplt`.
    Status: **bearbeitet (25.09.2026, Commit 15a87ab)** – auf dem Board noch nicht getestet – TX-Ringpuffer (8 KB) in `USBDriver`, Nachrichten werden im kritischen Abschnitt vollständig kopiert, Versand in Blöcken bis 2 KB aus statischem `txBuf_`, Nachladen in `CDC_TransmitCplt_FS` (USER CODE 13). Kein zusätzlicher Task. READ-Streaming wartet bis 20 ms auf Platz und verwirft sonst den Rest des Frames. Zähler `USBDriver::txDropped()`.
12. **Kommandolänge widersprüchlich** – Handler verlangt `payloadLen == 16`, Vorlagen für Typ 1–3 in `SDS_Structs.hpp` haben `0x0C`. Gegen PC-Seite prüfen.
    Status: **bearbeitet (25.09.2026, Commit 7207038)** – gebaut, auf dem Board nicht getestet.
    - Klärung: Der Handler (16) ist richtig. Seit Commit `b0cfc2c` (07.09.2026, Magic `DE AD BE EF` eingeführt) ist das Längenfeld die Gesamtlänge der Nachricht (4 Magic + 1 Id + 3 Länge + 4 Wert + 4 CRC = 16), wie bei den Nachrichten Board → PC. Vorher (bis `ce6a0a2`, 27.08.2026) gab es keinen Magic, und die Länge war 12. `PC_Monitor_Test.ptp` sendet ebenfalls `00 00 10`. Die Vorlagen mit `0x0C` stammten aus dem alten Format und werden in der Firmware nicht verwendet.
    - `SDS_Structs.hpp`: Konstante `SDS_CMD_LENGTH = 16`, alle vier Vorlagen nutzen sie, `static_assert` auf 16 Byte; Formatbeschreibung als Kommentar.
    - `USBTask`: `payloadLen()` → `msgLen()` (ist die Gesamtlänge), Vergleich gegen `SDS_CMD_LENGTH` statt der Zahl 16.
    - Weiterhin offen (bekanntes ToDo): Die CRC der Kommandos wird nicht geprüft.
13. **FreeRTOS-Heap (32 KB) zu knapp** – mit ProcessingTask (16 KB Stack) ≈ 30 KB plus TCBs/Queues. `TaskBase::start()` prüft `osThreadNew` nicht auf NULL. Heap auf ≥ 64 KB erhöhen.
    Status: **bearbeitet (25.09.2026, Commit 8d3644c)** – gebaut, auf dem Board nicht getestet.
    - Genauere Bilanz (Größen aus dem ELF, TCB 172 B; Idle- und Timer-Task liegen statisch, nicht im Heap): heute ≈ 10,3 KB, mit ProcessingTask ≈ 26,9 KB von 32 KB. Es hätte gepasst, aber mit nur ≈ 6 KB Reserve und ohne Fehlermeldung bei Überschreitung.
    - `configTOTAL_HEAP_SIZE` 32 KB → 64 KB in `FreeRTOSConfig.h` **und** `SDS.ioc` (sonst setzt CubeMX den Wert zurück). RAM-Belegung 69 → 101 KB von 320 KB.
    - `TaskBase::start()` gibt jetzt `bool` zurück und hält per `configASSERT` an, wenn `osThreadNew` NULL liefert (wie `TaskTimerBase`).
    - `freertos.c` (USER CODE 4/5): `vApplicationStackOverflowHook` und `vApplicationMallocFailedHook` waren leer; sie halten jetzt mit abgeschalteten Interrupts an.
    - Hinweis: Der ProcessingTask ist weiterhin auskommentiert (Punkt 1, „nicht bearbeiten“).

## Kleinere Punkte

14. SDRAM-Selbsttest läuft erst nach dem Konstruktor, der bereits ins SDRAM schreibt → schützt nicht.
    Status: **bearbeitet (25.09.2026, Commit 8a1240d)** – Logik im Host-Test geprüft, auf dem Board nicht getestet.
    - Zusätzlich gefunden: Der alte Test hätte auch später nichts erkannt – das SDRAM ist per MPU Write-Back-cachebar, die zwei Testwörter wurden aus dem D-Cache zurückgelesen.
    - Neu `Infrastructure/Driver/SDRAMSelfTest.{hpp,cpp}`: prüft zuerst `hsdram1.State == READY` (sonst kein Zugriff), dann Muster an Offset 0, 2^k und am letzten Wort von `.sdram_data` (je Adresse eigener Wert → Adressleitungen), `SCB_CleanInvalidateDCache()` vor dem Zurücklesen, zweiter Durchgang invertiert (Datenbits).
    - `SDS110_Init()`: Test läuft vor `Processing_Module_120::instance()`; bei Fehler Meldung „SDRAM self-test failed“, keine Initialisierung von 112/120, `SDS110_StartProcessingTask()` startet dann nicht. Alter Test und `Processing_Module_120_spectraProbe()` entfernt.
    - Host-Test (HAL-Stub): Handle nicht READY → false ohne Speicherzugriff; intakter Bereich (591 KB) → true, 20 Prüfadressen, 2 Cache-Flushes, kein Schreiben außerhalb; leerer Bereich → true. Fehlerhaftes SDRAM und das Cache-Verhalten lassen sich auf dem Host nicht nachbilden.
15. SDRAM-Kommando in `MX_DMA2D_Init` läuft vor `MX_FMC_Init` → wirkungslos, entfernen.
    Status: **bearbeitet (25.09.2026, Commit c673854)** – gebaut.
    - Geprüft: `hsdram1.State` ist vor `MX_FMC_Init` `HAL_SDRAM_STATE_RESET`; `HAL_SDRAM_SendCommand` gibt dann `HAL_ERROR` zurück, ohne Register anzufassen. Der Block war also harmlos, kostete nur `HAL_Delay(1)` beim Start. Die vollständige Sequenz (inkl. Clock-Enable) sendet `SDRAM_InitSequence()` in USER CODE „FMC_Init 2“.
    - Inhalt von USER CODE „DMA2D_Init 0“ in `main.c` entfernt (Marker und CRLF erhalten).
16. Zeitstempel 1-ms-Tick-basiert und vom Ende des DMA-Blocks (≈ 2,7 ms zu spät).
    Status: **bearbeitet (25.09.2026, Commit ea1e5b7)** – Zeitlogik im Host-Test geprüft, auf dem Board nicht getestet.
    - Neu `Infrastructure/Utils/TimeBase.{hpp,cpp}`: `TimeBase::nowUs()` aus DWT->CYCCNT (216 MHz, 4,6 ns), auf 64 bit erweitert (`CycleExtender`); zwischen zwei Aufrufen verpasste Überläufe (CYCCNT läuft alle 19,9 s über) werden über den 1-ms-HAL-Tick ermittelt. TIM2/TIM5 bewusst nicht genutzt (von CubeMX als PWM mit Pins belegt).
    - `Sampling_Circuitry_116`: Zeitstempel = `nowUs()` − Blockdauer (`DMA_BLOCK_SAMPLES` / Ist-Fs aus Punkt 5 ≈ 2 667 µs) → Zeit des ersten Samples im Block.
    - `ProcessingTask`: Simulationspfad nutzt dieselbe Zeitbasis statt `HAL_GetTick() · 1000`.
    - Host-Test `CycleExtender` (DMA-Takt 10 min; zufällige Abstände bis 15 s; Pausen 18–60 s; Pausen bis 1 h; Tick bis 1 ms verzögert): monoton, max. Fehler 0 µs. Gegenprobe ohne Tick-Korrektur: Pausen-Szenarien schlagen fehl.
    - Weiterhin offen: Die Zeit ist Laufzeit seit Start, keine UTC. Für die Inter-Unit-Synchronisation (Patent: 10 µs) fehlen GNSS-PPS/PTP; der USB-Zeitabgleich (`SDS_Data::syncTimeDifference`) wird nicht angewendet.
17. 50-%-Überlappung nicht umgesetzt; Framerate 15,6/s statt 30/s (bekannt).
    Status: **bearbeitet (25.09.2026, Commit 05b3d3d)** – im Host-Test geprüft. Rechenlast am Board gemessen (28.09.2026): Proc 31,8 ms je 32-ms-Hop (P7 F15 M4 K5), Simulator zusätzlich 13,5 ms; siehe Befunde 42 und 43.
    - Aufbau: 114 liefert Hops (`HOP_SAMPLES` = 1536, 32 ms) ohne Überlappung; 118 verarbeitet jeden Hop genau einmal (IIR-Zustand läuft durch); neuer `Sensor_Unit_112/Frame_Assembler` setzt je Hop einen Analyse-Frame aus den letzten 2 Hops zusammen (3072 Samples, 50 % Überlappung, 31,25 Frames/s). Bei Lücken in der Hop-Folge beginnt das Fenster neu. `Sensor_Unit_112::nextFrame()` liefert Analyse-Frames, `nextHop()` die Hops für den READ-Modus (12 statt 24 Pakete je Mikrofon).
    - Zeitkonstanten jetzt in Sekunden in `SDS_110_Config.hpp` (`AGC_*_TAU_S`, `NS_FLOOR_TAU_S`, `HBD_FLOOR_TAU_S`, `HBD_FLOOR_RISE_DB_S`, `HBD_WARMUP_S`, `HBD_CONSISTENCY_S`, `framesFor()` für `HBD_HOLD_FRAMES`, `STATE_SMOOTH_FRAMES`, `AM_HISTORY_FRAMES`); Werte so gewählt, dass das Zeitverhalten gleich bleibt.
    - 118: NS · AGC wird als lineare Rampe je Hop angewendet. Ohne Rampe lag der Verstärkungssprung an der Hop-Grenze mitten im Frame; im Test sank dadurch bei 30 dB SNR die Peak-Ratio von 15,7 auf 5,4 und die gültigen Peilungen von 100 auf 91 %. Für den Distanzpegel (Punkt 9) wird die Verstärkung in der Frame-Mitte verwendet (`frameCenterGain()`).
    - Simulator: erzeugte je Aufruf `FRAME + 2·GUARD` Quellsamples, die Quelle sprang daher alle 64 ms um 128 Samples (bei getrennten Frames unsichtbar, mit Überlappung mitten im Frame). Jetzt wird jedes Sample genau einmal erzeugt (`generateHop()`, Vor-/Nachlauf aus dem vorigen Aufruf).
    - Host-Tests (Vergleich vorher → nachher, gleiche Zeitspanne): Peilung Drohne 30/20/10/3/0 dB gültig 100/100/100/98/91 → 100/100/100/99/90 %, Median-Fehler 0,24/0,49/0,92/1,77/2,16° → 0,19/0,43/0,88/1,55/2,21°; Reports 20/10/3/0 dB 100/100/98/51 → 100/100/100/56 %; Einzelton/Wind/Stille 0/0/0 % Reports; Breitband-Peiltest max. 0,09°; Distanzverhältnis 0,518–0,525 (vorher 0,521–0,528); stehender Ton weiterhin ~2 min erkannt. Eigentest `Frame_Assembler` (Füllen, Schieben, Zeitstempel, Neubeginn bei Lücke, `reset()`): alle Fälle bestanden.
    - Offen: Rechenlast verdoppelt sich (Schätzung aus MIGRATION_120: ~13 ms je Frame → ~40 % bei 31 Frames/s) – am Board mit den Task-Statistiken prüfen. `MIGRATION_122.md` („Overlap noch nicht implementiert“) ist damit veraltet (siehe Punkt 22).
18. Mit `NUM_UNITS = 1` ist die angezeigte Konfidenz immer ≈ 1.
    Status: **bearbeitet (25.09.2026, Commit e0ca865)** – im Host-Test geprüft, auf dem Board nicht getestet.
    - Zusätzlich gefunden: Zwei verschiedene Formeln – LCD (`SDS_Data`) `1/(1+Residuum_s)` ≈ 1, USB-Legacy-Frame `(Paare/28)/(1+Residuum·1000)` ≈ Paare/28 (Residuum in s bzw. als ms gerechnet, jeweils fast 0). Außerdem hat `CandidateLocation::ls_residual` je nach Pfad unterschiedliche Einheiten (Einzel-Unit s, `solve()` m) – jetzt dokumentiert.
    - Neu `candidateConfidence()` in 128: (Paare / max. Paare) · 1/(1 + (Residuum_Samples / `CONF_RESIDUAL_REF_SAMPLES`)²), Referenz 4 Samples. `CandidateLocation::confidence` wird in `fromBearing()` (Residuum s → Samples, max. 28 Paare) und `solve()` (Residuum m → Samples) gesetzt; `SDS_Data::setCandidate()` und der Legacy-Frame (über `UnitReport::confidence`) übernehmen den Wert.
    - Host-Test (Median): Drohne 30/20/10/3/0/−3 dB 0,96/0,86/0,68/0,53/0,49/0,29; Einzelton 0,13; Stille 0,01; Wind 0,99 (Konfidenz bewertet die Peilung, nicht die Drohnen-Detektion – die entscheidet `detected`, Punkt 24). UnitReport-Serialisierungstest (Punkt 10) weiterhin bestanden.
19. `Logger::write` nicht threadsicher; Längenbegrenzung 255 statt 256.
    Status: **bearbeitet (25.09.2026, Commit 7da4097)** – im Host-Test geprüft, auf dem Board nicht getestet.
    - Zusätzlich gefunden: `write()` prüfte den freien Platz nicht – ungelesene Daten wurden überschrieben; erreichte `head` genau `tail`, galt der Puffer als leer (bis 4 KB verloren). Derzeit ruft niemand `write()` auf, die Fehler hätten sich erst bei Nutzung gezeigt.
    - `Logger::write()`: Formatierung außerhalb der Sperre (max. `MAX_MSG` = 255 Zeichen), Einfügen in kurzem kritischen Abschnitt (PRIMASK, damit auch aus ISRs aufrufbar), Meldung ganz oder gar nicht; passt sie nicht, wird sie verworfen und gezählt (`dropped()`). Ungenutztes `#include "SDS_Data.hpp"` aus `Logger.hpp` entfernt.
    - Host-Test: 300 Zeichen → 255 Bytes ohne Nullbyte (vorher 256 mit Nullbyte); 4 Schreib-Threads × 20 000 Meldungen + 1 Leser parallel: 0 defekte, 0 vertauschte Meldungen, empfangen + verworfen = gesendet (vorher bereits die erste Meldung defekt).
20. `HBD_ML_Model_Data.hpp` (≈ 185 KB) nirgends eingebunden; Modell erwartet Cepstrum-Merkmale, die 122 nicht liefert. `SDS_SimDrone` ebenfalls ungenutzt.
    Status: **bearbeitet (27.09.2026, Commit 5017f27)** – Datei gelöscht (Befund 36). An ihrer Stelle `ML124_Model_Data.hpp` aus ML_Test `export.py` (Modell `k5_h48_d3`, 169 Merkmale aus 122, Kontext 5), eingebunden in 124 mit den Stufen HBD / Schatten / ML (`ML124_Config.hpp`, Standard Schatten). Host-Test `t_ml124`. Vergleich mit dem HBD: `doc/Vergleich_HBD_ML124.md` (Abnahme nicht erfüllt, Stufe ML bleibt aus). `SDS_SimDrone` weiter ungenutzt.
21. MPU-Region 0 macht SRAM1/2 komplett uncached (für DMA nötig, kostet Leistung).
    Status: **bearbeitet (25.09.2026, Commit 0621e76)** – gebaut und Platzierung geprüft, auf dem Board nicht getestet (Leistungsgewinn nicht gemessen).
    - Zusätzlich gefunden: Die Region war Strongly-ordered (TEX0/C0/B0), nicht nur uncached – jeder Zugriff geordnet, nicht ausgerichtete Zugriffe unzulässig. Betroffen waren u. a. der Stack aller ISRs (`_estack` = 0x20050000), Teile der Task-Stacks (Ende von `ucHeap`), USB-Puffer, Logger, `SDS_Data`.
    - DMA-Nutzer geprüft: SAI (Puffer mit Cache-Pflege), ETH-Deskriptoren (im DTCM, nie gecacht; ETH wird nicht gestartet), DMA2D (nur SDRAM-Framebuffer, Region 2), USB OTG FS (ohne DMA). SD/FatFS: DMA-Pfad vorhanden, aber keine DMA-Streams/IRQs eingerichtet und FatFS ungenutzt.
    - Linker: `RAM` 304 kB (DTCM + SRAM1), neu `RAM_NC` 16 kB (SRAM2) mit Sektion `.dma_nocache`; `_estack` = 0x2004C000 (Stack jetzt gecacht). MPU-Region 0: nur SRAM2, Normal nicht cachebar (TEX=1, C=0, B=0). SRAM1 fällt unter die Standard-Speicherkarte (Write-Back, Write-Allocate).
    - SAI-DMA-Puffer über `SDS110_DMA_SECTION` in `.dma_nocache`; Probe-Link mit aktivem ProcessingTask: `dmaBuffer_` (8 kB) an 0x2004C000.
    - Hinweis für FatFS (falls genutzt): SD-DMA erst einrichten (Streams, IRQs), Puffer dann in `.dma_nocache` legen oder `ENABLE_SD_DMA_CACHE_MAINTENANCE` mit 32-Byte-ausgerichteten Puffern verwenden (STs Invalidierung auf abgerundete Adressen kann sonst Nachbardaten verwerfen).
22. `MIGRATION_*.md` teilweise veraltet (z. B. Linker-Sektion existiert bereits).
    Status: **bearbeitet (25.09.2026, Commit 9941759)**.
    - Herkunftstabellen („Neu ← Aus SDS“) als Historie unverändert. Aktualisiert wurden Stand-, Speicher- und „Offene Punkte“-Abschnitte: Erledigtes durchgestrichen bzw. markiert mit Verweis auf die Befundnummer, Offenes mit Befund- und Statusangabe.
    - `MIGRATION_112.md` (Offene Punkte 1–6, Aufruf aus main.c), `MIGRATION_120.md` (Speicher gemessen mit `sizeof`, Laufzeit mit Überlappung, Offene Punkte; alte Azimut-Kalibrierung nach Befund 23 nicht übertragbar), `MIGRATION_122.md` (Speicher, ML-Modell-Stand, Overlap erledigt), `MIGRATION_MODEL.md` (`setCandidate`-Signatur), `MIGRATION_TASKS.md` (Stand, main.c-API, erledigte Hinweise), `Infrastructure/README.md` (tatsächlicher Inhalt), `Harness/README.md` (Hops, Rohformat, Szenario Silence), `Core/SDS_110/README.md` (Framing).
    - Nicht geändert: `doc/ADUA_Design.md` (gehört zu den zurückgestellten Punkten 3/4) und `doc/Findings.md` (eigene Notizen).
    - Offene Frage: Nachrichtentyp für das Tracking-Feedback – `MIGRATION_120.md` nennt 4, `Output_Interface_130.hpp` 6.

## Neu aus dem Host-Test (25.09.2026)

23. **Peilung um 180° verdreht** – `crossCorrelate()` bildet R = X_i·X_j*, dessen Peak bei τ = ((p_j − p_i)·u)/c liegt; `estimateBearing()` und `srpScan()` rechnen aber mit τ = ((p_i − p_j)·u)/c. Im Host-Test zeigt jede gültige Peilung (Wind, breitbandig) 179,9° neben dem wahren Azimut.
    Status: **bearbeitet (25.09.2026, Commit 187441b)** – im Host-Test geprüft, auf dem Board nicht getestet. `Correlation_Processing_Module_126`: Fernfeldmodell in `estimateBearing()` (Normalgleichungen, Residuum) und `srpScan()` (`pairDx_/pairDy_`) auf τ_ij = ((p_j − p_i)·u)/c umgestellt; `crossCorrelate()` unverändert (liefert τ_ij = t_i − t_j, gleiche Konvention wie `128::solve()`). Peiltest (breitbandige Quelle, alle Bänder, Azimut 0…345° in 15°-Schritten): vorher 180° Fehler bei allen 24 Richtungen, nachher 24/24 gültig, max. Fehler 0,08° (TDOA-LS) bzw. 0,07° (SRP). Hinweis: die in MIGRATION_120 erwähnte alte Azimut-Kalibrierung (+12°, ×0,98) stammt aus dem SRP-Code vor der Migration und muss nach dieser Korrektur neu gemessen werden.
24. **Band-Selektion in 124 bei Rauschen zu großzügig** – Das Gate `g = score / finalScoreThreshold` ist schon bei Rauschen offen (Score ≈ 0,5 > 0,48), und `HBD_BAND_SNR_DB = 8 dB` liegt nahe am Maximum von Rauschbins im Band. Folge: `SDS_Data::detected` bei Stille 22 %, bei Einzelton 54 %. Parameter mit Aufnahmen abstimmen (vgl. MIGRATION_120 „Offene Punkte 1“), evtl. Gate an `droneDetected` koppeln.
    Status: **bearbeitet (25.09.2026, Commit b4a18f4)** – im Host-Test geprüft, auf dem Board nicht getestet.
    - `Machine_Learning_Module_124`: Bänder ohne Harmonische erhalten höchstens `HBD_GATE_FLOOR · q_b` (< θ_sel). Das Gate hängt nicht mehr am Score, sondern ist offen, solange der HBD in den letzten `HBD_HOLD_FRAMES = 16` Frames (~1 s) eine Drohne erkannt hat; sonst p_b · `HBD_GATE_FLOOR`.
    - `HBD`: Anlaufzeit `warmupFrames = 48` (~3 s) ohne Entscheidung – der Floor schwingt nach dem Start (AGC-Anlauf) bis ~Frame 40 ein und löste vorher auch bei Wind aus. Folge: Nach jedem Start werden die ersten ~3 s keine UnitReports gesendet.
    - Host-Test (6 Richtungen, ab Frame 50): Reports Drohne 20/10/3/0 dB: vorher 99/97/85/67 %, nachher 100/100/98/51 %; Einzelton 3 → 0 %, Wind 22 → 0 %, Stille 0 → 0 % (`detected` 68 → 0 %). Langlauf 3 900 Frames je Rauschszenario: 0 Reports. Anteil selektierter Bänder mit Harmonischer: ~42 % → 87–99 %. Peilung Drohne (Punkt-8-Test): 20 dB Median 0,65° → 0,49°, 0 dB gültig 33 % → 91 %.
    - Hinweis: Eine gültige Peilung allein ist kein Detektionskriterium – Wind (Punktquelle) wird zu 77 % gültig gepeilt, gesendet wird nur bei `detected`.
25. **Modus-Werte in `PC_Monitor_Test.ptp` vertauscht** – Die Test-Makros senden „Calibrate“ = 3 und „Read“ = 2; die Firmware verwendet seit dem ersten Commit `DETECT = 1, CALIBRATE = 2, READ = 3` (`SDS_Mode`). Entweder sind die Makros falsch beschriftet oder der PC-Monitor nutzt eine andere Zuordnung. Gegen den PC-Monitor prüfen, dann `.ptp` oder `SDS_Mode` angleichen.
    Status: offen

## Neu aus der Analyse vom 27.09.2026 (26–38)

Quelle mit Messungen und Begründungen: `doc/Analyse_27_09_2026.md` (Abschnitte 3 und 4). Hier nur
Befund, Folge und Status. „Gemessen“ = im Host-Test nachgewiesen, „Code“ = am Quelltext
nachvollzogen, „plausibel“ = nicht nachgewiesen.

26. **TDOA-LS-Peilung mehrdeutig bei f0 über ca. 400 Hz** (126 `crossCorrelate()`, `estimateBearing()`; hoch, gemessen) – Ist 1/f0 kürzer als das Lag-Fenster ±61 Samples, hat jedes Paar zwei fast gleich hohe Spitzen; die falsche Peilung wird als gültig gemeldet. 95-%-Fehler bei 420 Hz 25°, bei 650 Hz Median 105°; SRP bleibt unter 4°. Echte Drohnen (Bebop) haben ihre stärksten Linien bei 390–530 Hz.
    Status: **bearbeitet (28.09.2026)** – Nachgemessen mit dem 200-mm-Array (Fenster ±30, `m_bearing_f0`, 20 dB): die Mehrdeutigkeit bleibt ab ~400 Hz, schwächer als beim 400-mm-Array (1000 Hz: 95 % 137,5°, 117 grobe Fehler > 30° unter 649 gültigen). Abhilfe in 126 `estimateBearing()`, Peilung bleibt TDOA-LS:
    1. Die Paarkorrelationen (±32 Lags) werden in jedem Frame gesichert (bereits berechnete Werte).
    2. Robuste LS: liegt ein Paar mehr als 8 Samples neben der Lösung (eine vertauschte Spitze liegt ≥ 12 Samples daneben), wird das Paar mit dem größten Residuum verworfen und neu gelöst.
    3. Bleibt die Lösung inkonsistent oder sind zu wenige Paare eindeutig: Richtung aus dem SRP-Scan der gesicherten Korrelationen; abweichende Paare nehmen die Spitze ±5 Samples um die vorhergesagte Verzögerung; sonst ungültig (keine falsche Peilung).
    Ergebnis 20 dB: 1000 Hz 95 % 3,3°, 0 grobe Fehler, 78 % gültig; 480 Hz 95 % 6,4° (vorher 20,6°); Drohne über SNR unverändert bis 3 dB, bei 0 dB 94 statt 98 % gültig. Host-Test `t_bearing_f0` (Gegenprobe: alter Stand fällt durch). Offen: Vorhersage aus dem Feedback Id 8 als zusätzliche Führung (A34); am Board nicht geprüft.
27. **Lücken im Hop-Strom werden nicht erkannt** (114 `pushBlock()`, `acquireFree()`; hoch, Code) – Im Verwerf-Pfad wird keine `frame_id` verbraucht; nach verworfenen Blöcken bleibt die Folge lückenlos, der Frame_Assembler setzt Frames aus zeitlich getrennten Hälften zusammen (z. B. nach CALIBRATE → DETECT). Zusätzlich wird der Block verworfen, auch wenn `acquireFree()` gerade einen Puffer geliefert hat.
    Status: **bearbeitet (28.09.2026)** – `pushBlock()` überspringt beim ersten verworfenen Block eine `frame_id`; Frame_Assembler und PC-Monitor (READ, Hop-Nummer) erkennen die Lücke. Host-Test `t_hop_gap` (Gegenprobe: alter Stand fällt durch). Zusammen mit Befund 42 neue Merkmalsversion `3a685d01f22f3eea`; Merkmale bitgleich (4 Referenzdateien, `export.py --same-as`), Modell neu exportiert.
28. **Task-Takt 40 ms passt nicht zu 32-ms-Hops** (`ProcessingTask`; mittel, Code) – Simulation ≤ 25 statt 31,25 Hops/s, Halte- und Anlaufzeiten gedehnt; READ verwirft Hops.
    Status: **bearbeitet (27.09.2026, Commit 84897b2)** – Host-Test `t_hopclock`, auf dem Board läuft der Task, Rate nicht gemessen. `HopClock` + `osDelayUntil` in der Simulation (höchstens 4 Hops nachholen, bei Überlast 1), mit Hardware Warten auf den Hop (Thread-Flag aus dem 116-Interrupt), READ sendet alle bereiten Hops. Überlastschutz: Commit 1bd9924.
29. **Getter von `SDS_Data` liefern bei Sperr-Timeout (2 ms) den Wert 0** (`SDS_Data.hpp`; hoch, plausibel → am Board beobachtet) – `getSimulation()` = 0 schaltete auf SAI-DMA (HardFault-Pfad Blocker 2); Setter verwerfen Werte. Am Board bei CPU-Überlast als springende Anzeigewerte gesehen.
    Status: **bearbeitet (28.09.2026)** – Seit Commit 4b93f70 gibt es `tryGetValue()`/`tryGetSimulation()`, der ProcessingTask behält bei Timeout den letzten Simulationswert, und der SAI-Start ohne DMA ist abgefangen. Neu: Alle Getter lesen bei Timeout den aktuellen Wert ohne Sperre statt 0 (Werte ≤ 4 Byte liest der M7 atomar; größere Strukturen nur Anzeige/USB). Setter warten nach dem ersten Timeout einmal 10 ms, erst dann wird verworfen (errorFlag 999). Die Timeouts werden gezählt, das LCD zeigt sie als „Lock n“ (gelb, wenn > 0). Das Mutex-Attribut `osMutexPrioInherit` ist gesetzt (FreeRTOS-Mutexe vererben ohnehin). Host-Test `t_sds_data` mit Fehlerinjektion im Shim; am Board nicht geprüft.
30. **Start trotz fehlgeschlagener Initialisierung** (116, `SDS110_Init`, `ProcessingTask`; mittel, Code) – Nach Codec-Fehler bleibt die SAI mit CubeMX-Takt (53,6 kHz) aktiv.
    Status: offen
31. **Wechsel Simulation ↔ Hardware mitten im Hop** (`ProcessingTask`; mittel, Code) – Ein Hop enthält echte und simulierte Daten.
    Status: offen – seit Commit 84897b2 erzeugt die Simulation immer ganze Hops; beim Wechsel Hardware → Simulation bleibt der angefangene DMA-Hop in 114 aber stehen und wird mit simulierten Blöcken aufgefüllt.
32. **Nur ein Kommando je USB-Paket** (`USBTask`; mittel, Code) – Zusammengefasste Kommandos gehen still verloren, geteilte lösen Fehler aus.
    Status: **bearbeitet (28.09.2026)** – Am Board beobachtet: Unit-ID (Id 5) und SRP (Id 6) blieben ohne Wirkung, sobald das Feedback (Id 8) mit bis zu 31/s lief; Windows fasst dicht folgende Schreibvorgänge zu einem USB-Paket zusammen. Neu: `Infrastructure/Utils/CommandAssembler.hpp` setzt den Bytestrom zusammen (Resync auf das Magic, Länge 16…56, Rest über die Paketgrenze, veralteter Rest nach 20 ms verworfen); der ISR reicht ganze Pakete (64 Byte + Empfangszeit) weiter, Queue 16 Pakete. Host-Test `t_usb_commands`; am Board nicht geprüft.
33. **UnitReport-Zeitstempel nur in ms (uint32)** (`Output_Interface_130.cpp`; mittel, Code) – Die µs-Zeit aus Befund 16 erreicht den PC nicht; Inter-Unit-TDOA (10 µs) damit unmöglich.
    Status: **Firmware bearbeitet (27.09.2026)** – Host-Test `t_unit_report`, am Board nicht geprüft; PC-Monitor offen.
    - UnitReport jetzt Message id 5 (ersetzt id 4): Kopf `[unit u16][time_us u64][src u8][bearing f32][residual f32][pairs u8][nsel u8][level f32]`, little-endian, danach Bänder (max. 51). `src`: 0 Laufzeit, 1 UTC vom PC, 2 GNSS-PPS (vorgesehen).
    - UTC-Abgleich: USB-Kommando Typ 7 (seit 28.09.2026 24 Byte: UTC in µs als u64 big-endian + Lufttemperatur, `doc/ICD_SDS_PC_Monitor.md`). Die Firmware merkt sich im USB-Interrupt die Empfangszeit und setzt Versatz = UTC − Laufzeit (`UtcClock`, `SDS_Data::utcOffset_`); der Report rechnet den Frame-Beginn damit in UTC um. Genauigkeit ~1 ms (USB-Laufzeit, nicht korrigiert).
    - Nachrichtenkopf (`timestamp` u32) und Legacy-Detect-Frame (id 1) bleiben ms; nach dem Abgleich UTC-ms modulo 2^32.
    - Für ≤ 10 µs zwischen Einheiten (FSL9 A6): GNSS-PPS in HW-Version 2 (Quelle 2).
34. **Sammelpunkt Kleinigkeiten** (niedrig, Code) – Feedback 150 → 126 wird nie zurückgesetzt (`clearFeedback()` nur in `init()`; seit 28.09.2026 behoben: Feedback kommt über USB Id 8, `pollFeedback` setzt nach 2 s ohne Feedback zurück, Host-Test `t_feedback`), `pred_azimuth_deg`/`TDOA_WINDOW_S` ungenutzt; TX-Ring sendet nach USB-Trennung alte Daten zuerst; `sendLogging()` Id in Byte 4 statt 7; `SPEED_OF_SOUND` fest trotz Kommentar „temperaturkorrigiert“ (seit 28.09.2026 behoben: c aus der Lufttemperatur im Sync-Kommando Typ 7, `SoundSpeed.hpp`, Host-Test `t_sound_speed`); SAI-Taktflanke `FALLINGEDGE` am Board prüfen (mit 4 und 6); SAI-Fehlercallback zählt teils doppelt.
    Status: offen
35. **READ-Modus liefert keine Rohdaten und verwirft Hops** (112/`USBDriver`; hoch für AP 8) – Gesendet wird nach Bandpass, NS und AGC (`sds_features` würde 118 doppelt anwenden); AGC bis 32-fach läuft beim PC über; Hop-Verluste ohne Kennung.
    Status: **bearbeitet (28.09.2026)** – Der Hop-Verlust durch den Task-Takt ist seit 28 behoben. Neu: `Sensor_Unit_112::nextHop()` liefert Rohdaten (ohne 118); der Kopf der Read-Nachricht trägt micNr u8, blockNr u8 und die Hop-Nummer u16 (`frame_id`), per `static_assert` auf ICD 5.3 festgelegt. Der PC-Monitor zählt Lücken und speichert Aufnahmen als 8-kanaliges WAV (24 Bit) für `sds_features`. USB Full Speed reicht weiter nicht für alle 96 Nachrichten je Hop (~1,6 MB/s): Lücken sind jetzt sichtbar, aber nicht vermieden. Am Board nicht geprüft.
36. **`HBD_ML_Model_Data.hpp` veraltet** (ML-Kette; mittel) – Aus einer externen Sitzung, trainiert ohne Überlappung, 171 Eingänge mit Cepstrum.
    Status: **bearbeitet (27.09.2026, Commit 5017f27)** – gelöscht und durch `ML124_Model_Data.hpp` (ML_Test `export.py`, `k5_h48_d3`) ersetzt; siehe Befund 20 und `doc/Vergleich_HBD_ML124.md`.
37. **HBD-Vergleich schließt die HBD-Anlaufzeit nicht aus** (ML_Test `ml124_data.py`; mittel) – Verworfen werden 31 Frames, der HBD entscheidet erst nach 94; die HBD-Werte in `Training_ML124.md` § 3 fallen zu niedrig aus.
    Status: offen – Vergleich ab Frame 94 wiederholen.
38. **Prüfbereich f0 und Validierungstrennung** (ML_Test; mittel) – Synthetische Drohnen nur BPF 80–350 Hz, echte teils darüber (vgl. 26); Umwelt-Clips der Validierung aus demselben Pool wie im Training.
    Status: offen – BPF bis ca. 700 Hz trainieren, `m_bearing_drone`/`m_selection` bis 650 Hz messen.

## Neu vom Board (27.09.2026)

Testaufbau: STM32F746G-DISCO mit dessen Display RK043FN48H, bis die Platine mit den 8 Mikrofonen
vorhanden ist.

39. **Bild zittert in allen Modi (LTDC-Pins zu langsam)** – Alle 28 LTDC-Pins (R/G/B-Daten, HSYNC, VSYNC, DE, CLK) waren aus dem CubeMX-Preset mit `GPIO_SPEED_FREQ_LOW` konfiguriert (`HAL_LTDC_MspInit`, `SDS.ioc` ohne `GPIO_Speed`). Bei 9,6 MHz Pixeltakt sind die Flanken damit zu flach, das Panel tastet Takt und Daten unsicher ab: Das Bild zitterte, als würde der Speicher überschrieben. Das ST-BSP für das DISCO nutzt eine hohe Geschwindigkeitsstufe; die FMC-Pins zum SDRAM standen bereits auf `VERY_HIGH`.
    Status: **bearbeitet (27.09.2026, Commit 604b34d)** – auf dem Board geprüft: Das Bild flackert nicht mehr.
    - `stm32f7xx_hal_msp.c`, `HAL_LTDC_MspInit`: `GPIO_SPEED_FREQ_LOW` → `GPIO_SPEED_FREQ_HIGH` (5 Pin-Gruppen). `SDS.ioc`: `GPIO_Speed=GPIO_SPEED_FREQ_HIGH` für alle LTDC-Pins, damit CubeMX die Einstellung beim Neugenerieren behält.
    - Vorher ausgeschlossen, am Board: CPU-Überlast (LCD-Zyklus 3 s → 9 ms nach Commit 1bd9924, Flackern blieb); LTDC-FIFO-Unterlauf und DMA2D-Fehler (Zähler nach Commit 24b3c7b alle 0). Per Code-Prüfung: Linkerskript (`STM32F746NGHX_FLASH.ld`, Framebuffer-Bereich frei), SDRAM-Timing und Modusregister (CAS 3), LTDC-Timing und -Takt (9,6 MHz, 59,3 Hz, passend zum DISCO-Display).
    - Nebenbei geändert (Commit 24b3c7b): Puffertausch in der vertikalen Austastlücke statt `RELOAD_IMMEDIATE`, DMA2D-Pausen je Burst.
    - Diagnose bleibt im Code: LCD-Zeile y = 160 `FB <Zyklen> x<Streifen>` (Prüfsumme des angezeigten Puffers, Commit 1d36970), y = 180 `LTDC U.. T.. D2D ../.. VB..`, Testbild mit `LCDTask::kTestPattern`.
    - Für die eigene Platine: siehe Hinweis bei Blocker 3.

## Neu aus der FSL9-Prüfung (27.09.2026)

Quelle: `doc/Traceability_FSL9.md`.

40. **Arraydurchmesser 400 mm statt 200 mm** (FSL9 §1, A3; hoch, Code) – `MIC_RADIUS_M = 0.20` war als Durchmesser gelesen worden, ist aber der Radius; das Array hatte damit 400 mm Durchmesser. Sollwert laut Rückfrage: Radius 100 mm, Durchmesser 200 mm.
    Status: **bearbeitet (27.09.2026, Commit 28a7279)** – Host-Tests bestanden, am Board nicht gemessen.
    - `SDS_110_Config.hpp`: `MIC_RADIUS_M = 0.10`; `SRP_MAX_LAG` 64 → 32 (größte Verzögerung 0,2 m / c · fs · 1,1 = 30,8 Samples), `static_assert` gegen ein zu kleines Fenster. 126: Schnellpfad rechnet ±32 statt ±64 Lags (`WIN_HALF = SRP_MAX_LAG`), `estimateBearing()` am Host 0,13 statt 0,28 ms; am Board wird „K GCC“ etwa halbiert erwartet (bisher 8,5 ms).
    - Peilung (`m_bearing_drone`, DroneStatic): Fehler etwa doppelt so groß, Median 30 dB 0,37° (vorher 0,19°), 10 dB 1,85° (0,91°), 0 dB 4,36° (2,07°); gültig bei 0 dB 98 % (90 %). Werte in `doc/Host_Tests.md`.
    - Merkmalsversion 837ff89cbda34b21 → cadc6552b54a4a01 (Config geht in den Hash ein). Die Merkmale hängen nicht von der Geometrie ab (122 rechnet auf dem Referenzmikrofon): altes und neues `sds_features` liefern auf den Referenzsignalen bitgleiche Merkmale. Modell `k5_h48_d3` daher ohne neues Training neu exportiert (ML_Test `export.py --same-as`).
    - Zu Befund 26: Mit dem halben Lag-Fenster sollte die Mehrdeutigkeit erst bei etwa doppelter f0 (ca. 800 Hz) auftreten – nicht gemessen.
    - Host-Makefile: Objekte hingen nicht von den Headern ab; nach der Änderung an `SDS_110_Config.hpp` lief `t_gcc_direct` zunächst mit veralteten Objekten (±61 statt ±31 Samples). Jetzt `-MMD -MP`.

41. **Azimut ab der x-Achse gegen den Uhrzeigersinn statt ab Nord** (FSL9 §6, FIG. 5, A28; mittel, Code) – 126 lieferte atan2(uy, ux) im Array-System; der PC-Monitor zeichnete den Wert als Kompass-Azimut. Das Ziel erschien gespiegelt und gedreht (PC-Monitor Befund P8).
    Status: **bearbeitet (28.09.2026)** – Festlegung: 0° = Nord, im Uhrzeigersinn, Mikrofon 0 zeigt nach Nord. Host-Test `t_azimuth`, am Board nicht geprüft.
    - `Infrastructure/Utils/Azimuth.hpp`: Umrechnung Array-System (x = Mikrofon 0 = Nord, y = West bei Nummerierung gegen den Uhrzeigersinn) ↔ Azimut; Schalter `MIC_NUMBERING_CLOCKWISE`, falls die Platine im Uhrzeigersinn nummeriert ist.
    - Umgestellt: Peilung und SRP (126), Lokalisation (128), Simulator (wahrer Azimut ist jetzt Kompass), LCD-Radar (Nord oben, „N“) und Azimutfehler mit Umlauf (359°/1° = 2°). 114 und Config bleiben unverändert (Merkmalsversion unverändert).

42. **MicFrame-Kanäle im selben D-Cache-Satz** (114 `MicFrame::data[8][1536]`; hoch, Rechenlast, gerechnet) – Der Zeilenabstand 1536 × 4 = 6144 Byte ist ein Vielfaches von 1 KB (Weggröße des 4-KB-D-Cache, 4-fach assoziativ, 32-Byte-Zeilen). Alle 8 Kanäle einer Sampleposition fallen in denselben Satz; `pushBlock()` schreibt je Sample alle 8 Kanäle nacheinander, jeder Schreibzugriff verdrängt eine gleich wieder gebrauchte Zeile (Füllen + Rückschreiben aus dem SDRAM, 12 288 je Hop). Trifft den Simulator und den DMA-Pfad 116. Hinweis am Board: Simulator 13,5 ms je Hop, Faktor 106 zum Host statt ~35 wie die übrigen Stufen.
    Status: **bearbeitet (28.09.2026)** – `MicFrame::data[NUM_MICS][HOP_SAMPLES + MIC_ROW_PAD]`, `MIC_ROW_PAD = 8` (eine Cache-Zeile): Kanalabstand 6176 Byte, `static_assert` gegen Vielfache von 1 KB. Merkmale bitgleich (`sds_features` auf 4 Referenzdateien, `check_features`, `m_bearing_drone`, `m_selection` bitgleich); neue Merkmalsversion `3a685d01f22f3eea`, Modell k5_h48_d3 mit `export.py --same-as` neu exportiert (Gewichte unverändert). Wirkung am Board noch nicht gemessen.
43. **Debug-Build (-O0): `#pragma GCC optimize("O2")` schaltet kein Inlining ein** (`DspOptimize.hpp`; mittel, Rechenlast, am Objektcode geprüft) – Kleine Hilfsfunktionen, Lambdas und `std::sqrt`/`std::log`/`std::fmin` in inneren Schleifen bleiben Funktionsaufrufe; 114 band das Pragma gar nicht ein (`pushBlock` mit -O0).
    Status: **teilweise bearbeitet (28.09.2026)** – `SDS110_FORCE_INLINE` (always_inline, wirkt auch unter -O0); Simulator ohne Aufrufe in der inneren Schleife; 114 bindet `DspOptimize.hpp` ein (die Zeile ist aus der Merkmalsversion herausgefiltert). Offen: 118/122/124/126 im Debug-Build (Aufrufe je Bin); Release (-Os) ist nicht betroffen. Am Board die Stufenzeiten im Release-Build vergleichen.
44. **Rechenlast P und F: Kopieren im SDRAM, FFT-Tabellen im Flash** (112/118/122; hoch, am Board gemessen) – Aufteilung am LCD (28.09.2026): 118 4,3 ms, Frame_Assembler 2,6 ms (nur `memmove` + `memcpy` von 96 KB im SDRAM), 122 Referenzkanal 3,4 ms, je weiteres Mikrofon 1,69 ms (Fenster + FFT 4096 + Kopie, ×7 = 11,8 ms).
    Status: **bearbeitet (28.09.2026)** – (1) Frame_Assembler ohne Schieben: zwei Hop-Slots im Ring (mit Zeilenfüllung wie 114), `AnalysisFrame` zeigt auf die beiden Teile; 118 schreibt je Kanal direkt in den Slot (`beginHop` / `process(frame, out)` / `commitHop`) und rechnet in einem internen 6-KB-Puffer statt dreimal im SDRAM; 122 fenstert beide Teile getrennt. (2) Twiddle-Tabellen der RFFT 4096 (2 × 16 KB, für 122 und 126) beim Start ins interne RAM (`FftTables.hpp`, Schalter `SDS110_FFT_TABLES_IN_RAM`); dafür FreeRTOS-Heap 64 → 48 kB (belegt ~28 kB), RAM 97,8 %. Merkmale bitgleich (4 Referenzdateien seit dem Trainingsstand, `check_features`, Board-Pfad = Werkzeug-Pfad in `t_frame_assembler`, Messprogramme bitgleich); Merkmalsversion `5e2aeca1b90a4d17`, Modell neu exportiert (`--same-as`). Wirkung am Board noch nicht gemessen. Nicht umgesetzt: 118 zu einem Durchgang zusammenfassen (CMSIS-`arm_rms_f32` summiert anders -> nicht bitgleich, Neutraining).
