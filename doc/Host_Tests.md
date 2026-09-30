# SDS_110 – Host-Tests

Sammlung aller Tests, die bei der Code-Analyse und der Bearbeitung der Befunde
(`doc/Analyse_Befunde.md`) verwendet wurden. Sie laufen unter Linux/WSL2 mit `g++` –
ohne Board, ohne HAL, ohne RTOS.

Referenzergebnisse: Stand 25.09.2026, Code-Stand Commit `0dc8150`.
Seit 27.09.2026 erzeugt der Simulator Rauschen und Oszillatoren anders (Irwin-Hall statt
Box-Muller, Zeiger in float statt `sin()` in double – Rechenzeit auf dem Board). Die Messwerte
weichen dadurch leicht ab; neu gemessen und eingetragen sind `t_bearing_broadband`,
`m_bearing_drone` und `m_selection`, die übrigen liegen innerhalb weniger Prozent der Tabellen.
Seit 27.09.2026 (FSL9 A3) hat das Array 200 mm Durchmesser (`MIC_RADIUS_M = 0.10`, vorher
0,20 m). Die Laufzeitunterschiede halbieren sich, die Peilfehler verdoppeln sich etwa; alle
Peilwerte unten sind mit 200 mm neu gemessen, die Werte für 400 mm stehen in Klammern.

---

## 1. Überblick

| Art | Programm | Bezug (Befund) | Inhalt |
|---|---|---|---|
| Prüfung | `t_scaling` | 6 | Rohdaten-Skalierung in 114 (24 Bit linksbündig) |
| Prüfung | `t_frame_assembler` | 17, 44 | Analysefenster mit 50 % Überlappung; Board-Pfad (118 schreibt in den Slot) = Werkzeug-Pfad |
| Prüfung | `t_unit_report` | 10, 33 | Serialisierung des UnitReport (128 Byte), µs-Zeitstempel und UTC-Abgleich |
| Prüfung | `t_sdram_selftest` | 14 | Logik des SDRAM-Selbsttests |
| Prüfung | `t_timebase` | 16 | 64-bit-Erweiterung des DWT-Zählers |
| Prüfung | `t_logger` | 19 | Logger-Ringpuffer mit mehreren Schreibern |
| Prüfung | `t_bearing_broadband` | 23, 8 | Peilung einer breitbandigen Quelle, 24 Richtungen |
| Prüfung | `t_ml124` | 20, 36 | MLP in 124: Merkmalsversion, C++ ↔ Keras, Kontextstapel, Stufen |
| Prüfung | `t_hopclock` | 28 | Hop-Takt der Simulation im ProcessingTask (31,25 Hops/s) |
| Prüfung | `t_gcc_direct` | – | Schnellpfad der GCC-PHAT in 126 gleich IFFT, Zeit je Frame |
| Prüfung | `t_sound_speed` | 34 | Schallgeschwindigkeit aus der Lufttemperatur (Sync Typ 7), Peilung −40…+60 °C |
| Prüfung | `t_feedback` | 34 | Feedback der Tracking-Einheit (USB Id 8): Kodierung, Ablauf nach 2 s, Wirkung in 126 |
| Prüfung | `t_usb_commands` | 32 | Kommandos aus dem USB-Bytestrom: mehrere je Paket, geteilt, Resync, veralteter Rest |
| Prüfung | `t_local_position` | – | Standort lokal Ost/Nord/Oben (USB Id 10, Nachricht Id 6): Grundwert Ursprung, Kodierung, Grenzen, Zurücksetzen |
| Prüfung | `t_sds_data` | 29 | Sperr-Timeouts in `SDS_Data`: Getter liefern den Wert statt 0, tryGet false, Setter mit zweitem Versuch |
| Prüfung | `t_bearing_f0` | 26 | Mehrdeutigkeit bei hohem f0: f0 = 180/480/1000 Hz, ≤ 1 % grobe Fehler, 95 % ≤ 10° |
| Messung | `m_bearing_f0` | 26 | Peilung über f0 = 120 … 1000 Hz (SNR als Argument) |
| Prüfung | `t_hop_gap` | 27, 42 | Verworfene DMA-Blöcke: frame_id springt, Frame_Assembler beginnt neu; Kanalabstand kein Vielfaches von 1 KB |
| Prüfung | `t_azimuth` | 41 | Azimut 0° = Nord, im Uhrzeigersinn, Mikrofon 0 = Nord; Kette in 8 Richtungen; Nordabgleich (Id 9) |
| Prüfung | `t_sim_flyby` | – | Simulator-Auswahl über USB Id 3; Szenario FlyBy (5 s Flug, 5 s Pause): Geometrie, Ablauf, Peilung und HBD |
| Messung | `m_overview` | 7, 8, 23, 24 | Alle Simulator-Szenarien + Empfindlichkeit |
| Messung | `m_hbd_diag` | 7 | HBD-Rauschboden und SNR je Harmonischer |
| Messung | `m_bearing_drone` | 8, 17, 23, 24 | Peilung eines Drohnensignals über die volle Kette |
| Messung | `m_selection` | 24 | Band-Selektion, Detektion, Reports, Fehlalarme |
| Messung | `m_distance` | 9, 17 | Distanzschätzung aus dem Pegel |
| Messung | `m_confidence` | 18 | Konfidenz und Peilresiduum |
| Messung | `m_longrun` | 7, 17 | Langzeitverhalten des HBD-Rauschbodens (~5 min) |

- **Prüfungen (`t_*`)** haben ein festes Kriterium und liefern Exit-Code 0 (bestanden) oder ≠ 0.
- **Messungen (`m_*`)** geben Tabellen aus; sie werden mit den Referenzwerten in Abschnitt 6
  verglichen. Sie dienen zum Abstimmen von Parametern und zum Erkennen von Rückschritten.

---

## 2. Aufruf

```bash
make -C test/host              # alle Programme bauen (Ausgabe: build/test_host/)
make -C test/host check        # alle Prüfungen, Abbruch mit Fehler, wenn eine fehlschlägt
make -C test/host measure      # alle Messungen (einige Minuten)
make -C test/host longrun      # Langzeitmessung (einige Minuten)
make -C test/host clean
make -C test/host gegenprobe TEST=t_name CODE=/pfad   # siehe unten
```

Einzelne Programme direkt starten, z. B. `build/test_host/m_bearing_drone 0`
(Argumente siehe Abschnitt 5 bzw. Kopfkommentar der Quelldatei).

Voraussetzungen: `g++` mit C++17, `make`. Die Firmware-Toolchain wird nicht benötigt.

### Gegenprobe gegen einen älteren Stand

Mit `CODE=…` übersetzt das Makefile die aktuellen Tests gegen einen anderen Code-Stand.
So lässt sich zeigen, dass ein Test den Fehler vor der Korrektur tatsächlich erkennt:

```bash
git worktree add --detach /tmp/sds_alt ad18673~1
make -C test/host gegenprobe TEST=t_scaling CODE=/tmp/sds_alt
git worktree remove --force /tmp/sds_alt
```

Die Gegenprobe baut nach `build/test_host_gegenprobe/`. Erwartet wird, dass der Test gegen den
alten Stand fehlschlägt (make meldet dann einen Fehler).

Ergebnisse (Stand vor der jeweiligen Korrektur → Test schlägt fehl, aktueller Stand → besteht):

| Test | Stand | Ergebnis |
|---|---|---|
| `t_scaling` | `ad18673~1` (vor Befund 6) | Vollaussteuerung ±256 statt ±1 – FEHLER |
| `t_unit_report` | `d572820~1` (vor Befund 10) | 57/64 Bänder → nsel 57/64, 130/144 Byte – FEHLER |
| `t_logger` | `7da4097~1` (vor Befund 19) | 256 Byte mit Nullbyte; erste Meldung defekt – FEHLER |

Grenzen: Tests, die die Hop-Kette aus Befund 17 benutzen (`t_bearing_broadband`, alle `m_*`),
übersetzen gegen Stände vor Commit `05b3d3d` nicht (API `generateHop`, `Frame_Assembler`).
Für die Gegenprobe zu Befund 23 wurde damals eine Vorversion ohne Hop-Kette verwendet
(Ergebnis: 180° Fehler in allen 24 Richtungen). `t_timebase`, `t_sdram_selftest` und
`t_frame_assembler` testen Code, der mit der Korrektur neu entstand; Gegenprobe für
`t_timebase` siehe Abschnitt 4.

---

## 3. Aufbau

```
test/host/
  Makefile
  shim/        Stubs: arm_math.h (CMSIS-DSP-Teilmenge), cmsis_os2.h (RTOS, einfädig),
               arm_rfft.cpp (arm_rfft_fast_f32 über kiss_fft, CMSIS-Packing, IFFT mit 1/N),
               sds_stubs.cpp (USBDriver/SDS_Data für Output_Interface_130; fängt Nachricht id 4 ab)
  shim_hal/    stm32f7xx_hal.h – SDRAM-Handle und SCB_CleanInvalidateDCache (zählt Aufrufe)
  shim_irq/    stm32f7xx.h – PRIMASK-Sperre als globaler Mutex (für echte Threads im Logger-Test)
  common/      chain.hpp – Simulator → 114 (Hop) → 118 → Frame_Assembler, wie Sensor_Unit_112::nextFrame()
  t_*.cpp      Prüfungen
  m_*.cpp      Messungen
```

Getestet wird der **unveränderte** Projektcode aus `Core/SDS_110` (Module 114, 118, 122,
124/HBD, 126, 128, 130, Signal_Simulator, Frame_Assembler, TimeBase, Logger, SDRAMSelfTest).
Die Signalquelle ist `Harness/Signal_Simulator` im Hardware-Rohformat.

**Was die Host-Tests nicht abdecken:** HAL, DMA, Interrupt-Prioritäten, Cache und MPU,
FreeRTOS-Scheduling, USB-Übertragung, Rechenzeit auf dem Cortex-M7, echte Mikrofone.
Die FFT kommt aus `kiss_fft` statt aus CMSIS-DSP (numerisch gleichwertig, nicht bitgleich).

---

## 4. Prüfungen (`t_*`)

### t_scaling – Befund 6
Speist Rohwerte im SAI/ADAU7118-Format (`pcm24 << 8`) über `Microphone_Array_114::pushBlock()`
ein und prüft den float-Wert im Puffer.
Kriterium: ±Vollaussteuerung, +0,5, −0,25, 1 LSB und 0 exakt (relativ 1e-6).
Referenz: 6/6 OK. Schiebt Blöcke nach, bis ein 114-Puffer fertig ist – funktioniert daher
mit Hop- (ab Befund 17) und Frame-großen Puffern.

### t_frame_assembler – Befund 17
Prüft `Frame_Assembler::push()` mit markierten Hops.
Kriterium: nach Hop 5 nicht voll; nach Hop 6 Frame [5|6] mit Zeitstempel von Hop 5; nach Hop 7
[6|7], `frame_id` fortlaufend; Hop 9 nach Lücke → Neubeginn; Hop 10 → [9|10]; `reset()`.
Seit Befund 44 (28.09.2026) zeigt der Frame auf zwei Hop-Slots (`AnalysisFrame::sample()`); zusätzlich:
Board-Pfad (`beginHop`, `Pre_Processor_118::process(frame, out)`, `commitHop`) und Werkzeug-Pfad
(118 in-place, `push`) liefern über 6 Hops bitgleiche Frames und dieselbe 118-Verstärkung.
`test/host/common/chain.hpp` nutzt jetzt den Board-Pfad.
Referenz: 8/8 OK.

### t_unit_report – Befunde 10, 33
Serialisiert UnitReports (Message id 5, Kopf 25 Byte) mit 0, 3, 8, 51, 52, 64 Bändern über
`Output_Interface_130::send()` (USB abgefangen).
Kriterium: nsel = min(Bänder, 51), Bandindizes und -wahrscheinlichkeiten innerhalb 128 Byte.
Zeitstempel: ohne Abgleich Laufzeit in µs (Quelle 0); nach `UtcClock::fromSync` (USB-Kommando
Typ 7) UTC = Laufzeit + Versatz auf die µs genau (Quelle 1); Zeiten vor 2020 werden verworfen.
Referenz: 6/6 OK, 5 Prüfungen ok. (id 4 bis 27.09.2026: 16 Byte Kopf, max. 56 Bänder, Zeit nur ms.)

### t_sdram_selftest – Befund 14
`sdramSelfTest()` mit HAL-Stub auf einem Speicherfeld von 591 kB + Wächterwörtern.
Kriterium: Handle nicht READY → false ohne Speicherzugriff; intakter Bereich → true, 20
Prüfadressen, 2 Cache-Flushes, Wächter unberührt; leerer Bereich → true.
Nicht abgedeckt: defektes SDRAM, echtes Cache-Verhalten.

### t_timebase – Befund 16
`CycleExtender` (216 000 Zyklen/ms) in vier Szenarien: DMA-Takt 2,67 ms über 10 min;
zufällige Abstände bis 15 s; Pausen 18–60 s; Pausen bis 1 h; Tick bis 1 ms verzögert.
Kriterium: monoton, max. Fehler < 1 µs. Referenz: alle 0,000 µs.
Gegenprobe: In einer Kopie von `TimeBase.hpp` die Korrekturzeile
`if (expected > d + (1ull << 31))` durch `if (false && …)` ersetzen und mit `-I` auf die Kopie
übersetzen – die Pausen-Szenarien schlagen dann fehl (Fehler im Bereich von Stunden).

### t_logger – Befund 19
(1) 300-Zeichen-Meldung → 255 Byte ohne Nullbyte. (2) 4 Schreib-Threads × 20 000 Meldungen und
1 Leser parallel.
Kriterium: 0 defekte, 0 vertauschte Meldungen je Schreiber, empfangen + verworfen = gesendet.
Referenz: gesendet 80 000, empfangen ≈ 30 000, verworfen ≈ 50 000 (gewollt: Schreiber schneller
als Leser), 0 defekt, 0 vertauscht.

### t_bearing_broadband – Befunde 23, 8
Breitbandige Quelle (Wind-Szenario als Punktquelle), alle Bänder selektiert, 24 Richtungen
0…345°; TDOA-LS-Peilung (`estimateBearing`) und SRP-Scan.
Kriterium: 24/24 gültig, max. Fehler < 0,5°. Referenz: max. 0,20° (TDOA-LS), 0,13° (SRP)
(400 mm: 0,12° / 0,10°; vor dem Simulator-Umbau 0,09° / 0,07°).

### t_ml124 – Befunde 20, 36 (AP6 Trainingskonzept ML124)
Modell aus `ML124_Model_Data.hpp`, Referenzvektoren aus `common/ML124_Model_Ref.hpp` (beide von
ML_Test `app/train_ml124/export.py`). Prüft:

1. Die Merkmalsversion des Modells ist gleich der dieses Code-Stands. Das Makefile berechnet sie
   wie `tools/features`; bei Abweichung bricht die Übersetzung mit `static_assert` ab.
2. C++-Inferenz gegen Keras, max. Abweichung ≤ 1e-5.
3. Kontextstapel in `infer()` bitgenau wie `stack_context()` in ML_Test.
4. Die Stufe Schatten liefert bitgenau dasselbe s(t) wie die Stufe HBD; die Stufe ML liefert
   endliche Werte in [0, 1].

Referenz (`k5_h48_d3`): max. |C++ − Keras| = 5,4·10⁻⁷. DroneStatic 10 dB in der Stufe Schatten:
Detektion gleich 100 %, Band-Überlappung 0,12, mittleres |Δp| 0,43 (Bewertung siehe
`doc/Vergleich_HBD_ML124.md`).

### t_hopclock – Befund 28
`HopClock` (Infrastructure/Utils) gibt den Takt der Simulation im ProcessingTask vor. Die
Task-Schleife wird nachgebildet: aufwachen zu `nextTick()`, fällige Hops erzeugen, 0–30 ms rechnen.
Kriterium: nach 60 s genau 1875 Hops (31,25 /s) ohne übersprungene; 100 ms Verspätung → 3 Hops auf
einmal; 1 s Verspätung → 4 nachgeholt, 27 übersprungen und gezählt; Tick-Überlauf und 1024 Hz
(32,768 Ticks je Hop) über 10 min: 18 750 Hops.
Vorher (`osDelay(40)` plus Rechenzeit): höchstens 25 Hops/s.

### t_gcc_direct – Schnellpfad der GCC-PHAT (126)
Bei höchstens `DIRECT_MAX_BINS` (128) selektierten Bins berechnet 126 die Korrelation nur für die
Lags ±`SRP_MAX_LAG` (32 bei 200 mm, vorher 64) direkt aus den Bins statt per IFFT über 4096 Werte.
Geprüft gegen die IFFT (`setDirectMaxBins(0)`):
1. 400 verzögerte Spektrenpaare (τ innerhalb ±90 % des Intra-Unit-Fensters `maxIntraDelay()`,
   bei 200 mm ±27 Samples; 3–24 Bänder, Rauschen): gleiche Gültigkeit,
   |Δτ| ≤ 1e-3 Samples, Peak und Ratio relativ ≤ 1e-3.
2. Volle Kette, DroneStatic 10 dB, 12 Richtungen: Schnellpfad in jedem Frame, Peilung und SRP
   gleich (|Δaz| ≤ 0,01°).
3. SRP-Referenzscan abgeschaltet (`setSrpReference(false)`, am Board USB-Kommando Typ 6): Peilung
   bitgleich, `srpScan()` liefert false; nach dem Einschalten erst mit der nächsten Peilung gültig.
4. Nur jeder 4. Frame (`setSrpEvery(4)`, wie in 120): `srpScan()` gültig in Frame 1, 5, 9, Peilung
   und SRP-Azimut unverändert.

`srpScan()` sucht seit 28.09.2026 grob im 5°-Raster und fein ±4° im 1°-Raster (83 statt 360
Richtungen): 0,010 ms statt 0,029 ms je Aufruf (x86), SRP-Fehler in `t_azimuth` unverändert.

Referenz: |Δτ| 8·10⁻⁶ Samples, Δpeak 5·10⁻⁷, |Δaz| 0,0000° (SRP 0,0007°);
`estimateBearing()` je Frame 0,13 ms (Schnellpfad) gegenüber 1,51 ms (IFFT), x86 `-O2`
(400 mm, ±64 Lags: 0,28 ms).

### t_feedback – Feedback der Tracking-Einheit (FSL9 §10, Id 8)
1. `FeedbackCodec`: ŝ mit 4 Bit je Band, Azimut 0,01°, Distanz 0,1 m, Flags; falsche Länge und
   Azimut ≥ 360° werden verworfen; Bytes vom PC-Monitor (`build_feedback`) gleich und dekodierbar.
2. `Output_Interface_130::pollFeedback`: neu → übernommen; unverändert → nichts; älter als 2 s →
   einmal zurücksetzen (valid = false), danach nichts.
3. 126: Band mit ŝ_b = 0,8 wird schon bei p_b = 0,4 selektiert (θ = 0,3), Gewicht 0,4 · 1,8;
   nach dem Zurücksetzen wieder θ_sel = 0,5.

### t_usb_commands – Kommandos aus dem USB-Bytestrom (Befund 32)
`Infrastructure/Utils/CommandAssembler.hpp`, genutzt von `USBTask`.
1. Ein Kommando je Paket wie bisher.
2. Zusammengefasst: Feedback (52) + Unit-ID (16) auf 64 + 4 Byte; Unit-ID + SRP in einem Paket.
3. Geteilt: Sync (24) auf 10 + 14 Byte.
4. Müll vor dem Magic und ein scheinbares Magic mit falscher Länge melden Fehler, danach Resync;
   ein Magic-Anfang am Paketende bleibt erhalten.
5. Ein Rest, der älter als 20 ms ist, wird verworfen und nicht mit dem nächsten Kommando verbunden.

### t_local_position – Standort der Einheit (FSL9 A7)
`Infrastructure/Utils/LocalPosition.hpp`, genutzt von `USBTask` (Id 10), `LoggerTask` (Id 6 jede Sekunde)
und dem Simulator (FlyBy-Bahn um den Ursprung).
1. Grundwert: Ursprung [0, 0, 0], nicht gesetzt.
2. Id 10: Ost/Nord/Oben in mm mit Vorzeichen; Bytes gleich denen des PC-Monitors (`build_position_message`).
3. Außerhalb ±100 km bzw. −1000 … +10 000 m oder falsche Länge verworfen, alter Wert bleibt; Grenzwerte gültig.
4. Flags 0 setzt auf den Ursprung zurück.
5. Nachricht Id 6: Unit, Flags, Ost, Nord, Oben little-endian.

### t_sds_data – Sperr-Timeouts in SDS_Data (Befund 29)
Der Host-Shim `cmsis_os2.h` lässt die nächsten n `osMutexAcquire()` mit Timeout scheitern (`g_osMutexFailNext`).
1. Getter (Mode, Unit-ID, Simulation, Azimut, Task-Statistik, akustischer Zustand) liefern bei
   Timeout den aktuellen Wert statt 0; jeder Timeout wird gezählt.
2. `tryGet…` liefert false und lässt den Ausgabewert unverändert.
3. Setter: ein Timeout → zweiter Versuch übernimmt den Wert; zwei Timeouts → verworfen,
   gezählt, errorFlag 999.

### t_bearing_f0 / m_bearing_f0 – Peilung bei hohem f0 (Befund 26)
DroneStatic, 12 Richtungen, Frames 40…119. Prüfung (20 dB, f0 = 180, 480, 1000 Hz): höchstens 1 %
grobe Fehler (> 30°), 95-%-Fehler ≤ 10°, mindestens 70 % gültig. Gegenprobe mit dem Stand vor dem
28.09.2026: 480 Hz 17 grobe Fehler, 1000 Hz 117 (95 % 140°) → fällt durch.

| f0 (Hz) | 120 | 250 | 400 | 480 | 560 | 650 | 800 | 1000 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| gültig, vorher | 100 % | 100 % | 85 % | 85 % | 91 % | 96 % | 78 % | 66 % |
| 95 %, vorher | 5,4° | 3,6° | 10,6° | 20,6° | 12,2° | 6,5° | 34,3° | 137,5° |
| gültig, jetzt | 99 % | 100 % | 94 % | 89 % | 87 % | 95 % | 82 % | 78 % |
| 95 %, jetzt | 5,4° | 3,6° | 3,4° | 6,4° | 8,5° | 6,8° | 3,8° | 3,3° |
| grob > 30°, jetzt | 0 | 0 | 0 | 1 | 2 | 0 | 1 | 0 |

### t_hop_gap – Lücken im Hop-Strom (Befunde 27, 42)
`Microphone_Array_114::pushBlock()` mit vollen Puffern (Leser holt nichts ab):
1. ohne Verwerfen ist die `frame_id` fortlaufend;
2. nach verworfenen Blöcken ist eine Nummer übersprungen (Referenz: gehalten 4 5 6, danach 8 9);
3. Frame_Assembler setzt keinen Frame aus Hops vor und nach der Lücke zusammen;
4. Kanalabstand in `MicFrame` 6176 Byte (mod 1024 = 32).
Gegenprobe mit dem Stand vor dem 28.09.2026: 2–4 fallen durch.

### t_azimuth – Azimut-Konvention (FSL9 A28)
`Infrastructure/Utils/Azimuth.hpp`: 0° = Nord, im Uhrzeigersinn; Mikrofon 0 zeigt nach Nord.
1. Umrechnung Array-System ↔ Azimut umkehrbar, `wrap360`, `diff` (359°/1° → 2°).
2. Geometrie: Nord = Mikrofon 0, Ost = 6, Süd = 4, West = 2 (Nummerierung gegen den Uhrzeigersinn).
3. Kette (DroneStatic 30 dB), Quelle bei 0, 45, …, 315°: TDOA-LS und SRP liefern den Azimut der
   Quelle. Referenz: Median-Fehler 0,36…0,76° (SRP 0,19…0,34°).
4. Nordabgleich (USB Id 9): Offset aus 0,01° (15,00°, −180,00°); außerhalb ±180° verworfen, der
   alte Wert bleibt; 128 addiert den Offset auf die Peilung (350° + 15° → 5°, 10° − 20° → 350°).

### t_sim_flyby – Simulator-Auswahl und Überflug
`Harness/SimScenario.hpp`, `Signal_Simulator` (FlyBy).
1. USB Id 3: 0 = Mikrofone, 1 = `SIM_SCENARIO_ID`, 2 + k = Szenario k (7 = FlyBy), ab 8 ungültig.
2. Geometrie (15 m/s, kürzester Abstand 30 m, Ost-Kurs): Mitte 30 m bei 0°, Anfang 48,0 m bei 308,7°,
   Ende 51,3°; 5 … 10 s Pause; im 2. Durchgang Kurs um 45° gedreht. Die Bahn liegt um den
   Ursprung: Einheit bei [0, −50, 0] -> Mitte 80 m bei 0°, Anfang 88,4 m bei 334,9°; 40 m
   Höhenunterschied gehen in die Distanz ein.
3. Ablauf im Simulator über 20 s: Quelle zu 50 % aktiv, 3 Wechsel.
4. Kette: Peilung im Flug Referenz Median 1,15°, 95 % 3,25° (Grenze 3° / 10°); HBD ab dem
   2. Durchgang im Flug 122/122, in der zweiten Pausenhälfte 0/78. Beim Kaltstart erkennt HBD die
   Drohne erst nach ~3 s (Rauschboden), daher zählt der 1. Durchgang für HBD nicht.

### t_sound_speed – Schallgeschwindigkeit (FSL9 A23)
Die Lufttemperatur kommt im Sync-Kommando (USB Typ 7, `doc/ICD_SDS_PC_Monitor.md`); daraus
c = 331,3 m/s · √(1 + T/273,15 °C) (`SoundSpeed.hpp`), gesetzt in 126, 128 und im Simulator.
1. Formel: 0 °C 331,3 m/s, 20 °C 343,2, −40 °C 306,1, +60 °C 365,9.
2. Wire-Wert 0,01 °C, 0x8000 = unbekannt, außerhalb −40…+60 °C verworfen.
3. Volle Kette (DroneStatic 30 dB, 6 Richtungen) bei −40…+60 °C, Luft und 126 mit derselben c:
   Peilung gültig, Median ≤ 1° (TDOA-LS und SRP), Schnellpfad in jedem Frame. Das Lag-Fenster
   wächst mit der Kälte: ±29 (+60 °C) … ±35 (−40 °C), mit SRP-Referenz mindestens ±32.
4. Ohne Korrektur (126 mit 343 m/s) bei −40 °C: im Mittel 26,4 statt 28,0 gültige Paare, weil
   Verzögerungen bis 31,4 Samples am Rand des Fensters ±30 liegen. Der Azimut selbst ändert sich
   kaum (alle Paarverzögerungen skalieren gleich).

Referenz: Median-Fehler 0,55…0,62° (SRP 0,47…0,66°), Residuum 0,82…0,86 Samples, alle Frames
gültig und im Schnellpfad.

---

## 5. Messungen (`m_*`) und Referenzwerte

Frame-Angaben in den Messprogrammen: Seit Befund 17 ist ein Analyse-Frame 32 ms; die Argumente
von `m_overview` und `m_selection` werden in „alten“ 64-ms-Frames angegeben und intern verdoppelt.
Der HBD entscheidet nach dem Start 3 s lang nicht (Befund 24) – Messungen, die früher beginnen,
zeigen deshalb weniger als 100 % Detektion.

`m_overview`, `m_selection`, `m_bearing_drone` und `m_confidence` rechnen in der Stufe aus
`ML124_Config.hpp` (Standard Schatten, s(t) wie HBD). Die Umgebungsvariable `ML124_MODE`
(0 HBD, 1 Schatten, 2 ML) wählt eine andere; die erste Ausgabezeile nennt die Stufe. Die folgenden
Referenzen gelten für HBD bzw. Schatten; für ML siehe `doc/Vergleich_HBD_ML124.md`.

### m_overview – Übersicht (Aufruf: `m_overview [Frames, Standard 120]`)
Je Szenario (SNR 20 dB): HBD-DRONE-Anteil, detected (≥ 3 Bänder > θ_sel), Bänder, f0, Score,
mittlerer Floor, gültige Peilungen, Median-Fehler; danach DroneStatic 30…−6 dB.
Referenz (Standardaufruf, Messung ab 1,9 s – enthält die HBD-Anlaufzeit):

| Szenario | HBD DRONE | detected | Peilung | Fehler |
|---|---|---|---|---|
| DroneSweep / DroneStatic | 81 % | 81 % | 100 % | 1,3° / 1,0° (0,9° / 0,6°) |
| SingleTone | 0 % | 0 % | 26 % | – |
| WindNoise | 0 % | 0 % | 85 % | – |
| Silence | 0 % | 0 % | 2 % | zufällig |

Empfindlichkeit DroneStatic (HBD DRONE): 30 dB 81 %, 10 dB 78 %, 6 dB 56 %, 3 dB 42 %,
0 dB 14 %, −3 dB 3 %, −6 dB 1 %. Für eingeschwungene Werte `m_overview 300` verwenden.

### m_hbd_diag – HBD-Rauschboden (Aufruf: `m_hbd_diag [Szenario 0..4, Standard 4]`)
Quantile von (magDb − noiseFloorDb) im Bereich 100–4000 Hz und SNR je Harmonischer.
Referenz: Silence Median +1,4 dB (5 %/95 %: −9,6 / +7,8 dB), kein DRONE;
DroneStatic Median +2,1 dB, 95 % +36,5 dB (Harmonische), SNR je Harmonischer 16–47 dB, DRONE.
Vor Befund 7 lag der Median bei rund +70 dB (Floor an −60 dB festgeklemmt).

### m_bearing_drone – Peilung Drohne (Aufruf: `m_bearing_drone [1 = Static, 0 = Sweep]`)
Volle Kette 118 → 122 → 124 → 126, 12 Richtungen, Messung ab 1,3 s.
Referenz DroneStatic:

| SNR | gültig | Paare | Ratio-Median | Fehler Median | Fehler 95 % |
|---|---|---|---|---|---|
| 30 dB | 100 % | 28,0 | 18,4 | 0,37° (0,19°) | 1,53° (0,73°) |
| 20 dB | 100 % | 27,9 | 17,7 | 0,92° (0,44°) | 2,76° (1,31°) |
| 10 dB | 100 % | 27,3 | 17,9 | 1,85° (0,91°) | 6,00° (3,06°) |
| 6 dB | 100 % | 26,9 | 18,7 | 2,46° (1,16°) | 8,26° (3,89°) |
| 3 dB | 99 % | 26,6 | 18,9 | 3,41° (1,62°) | 10,22° (5,45°) |
| 0 dB | 98 % | 24,8 | 16,2 | 4,36° (2,07°) | 16,15° (6,66°) |

DroneSweep (1° je Hop): Median 0,7–2,2°, 95 % 1,7–7,4° (der Sweep dreht innerhalb eines Frames).
Vor Befund 8 waren bei Drohnensignal 0 % der Peilungen gültig (Ratio-Median ≈ 1,17).

### m_selection – Band-Selektion (Aufruf: `m_selection [Frames] [noise]`)
6 Richtungen, Messung ab 3,2 s. „Report“ = Peilung gültig und detected (so sendet 120).
Referenz:

| Szenario | detected | HBD | Bänder | davon Harmonische | Report |
|---|---|---|---|---|---|
| Drohne 20 dB | 100 % | 100 % | 8,0 | 87 % | 100 % |
| Drohne 10 dB | 100 % | 95 % | 8,0 | 87 % | 100 % |
| Drohne 3 dB | 100 % | 52 % | 5,0 | 94 % | 100 % |
| Drohne 0 dB | 64 % | 14 % | 3,0 | 99 % | 63 % |
| Einzelton | 0 % | 0 % | 0 | – | 0 % |
| Wind | 0 % | 0 % | 0 | – | 0 % |
| Stille | 0 % | 0 % | 0 | – | 0 % |

`m_selection 650 noise`: Langlauf nur mit Rauschszenarien (Fehlalarmrate), Referenz 0 Reports.
Vor Befund 24: Wind 22 % Reports, Stille detected 68 %, Harmonischen-Anteil ~42 %.

### m_distance – Distanz (Aufruf: `m_distance`)
Simulator-Pegel ∝ 1/r, 10…200 m; levelA durch `frameCenterGain()` geteilt (wie 120), r = K/A.
Kriterium (Auswertung): Verhältnis r_geschätzt/r_wahr über alle Distanzen konstant.
Referenz: 0,518 / 0,525 / 0,522 / 0,520 / 0,524. Vor Befund 9: 0,073 … 0,024 (AGC regelte).
Der absolute Wert hängt an `LEVEL_DIST_K_REF` (unkalibriert).

### m_confidence – Konfidenz (Aufruf: `m_confidence`)
Verteilung von `candidateConfidence()` und Residuum (Samples), 6 Richtungen, ab 3,2 s.
Referenz (Median): Drohne 30/20/10/3/0/−3 dB 0,95/0,86/0,69/0,56/0,48/0,27 (Residuum
0,86…5,4 Samples, Peilfehler 0,56…5,5°); Einzelton 0,13 (8,2); Wind 0,99 (0,34); Stille ohne
gültige Peilung (400 mm: 0,01, Residuum 32 Samples).
Die Konfidenz bewertet die Peilung, nicht die Drohnen-Detektion.

### m_longrun – Langzeitverhalten (Aufruf: `m_longrun [maxRise, −1 = Projektwert] [SNR]`)
DRONE-Anteil je 10 s über ~5 min bei stehender Drohne (20 dB).
Referenz (Projektwert 0,156 dB/s): 70 % (Anlaufzeit) · 100 % bis ~2 min 20 s · dann Abfall auf
~78 % (der stehende Ton wird allmählich als Rauschen gelernt).
Vergleich (damals, 64-ms-Frames): 0,05 dB/Frame → Abfall nach ~30 s; 0 → kein Abfall.

---

## 6. Einmalige Untersuchungen (nicht als Test übernommen)

Diese Programme dienten einer Entscheidung und sind nicht Teil von `test/host`;
die Ergebnisse stehen hier zur Nachvollziehbarkeit.

- **Varianten der Band-Wahrscheinlichkeiten in 124 (Befund 24)** – Kopie von
  `Machine_Learning_Module_124.cpp` mit Umschalter, SNR 20 dB, Reports:

  | Variante | Drohne 0 dB | Einzelton | Wind | Stille |
  |---|---|---|---|---|
  | V0 Ist (Gate am Score) | 67 % | 3 % | 22 % | 0 % |
  | V1 Gate = HBD-Entscheidung | 15 % | 0 % | 0 % | 0 % |
  | V2 nur Harmonische | 76 % | 2 % | 5 % | 4 % |
  | V3 nur Harmonische + Konsistenz-Gate | 48 % | 1 % | 3 % | 3 % |
  | V5 nur Harmonische + HBD-Haltezeit 16 (umgesetzt) | 69 % | 2 % | 4 % | 3 % |
  | V6 wie V5, Haltezeit 32 | 75 % | 2 % | 5 % | 4 % |

  Die Restfehlalarme von V5 stammten aus der HBD-Anlaufphase → HBD-Anlaufzeit 3 s ergänzt,
  danach 0 % (siehe `m_selection`).
- **HBD-Anlaufverhalten (Befund 24)** – letzter HBD-Treffer bei Wind/Stille nach dem Start
  bei Frame 22–29 (64-ms-Frames), beim Einzelton bis Frame 39 → Anlaufzeit 3 s.
- **GCC-PHAT-Kohärenz** (Peak / theoretisches Maximum, zu Befund 24/8) – Median Drohne 0,60,
  Stille 0,42 (95 %: 0,67): trennt nicht sauber, daher nicht als Kriterium verwendet.
- **Floor-Anstiegsrate (Befund 7)** – 0,05 / 0,01 / 0 dB je 64-ms-Frame; Grundlage für
  `HBD_FLOOR_RISE_DB_S`.

---

## 7. Neuen Test hinzufügen

1. Datei `test/host/t_name.cpp` (Prüfung, Exit-Code) oder `m_name.cpp` (Messung) mit
   Kopfkommentar (Bezug, was geprüft wird, Aufruf).
2. Für die Signalkette `#include "chain.hpp"` und `nextAnalysisFrame(sim, arr, pre)` benutzen;
   vor jedem Szenario `sim.init(...)`, `g_fa.reset()` und `init()` der Module aufrufen.
3. Im Makefile den Namen in `CHECKS` bzw. `MEASURES` eintragen; braucht der Test nur einzelne
   Module, eine eigene Regel wie bei `t_scaling` anlegen.
4. Referenzergebnis in diesem Dokument ergänzen.
