# SDS_110 – Offene Punkte

Stand 28.09.2026 (nachgeführt; erste Fassung 27.09.2026)

Dieses Dokument listet alle offenen Punkte, nach Bereich geordnet. Sie stammen aus drei Quellen:
- `doc/Analyse_Befunde.md` (Befunde 1–43)
- `doc/Analyse_27_09_2026.md`
- dem Trainingskonzept ML124

Außerdem sind die Punkte des PC-Monitors aufgeführt, die die Firmware berühren. Das Online-Dokument wird aus dieser Datei nachgeführt. Die zuletzt erledigten Punkte stehen am Ende.

## Entscheidungen

| Datum | Thema | Entscheidung | Folgen |
| --- | --- | --- | --- |
| 28.09.2026 | T7: Daten mehrerer Einheiten zum PC | **Nur TDOA übertragen:** Jede Einheit sendet ihre Intra-Unit-Ergebnisse (TDOA der 28 Paare bzw. Peilung, Qualität, µs-Zeit), keine Spektren oder Signale | Die Inter-Unit-GCC-PHAT (FSL9 §5, Anspruch 1(e); PC-Traceability P4) entfällt. Die Lokalisation mehrerer Einheiten (§6) muss aus Peilungen und µs-Zeitstempeln der Einheiten erfolgen, also Kreuzpeilung und Zeitdifferenzen gleicher Ereignisse, und ist eine Abweichung von FSL9. Der UnitReport braucht dafür die Paar-TDOA oder mindestens die Peilung mit Unsicherheit; die Formatänderung wird mit T7 festgelegt. Die Bandbreite bleibt klein (≈ 144 B je Frame), passend für WiFi/USB. |
| 28.09.2026 | Azimut | 0° = Nord, im Uhrzeigersinn, Mikrofon 0 = Nord | umgesetzt (Befund 41) |
| 29.09.2026 | Standort der Einheit | Lokale Koordinaten Ost/Nord/Oben in m statt WGS84; Grundwert Ursprung [0, 0, 0], gesetzt vom PC-Monitor (Id 10); kein GNSS-Vorrang | umgesetzt; der Simulator (FlyBy) legt die Bahn um den Ursprung; GNSS offen (A7) |

## Firmware ohne Board

Alles hier lässt sich auf dem Host umsetzen und mit `test/host` belegen.

| Befund | Aufgabe | Priorität | Status |
| --- | --- | --- | --- |
| 43 | Debug-Build: Inlining in 118/122/124/126 (Aufrufe je Bin trotz Pragma) | mittel | Teilweise (Simulator, 114); Release-Build vergleichen |
| 34 (A34) | Vorhersage aus Feedback Id 8 nutzen: TDOA-Suchfenster ±2 ms um die vorhergesagte Verzögerung, zusätzlich als Führung für Befund 26 | mittel | Offen |
| 30 | Start von SAI/Verarbeitung nur nach erfolgreicher Initialisierung | mittel | Offen |
| 31 | Beim Wechsel Simulation ↔ Hardware Hop-Puffer und Frame_Assembler zurücksetzen | mittel | Offen |
| 12 | CRC der USB-Kommandos prüfen (der PC-Monitor sendet sie korrekt) | mittel | Offen |
| 16 | Inter-Unit-Zeitbasis (GNSS-PPS in Hardware-Version 2 oder PTP), `syncTimeDifference` anwenden | mittel | Offen |
| 34 | TX-Ring beim Trennen leeren; Kopf von `sendLogging()` angleichen | niedrig | Offen |
| 8 | Peak-Ratio in `srpScan()` wie in `crossCorrelate()` berechnen | niedrig | Offen |
| Doku | Merkmalshash auf einen Header mit den Merkmalskonstanten begrenzen (dann ändern reine Code-Optimierungen in 114/118/122 die Version nicht) | mittel | Offen |

## ML-124 (Trainingskonzept, ML_Test)

AP 1 und AP 3–7 sind erledigt. Offen sind:

| Bezug | Aufgabe | Priorität | Status |
| --- | --- | --- | --- |
| ⚠ PRÜFEN | Freigabe der Drohnendaten fürs Training prüfen (Prüfpunkte 1–4 in `docs/Daten_ML124.md`) | hoch | Offen |
| AP 2 | Inventur echter Drohnen abschließen; saubere echte Aufnahmen beschaffen (READ liefert jetzt Rohdaten, PC-Monitor nimmt WAV auf) | hoch | Offen |
| 38 | Synthetische Drohnen bis BPF ca. 700 Hz erweitern, Datensatz neu erzeugen und neu trainieren | hoch | Offen |
| AP 8 | Board-Aufnahmen und Endabnahme; Voraussetzung Befund 35 ist erledigt, Blocker 2–4 sind offen | mittel | Offen |
| FSL9, AP 7 | **ML-Modus (Patent verlangt ML in 124): k5_h48_d3 besteht die Abnahme nicht** (Peilung im ML-Modus 30 dB: Median 94°, Ratio 1,05; selektiert Bänder ohne Drohnenenergie). Voraussetzung: neu trainieren (38, AP 2, Datenfreigabe) und AP 7 wiederholen; bis dahin Stufe Schatten | hoch | Offen |
| 37 | HBD-Vergleich ab Frame 94 wiederholen (nach der HBD-Anlaufzeit) | mittel | Offen |
| 38 | Validierung: Umwelt-Clips getrennt vom Training ziehen | niedrig | Offen |
| Lizenz | ESC-50 (CC BY-NC 3.0) vor einer kommerziellen Verwertung klären oder ersetzen | niedrig | Offen |

## Hardware und Messungen am Board

Die Blocker 2–4 sind zurückgestellt. Ohne sie gibt es keine echten Mikrofondaten, betroffen sind der Modus Real und alle akustischen Messungen.

| Befund | Aufgabe | Priorität | Status |
| --- | --- | --- | --- |
| 2 | SAI-DMA einrichten (MspInit, Streams, IRQ-Handler) | hoch | Zurückgestellt |
| 3 | CubeMX an die eigene Platine anpassen: SAI1 PE4/PE5, I2C2, PE3; SAI-Takt aus PLLI2S, SPDIFRX aus | hoch | Zurückgestellt |
| 4 | ADAU7118-Registertabelle und I2C-Adresse (0x4B oder 0x3A) klären | hoch | Zurückgestellt |
| 17, 42, 44 | Rechenlast gemessen (Release, 28.09.2026): Proc 29,0 ms bei 32 ms Budget (118 4,6 · Fa 0,0 · Fr 3,3 · Fs 1,64 × 7), Sim 6,4 ms (nur Simulation, Sim + Proc 35,4 ms). Echtbetrieb passt | – | Erledigt |
| 44 (Reserve) | Falls die Integration mehr Rechenzeit braucht: FFT-Arbeitspuffer (`buf_`, `fftOut_`, 32 KB) und Twiddles (32 KB) in den DTCM (64 KB, ohne Wartezyklen); dafür ML-Instanz und Heap aus dem DTCM verlegen, Linker-Abschnitt `.dtcm`, 122 geändert → Neuexport mit `--same-as`. Die Twiddles im SRAM1 brachten nur ~3 % (Fs 1,69 → 1,64 ms); mit `SDS110_FFT_TABLES_IN_RAM 0` lassen sich 32 KB RAM zurückgewinnen | niedrig | Reserve |
| 26, 29, 32, 35 | Am Board prüfen: Peilung bei hohem f0, Lock-Zähler am LCD, Unit-ID/SRP bei laufendem Feedback, READ-Rohdaten mit Hop-Nummer | mittel | Offen |
| 5 | Abtastrate messen: FSYNC 47,991 kHz, BCLK 12,286 MHz | mittel | Offen |
| 6, 34 | Bitlage im 32-Bit-Slot und SAI-Taktflanke am Oszilloskop prüfen | mittel | Offen |
| 9 | `LEVEL_DIST_K_REF` mit realer Drohne in bekanntem Abstand kalibrieren | niedrig | Offen |
| 23 | Nordabgleich am Board messen (Werkzeug: USB Id 9, PC-Monitor Tab Calibrate) | niedrig | Offen |
| 21 | Leistungsgewinn durch gecachten SRAM1 messen | niedrig | Offen |

## System und PC-Monitor

| Bezug | Aufgabe | Priorität | Status |
| --- | --- | --- | --- |
| T7 | Mehrere Einheiten mit „nur TDOA“ (siehe Entscheidungen): UnitReport um Paar-TDOA bzw. Peilungsunsicherheit erweitern, PC: je Einheit Port/Unit-ID und Standort, Kreuzpeilung, Candidate Report | hoch | Offen |
| – | Unit-ID im PC-Monitor speichern und beim Verbinden senden; gemeldete ID im Bedienfeld anzeigen | niedrig | Zurückgestellt (28.09.2026) |
| 9 | PC-Monitor: neue Skala des UnitReport-Felds `level` berücksichtigen | niedrig | Offen |
| 25 | Moduswerte in `PC_Monitor_Test.ptp` angleichen (CALIBRATE = 2, READ = 3) | niedrig | Offen |
| A7 | GNSS (Hardware-Version 2) mit lokalen Koordinaten: Bezugspunkt des Ursprungs in WGS84 festlegen und GNSS-Position umrechnen; bis dahin nur Id 10 vom PC (Entscheidung 29.09.2026) | niedrig | Offen |

## Dokumentation und Klärungen

| Bezug | Aufgabe | Priorität | Status |
| --- | --- | --- | --- |
| Doku | Veraltete Stellen korrigieren: Kommentare in `ProcessingTask.hpp` und `Frame_Assembler.hpp`, Befunde 6, 9, 20, `SPEED_OF_SOUND`, Trainingskonzept Frage 6 | mittel | Offen |
| Konzept | Offene Fragen 1–5 im Trainingskonzept klären (Patent-Vorgaben Abschnitt 3, Datenlage, Board-Aufnahmen, ML ersetzt oder ergänzt HBD, Framework) | mittel | Offen |
| Doku | `doc/ADUA_Design.md` korrigieren oder mit Hinweis auf Befund 4 versehen | niedrig | Offen |
| Doku | `doc/Test & Integration.md` füllen oder entfernen | niedrig | Offen |

## Seit dem 27.09.2026 erledigt

| Befund / Bezug | Thema | Nachweis |
| --- | --- | --- |
| 1 | ProcessingTask läuft | Board-Messungen 28.09.2026 |
| 26 | Mehrdeutige TDOA-Paare bei hohem f0: robuste LS, SRP-geführte Neuwahl, sonst ungültig | `t_bearing_f0`, `m_bearing_f0` |
| 44 | Frame_Assembler ohne Schieben (118 schreibt in Hop-Slots), FFT-Twiddles im internen RAM, Heap 48 kB; Merkmalsversion `5e2aeca1b90a4d17`, Modell neu exportiert | `t_frame_assembler`, `check_features`, `t_ml124` |
| 27, 42 | Lücken im Hop-Strom erkennbar; MicFrame-Zeilen aufgefüllt (Cache-Aliasing); Merkmalsversion `3a685d01f22f3eea`, Modell neu exportiert (Merkmale bitgleich) | `t_hop_gap`, `check_features`, `t_ml124` |
| 28 | Simulation an den 32-ms-Hop gekoppelt | `t_hopclock` |
| 29 | `SDS_Data`-Getter liefern bei Sperr-Timeout den Wert statt 0; Setter mit zweitem Versuch; Zähler am LCD | `t_sds_data` |
| 32 | Mehrere und geteilte Kommandos je USB-Paket (`CommandAssembler`) | `t_usb_commands` |
| 33 | UnitReport mit µs-Zeit, vom PC-Monitor gelesen | `t_unit_report`, PC `test_messages` |
| 34 | Feedback nach 2 s ohne Nachricht zurücksetzen | `t_feedback` |
| 35 | READ: Rohdaten vor 118, Hop-Nummer im Kopf; PC zählt Lücken, nimmt WAV auf | ICD 5.3, PC `test_read_record` |
| 36, 20 | Veraltetes Modell ersetzt | Commit 5017f27 |
| 22 | Nachrichtentyp für das Tracking-Feedback: Id 8 | ICD 4.3 |
| AP 5–7 | Export, Inferenz in 124, Vergleich HBD ↔ ML | `t_ml124`, `doc/Vergleich_HBD_ML124.md` |
| Doku | `Analyse_27_09_2026.md` im Repo; Befunde 26–43 in `Analyse_Befunde.md` | – |
| neu | USB Id 9 Nordabgleich, Id 10 Standort (seit 29.09.2026 lokal Ost/Nord/Oben, vorher WGS84), Id 6 Standort-Meldung | `t_azimuth`, `t_local_position` |
| neu | SRP-Referenzscan: Grob-/Feinraster, nur jeden 4. Frame (Board: 32,1 → 0,1 ms) | `t_gcc_direct` |
| neu | Simulator: innere Schleife ohne Aufrufe, xorshift-Rauschen | Host-Prüfungen |
