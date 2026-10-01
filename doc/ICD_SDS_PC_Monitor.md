# ICD SDS_110 ⇄ PC-Monitor

Schnittstellenbeschreibung (Interface Control Document) zwischen der Sensoreinheit SDS_110
(STM32F746, Firmware in diesem Repository) und dem PC-Monitor.

- **Stand:** 28.09.2026. Gilt für die Firmware ab dem Commit, der diese Datei einführt.
- **Maßgeblich im Code:**
  - `Core/SDS_110/Infrastructure/Model/SDS_Structs.hpp` (Strukturen)
  - `Infrastructure/Tasks/USBTask.cpp` (Kommandos)
  - `Processing_Module_120/Output_Interface_130/Output_Interface_130.cpp` (Reports)
  - `Infrastructure/Driver/USBDriver.cpp` (Rahmen, CRC)
- **Bei Widerspruch gilt der Code.** Diese Datei ist dann nachzuführen.

## 1. Übersicht

| Richtung | Id | Name | Länge (Byte) | Zweck |
| --- | --- | --- | --- | --- |
| PC → SDS | 1 | Time Sync (alt) | 16 | veraltet, ersetzt durch Id 7 |
| PC → SDS | 2 | Mode | 16 | Betriebsart DETECT / CALIBRATE / READ |
| PC → SDS | 3 | Simulation | 16 | Signalquelle Mikrofone oder Simulator mit Szenario |
| PC → SDS | 5 | Unit-ID | 16 | Kennung der Sensoreinheit setzen |
| PC → SDS | 6 | SRP-Referenz | 16 | Vergleichsscan SRP-PHAT ein/aus |
| PC → SDS | 7 | Sync | 24 | UTC-Zeit und Lufttemperatur |
| PC → SDS | 8 | Feedback | 52 | Referenzzustand ŝ und Vorhersage der Tracking-Einheit (FSL9 §10) |
| PC → SDS | 9 | Nordabgleich | 16 | Azimut-Offset der Einheit |
| PC → SDS | 10 | Standort | 28 | Position der Einheit lokal: Ost, Nord, Oben in mm |
| SDS → PC | 1 | Detect | 32 | Peilung (Legacy-Format) |
| SDS → PC | 2 | Read | 532 | Rohdatenblock im Modus READ |
| SDS → PC | 5 | UnitReport | 144 | Peilung, Zeit in µs, selektierte Bänder |
| SDS → PC | 6 | Standort | 144 | lokale Position, die das Board verwendet |
| SDS → PC | 99 | Logger | 144 | Textmeldungen der Firmware |

- **Frei:** Id 4 (PC → SDS). Im Pfad SDS → PC ist Id 4 der UnitReport bis 27.09.2026 und wird nicht mehr gesendet.

## 2. Transport

- **Physik:** USB Full Speed, Klasse CDC-ACM (virtueller COM-Port). Die Baudrate spielt keine Rolle.
- **STM32F746ZGT6-Board:** USART1 über CP2102N (USB-C #2), virtueller COM-Port des CP210x-Treibers, **921600 Baud, 8N1, ohne Flusssteuerung** (`SDS110_UART_BAUD`). READ-Streaming ist darüber nicht möglich (Bandbreite).
- **STM32F746ZGT6-Board über WLAN** (`SDS110_PC_UART` 3, seit 01.10.2026): USART3 → ESP32-C3 → TCP. Der PC verbindet sich mit **TCP-Port 3333** der Brücke (Access Point: 192.168.4.1; im vorhandenen WLAN meldet die Brücke ihre Adresse alle 2 s per UDP-Broadcast auf Port 3334: `SDS110-BRIDGE <ip> <port> <mac>`). Die Bytes laufen unverändert durch, Rahmen und CRC wie unten. Mit pyserial: `serial.serial_for_url("socket://192.168.4.1:3333")`. Ein PC zur Zeit; ohne verbundenen PC verwirft die Brücke die Nachrichten. READ-Streaming ist nicht möglich. Die UTC aus Id 7 ist über WLAN nur auf einige ms genau.
- **Kommandos PC → SDS:**
  - **Bytestrom:** Die Firmware setzt die USB-Pakete zu einem Bytestrom zusammen und löst die Kommandos über Magic und Länge heraus (`CommandAssembler.hpp`, seit 28.09.2026). Ein Paket darf mehrere Kommandos oder den Teil eines Kommandos enthalten; der Host fasst dicht folgende Schreibvorgänge oft zusammen (Befund 32).
  - **Rest eines Kommandos:** Folgt der Rest nicht innerhalb von 20 ms, wird der Anfang verworfen.
  - **Höchstens 56 Byte je Kommando.**
- **Keine Antwort:** Die Firmware bestätigt Kommandos nicht. Die Wirkung ist am LCD sichtbar und an den folgenden Reports erkennbar, zum Beispiel Zeitquelle und Unit-ID.
- **Fehlerhafte Kommandos:** Gemeint sind ein falsches Magic, eine unbekannte Id, eine falsche Länge oder ungültige Werte. Die Firmware setzt dann das Fehler-Flag und zeigt die ersten 16 Byte etwa 10 s auf dem LCD an.
- **Senden SDS → PC:** Die Firmware sendet nur bei verbundenem Host. Ist der Sendepuffer voll, werden Nachrichten verworfen und gezählt.

## 3. Konventionen

| | PC → SDS (Kommandos) | SDS → PC (Nachrichten) |
| --- | --- | --- |
| Magic | Bytes `DE AD BE EF` | `uint32` 0xDEADBEEF little-endian, also Bytes `EF BE AD DE` |
| Id und Länge | Byte 4 = Id, Bytes 5–7 = **Gesamtlänge** big-endian | `len_id` (uint32 LE): Bits 0–23 = Gesamtlänge, Bits 24–31 = Id |
| Werte | big-endian (höchstwertiges Byte zuerst) | little-endian, IEEE-754 `float` |
| CRC32 | letzte 4 Byte; **wird noch nicht geprüft** (Befund 12), PC soll trotzdem die CRC wie rechts senden, big-endian | letzte 4 Byte, LE; über alle Bytes davor |

- **CRC32** ist das reflektierte CRC32 mit Polynom 0xEDB88320, Start 0xFFFFFFFF und End-XOR 0xFFFFFFFF. Das entspricht Python `zlib.crc32(daten)`.
- **Zeitstempel `timestamp` (uint32) in den Nachrichten SDS → PC** sind Millisekunden.
  - Bis zum ersten Sync ist es die Laufzeit seit dem Start, danach UTC.
  - Der Wert läuft modulo 2³² über, das sind etwa 49,7 Tage.
  - Die genaue Zeit steht nur im UnitReport (µs, 64 bit).

## 4. Kommandos PC → SDS

### 4.1 Gemeinsamer Aufbau (16 Byte, Id 1–6 und 9)

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | 4 × u8 | `DE AD BE EF` |
| 4 | id | u8 | Kommando-Id |
| 5–7 | length | u24 BE | `00 00 10` (16) |
| 8–11 | value | u32 BE | siehe Tabelle |
| 12–15 | crc | u32 BE | CRC32 über Byte 0–11 (noch nicht geprüft) |

| Id | Name | value | Standard nach dem Start | Wirkung |
| --- | --- | --- | --- | --- |
| 1 | Time Sync (alt) | u32, bisher Unix-Sekunden | – | wird gespeichert, **nicht angewendet**; für UTC Id 7 verwenden |
| 2 | Mode | 1 = DETECT, 2 = CALIBRATE, 3 = READ | 1 | Betriebsart; setzt die Task-Zähler zurück. CALIBRATE verarbeitet wie DETECT (Detect und UnitReport) und zeigt am LCD den Nordabgleich |
| 3 | Simulation | 0 = Mikrofone (SAI/DMA), 1 = Simulator (Standardszenario `SIM_SCENARIO_ID`), 2 DroneSweep, 3 DroneStatic, 4 SingleTone, 5 WindNoise, 6 Silence, 7 FlyBy | 1 | Signalquelle und Szenario (`Harness/SimScenario.hpp`); ein anderes Szenario beginnt neu. Andere Werte setzen das Fehler-Flag, der alte Wert bleibt. Setzt die Task-Zähler zurück |
| 5 | Unit-ID | u32, genutzt werden die unteren 16 Bit | aus der STM32-UID | Kennung in Detect (`mic`) und UnitReport (`unit`) |
| 6 | SRP-Referenz | 0 = aus, sonst ein | 0 | Vergleichsscan SRP-PHAT, nur in jedem 4. Frame, Raster grob 5°/fein 1°; die Peilung hängt nicht davon ab |
| 9 | Nordabgleich | i32 BE (Zweierkomplement) in 0,01°, −18000 … 18000 | 0 | Offset auf die Peilung, siehe 4.4 |

In der Makrodatei `PC_Monitor_Test.ptp` sind CALIBRATE und READ vertauscht beschriftet: CALIBRATE sendet 3, READ sendet 2 (Befund 25). Verbindlich ist die Tabelle oben.

### 4.2 Sync (Id 7, 24 Byte)

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | 4 × u8 | `DE AD BE EF` |
| 4 | id | u8 | `07` |
| 5–7 | length | u24 BE | `00 00 18` (24) |
| 8–15 | utc_us | u64 BE | UTC in µs seit 01.01.1970 00:00:00; **0 = keine Zeit** (nur Temperatur übernehmen) |
| 16–17 | temp | i16 BE | Lufttemperatur in 0,01 °C; **0x8000 = unbekannt** |
| 18–19 | reserved | 2 × u8 | `00 00` |
| 20–23 | crc | u32 BE | CRC32 über Byte 0–19 (noch nicht geprüft) |

**Verarbeitung in der Firmware**
- **Empfangszeit:** Die Firmware merkt sich die Empfangszeit bereits im USB-Interrupt.
- **UTC:**
  - Aus `utc_us` und der Empfangszeit berechnet sie Versatz = UTC − Laufzeit.
  - Alle folgenden UnitReports tragen dann UTC mit Zeitquelle 1.
  - Eine Zeit vor dem 01.01.2020 gilt als Fehler: Die Zeit wird nicht übernommen, die Temperatur trotzdem.
- **Temperatur:**
  - Gültig ist der Bereich −40,00 … +60,00 °C.
  - Daraus berechnet die Firmware die Schallgeschwindigkeit c = 331,3 m/s · √(1 + T / 273,15 °C) für trockene Luft (`Infrastructure/Utils/SoundSpeed.hpp`).
  - c wirkt ab dem nächsten Frame auf die Paarverzögerungen, das Lag-Fenster und die Peilung (126), auf die Lokalisation (128) und im Simulationsbetrieb auf den Simulator.
  - „Unbekannt“ lässt die letzte Temperatur stehen. Ein Wert außerhalb des Bereichs gilt als Fehler und lässt die letzte Temperatur ebenfalls stehen.
  - Ohne jede gültige Temperatur gilt c = 343 m/s (≈ 20 °C).
- **Anzeige:** Das LCD zeigt in Zeile y = 230 `T <°C>C c <m/s> UTC|Lauf`.

**Senden:** Nach dem Verbinden und dann etwa jede Minute, weil der Quarz des Boards driftet. Bei Temperaturänderungen auch öfter.

**Genauigkeit:**
- **UTC über USB:** etwa 1 ms, weil die USB-Laufzeit nicht herausgerechnet wird. Das reicht für den UTC-Bezug der Reports, nicht aber für die Synchronisation mehrerer Einheiten (FSL9: ≤ 10 µs). Diese ist mit GNSS-PPS in HW-Version 2 vorgesehen (Zeitquelle 2).
- **Temperatur:** 1 K Fehler ergibt etwa 0,18 % Fehler in c. Bei einer Einheit wirkt sich c kaum auf den Azimut aus, weil alle Paarverzögerungen gleich skalieren. Wichtig ist c für das Lag-Fenster: Bei −40 °C ohne Korrektur fallen Paare weg (Host-Test `t_sound_speed`). Bei mehreren Einheiten geht c direkt in die Entfernungsdifferenzen ein.

**Beispiel:** 28.09.2026 12:00:00 UTC, 21,50 °C:

```
DE AD BE EF 07 00 00 18 00 06 5C 89 CE 33 30 00 08 66 00 00 01 B1 FF D4
```

### 4.3 Feedback (Id 8, 52 Byte)

Die Tracking-Einheit (PC-Monitor `app/tracking/`) sendet das Feedback nach jeder Übernahme eines Reports in eine **bestätigte** Spur, also bis zu 31-mal pro Sekunde (FSL9 §10, Anspruch 8). Endet eine Spur, sendet sie ein Feedback mit Flags = 0 (zurücksetzen).

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | 4 × u8 | `DE AD BE EF` |
| 4 | id | u8 | `08` |
| 5–7 | length | u24 BE | `00 00 34` (52) |
| 8–39 | ref_state | 32 × u8 | ŝ je Band mit 4 Bit, q = round(ŝ_b · 15): Band 2i im oberen, 2i+1 im unteren Halbbyte von Byte 8+i |
| 40–41 | pred_az | u16 BE | vorhergesagter Azimut in 0,01° (0 … 35 999; 0° = Nord, im Uhrzeigersinn) |
| 42–43 | pred_r | u16 BE | vorhergesagte Distanz in 0,1 m |
| 44 | flags | u8 | Bit 0 = ŝ gültig (0 = Feedback zurücksetzen), Bit 1 = Vorhersage gültig |
| 45–47 | reserved | 3 × u8 | `00 00 00` |
| 48–51 | crc | u32 BE | CRC32 über Byte 0–47 |

**Verarbeitung in der Firmware** (`FeedbackCodec.hpp`, `Output_Interface_130::pollFeedback`, 126 `applyFeedback`):
- **Schwelle und Gewicht:** Bänder mit ŝ_b > 0,6 bekommen die Selektionsschwelle 0,3 statt 0,5, ihr Gewicht wird mit (1 + ŝ_b) multipliziert.
- **Zurücksetzen:** Kommt 2 s lang kein neues Feedback, werden Schwellen und Gewichte zurückgesetzt. Ein Feedback mit Flags = 0 wirkt sofort.
- **Vorhersage:** Die Firmware nimmt sie an, nutzt sie aber noch nicht. Das Suchfenster ±2 ms um die vorhergesagte Verzögerung (FSL9 §10, A34) ist offen.
- **Ungültige Werte:** Eine falsche Länge oder ein Azimut ≥ 360° setzt das Fehler-Flag.

Die Kodierung mit 4 Bit hält das Kommando unter 56 Byte, sodass es in ein USB-Paket passt (Abschnitt 2). Für die Schwelle 0,6 und den Faktor (1 + ŝ_b) genügt diese Auflösung (1/15).

### 4.4 Nordabgleich (Id 9, 16 Byte)

Die Einheit wird selten genau mit Mikrofon 0 nach Nord aufgestellt. Der Nordabgleich gleicht das aus (PC-Monitor, Tab Calibrate):

1. **Messen:** Eine Referenzquelle mit bekanntem Azimut φ_ref betreiben, zum Beispiel einen Lautsprecher oder eine schwebende Drohne. Der PC-Monitor mittelt die Peilungen φ_i der UnitReports zirkular: φ̄ = atan2(Σ sin φ_i, Σ cos φ_i).
2. **Offset:** Der neue Offset ist o_neu = o_alt + (φ_ref − φ̄), auf −180° … +180° gebracht. Die Peilungen φ_i enthalten schon o_alt.
3. **Senden:** Der PC-Monitor sendet Id 9 mit round(o_neu · 100). Er speichert den Wert und sendet ihn bei jedem Verbinden erneut, denn die Firmware speichert ihn nicht über einen Neustart hinaus.

**Verarbeitung in der Firmware** (`USBTask::handleAzimuthOffset`, `Azimuth::offsetFromCenti`, 128 `setCalibration`):
- 128 addiert den Offset auf jede Peilung: φ = wrap360(φ_roh + o). Das gilt für Detect (`azi`) und UnitReport (`bearing_deg`).
- Die Skalierung bleibt 1.
- Werte außerhalb ±180,00° oder eine falsche Länge setzen das Fehler-Flag. Der alte Offset bleibt dann gültig.
- Die Vorhersage aus Id 8 ist schon abgeglichen, weil sie aus abgeglichenen Peilungen stammt.

### 4.5 Standort (Id 10, 28 Byte)

Position der Einheit in lokalen Koordinaten: x = Ost, y = Nord, z = Oben, in m relativ zum lokalen Ursprung [0, 0, 0]. Die Achsen sind die des Lageplans im PC-Monitor und des Azimuts (0° = Nord, 90° = Ost). Nach dem Start steht die Einheit im Ursprung. Der PC-Monitor sendet die Position nach einer Eingabe und bei jedem Verbinden (gespeicherter Wert). Bis 29.09.2026 war das Kommando WGS84 (Breite, Länge, Höhe) mit GNSS-Vorrang.

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | 4 × u8 | `DE AD BE EF` |
| 4 | id | u8 | `0A` |
| 5–7 | length | u24 BE | `00 00 1C` (28) |
| 8–11 | east | i32 BE | Ost in mm, −100 km … +100 km |
| 12–15 | north | i32 BE | Nord in mm, −100 km … +100 km |
| 16–19 | up | i32 BE | Oben in mm, −1000 m … +10 000 m |
| 20 | flags | u8 | Bit 0 = Position setzen; 0 setzt die Einheit zurück auf den Ursprung [0, 0, 0] |
| 21–23 | reserved | 3 × u8 | `00 00 00` |
| 24–27 | crc | u32 BE | CRC32 über Byte 0–23 (noch nicht geprüft) |

Beispiel: Ost 123,456 m, Nord −78,9 m, Oben 5,5 m → `DE AD BE EF 0A 00 00 1C 00 01 E2 40 FF FE CB CC 00 00 15 7C 01 00 00 00` + CRC `24 CF 03 6D`.

**Verarbeitung in der Firmware** (`LocalPosition.hpp`, `USBTask::handlePosition`):
- **Auflösung:** 1 mm, gespeichert als Ganzzahl.
- **Ungültige Werte:** Werte außerhalb der Grenzen oder eine falsche Länge setzen das Fehler-Flag. Die alte Position bleibt.
- **Antwort:** Die Firmware antwortet sofort mit Id 6 (5.5). So sieht der PC, ob die Position angekommen ist.
- **Speicherung:** Nur im RAM. Nach einem Neustart steht die Einheit wieder im Ursprung, bis der PC die Position sendet.
- **Simulator:** Das Szenario FlyBy legt seine Bahn um den Ursprung (kürzester Abstand 30 m). Azimut und Distanz rechnet der Simulator von der Position der Einheit aus. Steht die Einheit z. B. bei [0, −50, 0], fliegt die Drohne in 80 m Abstand vorbei.
- **Anzeige:** Das LCD zeigt die Position rechts unten (`ONH` Ost, Nord, Höhe in m; grau = Grundwert).

## 5. Nachrichten SDS → PC

### 5.1 Detect (Id 1, 32 Byte)

Wird je Frame gesendet, wenn eine gültige Peilung vorliegt und eine Drohne detektiert ist, also höchstens 31,25-mal pro Sekunde.

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | u32 LE | 0xDEADBEEF |
| 4–7 | len_id | u32 LE | 0x01000020 |
| 8–11 | timestamp | u32 LE | ms (Abschnitt 3) |
| 12–15 | mic | u32 LE | Unit-ID |
| 16–19 | azi | f32 | Azimut in Grad, 0 … < 360: 0° = Nord, im Uhrzeigersinn (90° = Ost); Mikrofon 0 zeigt nach Nord (FSL9 FIG. 5, `Azimuth.hpp`) |
| 20–23 | distance | f32 | Distanz in m aus dem Pegelmodell (nur eine Einheit, unkalibriert) |
| 24–27 | conf | f32 | Konfidenz 0…1 |
| 28–31 | crc32 | u32 LE | CRC32 über Byte 0–27 |

### 5.2 UnitReport (Id 5, 144 Byte)

Wird zusammen mit Detect gesendet. Rahmen:

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | u32 LE | 0xDEADBEEF |
| 4–7 | len_id | u32 LE | 0x05000090 |
| 8–11 | timestamp | u32 LE | ms (Abschnitt 3) |
| 12–139 | data | 128 × u8 | Nutzlast, siehe unten |
| 140–143 | crc32 | u32 LE | CRC32 über Byte 0–139 |

Nutzlast (Offsets relativ zu `data`, little-endian):

| Offset | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0 | unit | u16 | Unit-ID |
| 2 | time_us | u64 | Beginn des Analyse-Frames in µs: UTC, wenn Quelle ≠ 0, sonst Laufzeit seit dem Start |
| 10 | src | u8 | Zeitquelle: 0 = Laufzeit, 1 = UTC vom PC (Id 7), 2 = GNSS-PPS (HW-Version 2) |
| 11 | bearing | f32 | Azimut in Grad (wie Detect) |
| 15 | residual | f32 | LS-Residuum der Peilung in s |
| 19 | pairs | u8 | gültige Mikrofonpaare (0…28) |
| 20 | nsel | u8 | Anzahl der übertragenen Bänder n (0…51) |
| 21 | level | f32 | Pegelmaß der selektierten Bänder (Distanzmodell, ohne Einheit) |
| 25 … 25+n−1 | band_index | n × u8 | Bandnummern 0…63 (Band b beginnt bei 80 Hz + b · 62,5 Hz) |
| 25+n … 25+2n−1 | band_prob | n × u8 | p_b · 255, abgeschnitten |

Die restlichen Bytes sind 0. Sind mehr als 51 Bänder selektiert, werden nur die ersten 51 übertragen, und `nsel` nennt die tatsächliche Anzahl.

### 5.3 Read (Id 2, 532 Byte)

Wird nur im Modus READ gesendet, mit **Rohdaten** der Mikrofone: 114 ohne die Vorverarbeitung 118 (seit 28.09.2026, Befund 35). Je Hop (32 ms) gehen 8 Mikrofone × 12 Blöcke = 96 Nachrichten raus, das sind etwa 1,6 MB/s. Das übersteigt USB Full Speed; ein Hop, der nicht vollständig in den Sendepuffer passt, wird ab dort verworfen. Die Hop-Nummer zeigt dem PC jede Lücke.

| Byte | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–3 | magic | u32 LE | 0xDEADBEEF |
| 4–7 | len_id | u32 LE | 0x02000214 |
| 8–11 | timestamp | u32 LE | ms, Beginn des Hops |
| 12 | micNr | u8 | Mikrofon 0…7 |
| 13 | blockNr | u8 | Block 0…11 innerhalb des Hops |
| 14–15 | hopNr | u16 LE | Hop-Nummer (fortlaufend, mod 65 536) |
| 16–527 | data | 128 × i32 LE | Rohsamples, 24 Bit (±2²³) |
| 528–531 | crc32 | u32 LE | CRC32 über Byte 0–527 |

- **Rohdaten:** Kein Bandpass, keine Rauschunterdrückung, keine AGC. So lassen sich Aufnahmen mit `tools/features/sds_features` auswerten, das 118 selbst anwendet (8-kanaliges WAV, 24 Bit, 48 kHz).
- **Bis 27.09.2026:** Bytes 12–15 waren `micNr` u16 und `frameNr` u16, die Daten lagen nach 118.

### 5.4 Logger (Id 99, 144 Byte)

Rahmen wie beim UnitReport, mit `len_id` = 0x63000090 und `timestamp` = 0. Die Nutzlast ist ASCII-Text der Firmware-Meldungen, mit Nullbytes aufgefüllt. Die Firmware sendet bis zu 8 Nachrichten je 100 ms.

`USBDriver::sendLogging` (Id 3, 48 Byte) wird nicht verwendet und trägt die Id an falscher Stelle (Befund 34).

### 5.5 Standort (Id 6, 144 Byte)

Rahmen wie beim Logger, mit `len_id` = 0x06000090 und `timestamp` = 0. Die Firmware sendet die Nachricht jede Sekunde, auch mit dem Grundwert, und sofort nach jedem Kommando Id 10.

| Byte der Nutzlast | Feld | Typ | Inhalt |
| --- | --- | --- | --- |
| 0–1 | unit | u16 LE | Unit-ID |
| 2 | reserved | u8 | 0 (bis 29.09.2026 Quelle PC/GNSS) |
| 3 | flags | u8 | Bit 0 = vom PC gesetzt (0 = Grundwert Ursprung) |
| 4–7 | east | i32 LE | Ost in mm |
| 8–11 | north | i32 LE | Nord in mm |
| 12–15 | up | i32 LE | Oben in mm |
| 16–127 | – | – | Nullbytes |

## 6. Änderungen

| Datum | Änderung | PC-Monitor |
| --- | --- | --- |
| 01.10.2026 | Transport wahlweise über WLAN (ESP32-C3, TCP-Port 3333, Ankündigung UDP 3334); Format unverändert | Verbindungsart TCP neben COM-Port anbieten (`socket://<ip>:3333`), Brücke über UDP 3334 finden |
| 28.09.2026 | Read (Id 2): Rohdaten vor 118; Kopf Byte 12–15 jetzt micNr u8, blockNr u8, hopNr u16 (Befund 35) | neues Kopfformat lesen, Lücken über hopNr zählen, Aufnahme als WAV |
| 28.09.2026 | Kommando Id 10 (Standort) und Nachricht Id 6 (Standort mit Quelle, jede Sekunde) neu | Standort eingeben, speichern, beim Verbinden senden; Id 6 anzeigen |
| 28.09.2026 | Mehrere und geteilte Kommandos je USB-Paket werden ausgewertet (Befund 32); vorher blieben z. B. Unit-ID und SRP bei laufendem Feedback ohne Wirkung. LCD zeigt die Unit-ID dezimal | keine Änderung nötig; Unit-ID dezimal anzeigen |
| 28.09.2026 | Kommando Id 9 (Nordabgleich) neu; CALIBRATE verarbeitet wie DETECT (vorher keine Verarbeitung) | Tab Calibrate: messen, Offset senden, beim Verbinden erneut senden |
| 29.09.2026 | Id 10 und Id 6: lokale Koordinaten Ost/Nord/Oben in mm statt WGS84; Grundwert Ursprung [0, 0, 0]; kein GNSS-Vorrang mehr. FlyBy-Bahn um den Ursprung | Eingabe Ost, Nord, Oben in m; Id 6 lesen; gespeicherten WGS84-Standort nicht mehr senden |
| 29.09.2026 | Id 3 wählt das Simulator-Szenario (2 … 7, neu FlyBy); 0/1 wie bisher. Simulation: "True Azimuth" enthält den Nordabgleich; LCD zeigt den Nordabgleich auch in DETECT | Auswahl Szenario (Werte 0 … 7); Nordabgleich nur mit ruhender Quelle messen (nicht DroneSweep/FlyBy) |
| 28.09.2026 | Azimut in Detect und UnitReport jetzt 0° = Nord, im Uhrzeigersinn, Mikrofon 0 = Nord (vorher ab der x-Achse gegen den Uhrzeigersinn) | Darstellung Nord oben, Ost rechts: x = r · sin φ, y = r · cos φ |
| 28.09.2026 | Kommando Id 8 (Feedback der Tracking-Einheit) neu | nach jeder Übernahme einer bestätigten Spur senden, Flags = 0 bei Spurende |
| 28.09.2026 | Id 7 von 20 auf 24 Byte erweitert: Lufttemperatur (i16, 0,01 °C) + 2 Byte reserviert; UTC 0 = nur Temperatur | Id 7 im neuen Format senden |
| 27.09.2026 | UnitReport Id 5 ersetzt Id 4 (Zeit u64 µs + Zeitquelle, Kopf 25 statt 16 Byte, max. 51 statt 56 Bänder) | Id 5 lesen |
| 27.09.2026 | Kommando Id 7 (UTC in µs) neu | senden |
| 27.09.2026 | Kommando Id 6 (SRP-Referenz) neu, Standard aus | bei Bedarf senden |

## 7. Offene Punkte

- **CRC der Kommandos prüfen (Befund 12).** Danach werden Kommandos mit falscher CRC verworfen.
- **Kommando-Bestätigung durch die Firmware fehlt.**
- **Beschriftung CALIBRATE/READ in `PC_Monitor_Test.ptp` (Befund 25).**
- **Vorhersage aus Id 8 für das TDOA-Suchfenster nutzen (FSL9 §10, A34).**
