# Migration Processing_Module_120 (124 / 126 / 128 / 130 / 120 / ProcessingTask)

**Zuordnung (siehe README):** Das Board ist Sensoreinheit 112-n; die Klasse `Processing_Module_120`
hier ist die *Vorstufe* auf der Einheit (122, 124, 126-intra). Inter-Unit-Korrelation, `128::solve()`
und der Candidate Report liegen im Processing Module auf dem PC. Das Board sendet einen `UnitReport`.

| Neu | Aus SDS | Änderung |
|---|---|---|
| Machine_Learning_Module_124.{hpp,cpp}, HBD.{hpp,cpp} | HBD-Vorlage (klassischer Harmonic Band Detector) | HBD auf |X| aus 122; Brücke HBD -> s(t) über Band-SNR, Harmonischen-Boost, Score-Gate |
| Correlation_Processing_Module_126.{hpp,cpp} | Algorithm/SRPPhat, Model/SDS_SRPBuffers | GCC-PHAT jetzt quellkonditioniert (S(t), w); SRP-Gitterscan durch TDOA-Least-Squares-Peilung ersetzt; kiss_fft → CMSIS |
| Localisation_Module_128.{hpp,cpp} | Algorithm/DistanceEstimator, SRP_DAS_Distance, SRPPhat::calibrateAzimuth | hyperbolische LS-Lösung für N≥3; Einzel-Unit: Peilung + Pegel-Fallback |
| Output_Interface_130.{hpp,cpp} | USB_SendDetection in SRPTask | Legacy-Frame + neuer Candidate Report (id 4) |
| Processing_Module_120.{hpp,cpp} | Tasks/SRPTask (detectHandler/aiHandler) | Orchestrator 112→122→124→126→128→130 |
| Infrastructure/Tasks/ProcessingTask.{hpp,cpp} | Tasks/SRPTask (Task-Hülle) | Moduswahl DETECT/READ |
| entfällt | Algorithm/TrackingTask, SRPPhat::filterAzimuth | Tracking Unit 150, nicht in SDS 110 (Claim 10) |
| entfällt | Third_Party/kiss_fft | CMSIS-DSP |

## Speicher (statisch)
Stand 25.09.2026 (gemessen mit `sizeof`):
- Processing_Module_120: spectra_ 8 × 16,4 kB = 131 kB (SDRAM-Sektion)
- Die Instanz von Processing_Module_120 liegt komplett im SDRAM (`SDS110_SDRAM_SECTION static`), damit auch
  ihre Mitglieder: 122 ≈ 86 kB (`MEL_MAX_BINS_PER_BAND` = 128), 124 ≈ 26 kB, 126 ≈ 49 kB,
  `Frame_Assembler` (in 112) ≈ 98 kB.
- ProcessingTask-Stack 16 kB; FreeRTOS-Heap auf 64 kB erhöht (Befund 13).

## Laufzeit pro Frame (F746, grob)
8 × RFFT 4096 ≈ 2 ms · 28 Paare × (Spektrum-Produkt + IFFT) ≈ 28 × 0,35 ms ≈ 10 ms · Rest < 1 ms
→ ~13 ms pro Frame. Seit Befund 17 (50 % Überlappung) ein Frame je 32 ms → ~40 % Last (geschätzt,
am Board mit den Task-Statistiken prüfen).

## Offene Punkte
Stand 25.09.2026 – Details und Commits in `doc/Analyse_Befunde.md`.
1. HBD-Parameter (HBD_BAND_SNR_DB, HBD_GATE_FLOOR, perBandSnrDb) mit Aufnahmen abstimmen.
   *Teilweise erledigt: Rauschboden in dBFS (Befund 7), Band-Selektion nur für Harmonische mit HBD-Haltezeit
   (Befund 24) – im Simulator abgestimmt, mit echten Aufnahmen noch zu prüfen.*
2. Feedback-Empfang (Nachrichtentyp 4 in USBTask → Output_Interface_130::pollFeedback). *Offen.*
   Hinweis: `Output_Interface_130.hpp` nennt dafür Typ 6 – Nummer mit dem PC-Monitor festlegen.
3. PC-Monitor (Processing Module 120): Inter-Unit-GCC-PHAT auf den Referenzkanal-Spektren ≥ [3]
   Einheiten (Patent Abschnitt 1/6; Werte in Klammern noch offen) mit `126::crossCorrelate()` und
   `128::solve()` als Referenzcode. Voraussetzung: gemeinsame Zeitbasis ([10 µs]) und Übertragung der
   Referenzspektren im UnitReport. Bis dahin: Schnitt der Peilstrahlen ≥ 2 Einheiten. *Offen.*
4. Azimut-Kalibrierung nach Messung über `setCalibration()` setzen. *Offen.* Die alten Werte (+12°, ×0,98)
   stammen aus dem SRP-Code vor der Migration; seit der Vorzeichenkorrektur der Peilung (Befund 23) nicht
   übertragbar, neu messen.
5. ~~Linker-Sektion `.sdram_data`~~ – *erledigt*.

Weitere Änderungen seit der Migration: GCC-PHAT-Peak-Ratio mit lokalen Nebenmaxima (Befund 8), Distanzpegel
vor NS/AGC (Befund 9, `LEVEL_DIST_K_REF` noch unkalibriert), einheitliche Konfidenz (Befund 18), USB-Senden
über TX-Ringpuffer (Befund 11).
