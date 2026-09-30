# SDS_110 – Traceability Report FSL9

Stand 27.09.2026. Online-Fassung mit änderbarem Status und Diagramm:
[Traceability Report FSL9](https://claude.ai/code/artifact/28cd8a5a-fb43-4f4b-8f02-fa667071683c).

## Zusammenfassung

Von 36 zugesicherten Werten aus FSL9 sind 19 erfüllt, 11 teilweise erfüllt, 4 nicht erfüllt, 1 offen und 1 nicht im Umfang der Firmware. Die Signalkette 118 → 122 → 126 hält die Parameter der bevorzugten Ausführung weitgehend ein: Frames, FFT, Bänder, Selektion, Gewichtung, GCC-PHAT und Peak-Ratio. Die Abweichungen liegen vor allem im Systemaufbau und in den Zeitvorgaben:

- **Eine statt mindestens zwei Sensoreinheiten** (`NUM_UNITS = 1`). Die Distanz kommt deshalb aus einem Pegelmodell, das FSL9 nicht vorsieht; Multilateration und der Zwei-Unit-Modus werden nicht genutzt.
- **Keine Synchronisation der Einheiten auf ≤ 10 µs.** Der Report überträgt jetzt µs und UTC nach Abgleich vom PC über USB (~1 ms genau); die µs-Synchronisation soll GNSS-PPS in HW-Version 2 liefern.
- **Rechenzeit:** Das Processing Module braucht am Board 31,3 ms je Frame und liegt damit knapp unter dem Frame-Takt von 32 ms (0,7 ms Reserve). Ob ~30 Reports/s mit echten Mikrofonen durchlaufen, ist erst mit der 8-Mikrofon-Platine nachweisbar; im Simulationsbetrieb reicht die Zeit nicht (Simulator 13,4 ms).
- **ML-Modul:** s(t) kommt vom HBD. Das trainierte MLP läuft nur im Schatten und ist kein CNN. Die Labels folgen dem Band-SNR statt der Harmonischen aus Telemetrie.

Die Tracking-Einheit (Abschnitte 8–9, Ansprüche 6–8 und 11) ist nicht Teil von SDS_110.

## Grundlage und Methode

Geprüft wurde die Firmware SDS_110 `master` Commit `d046577` (A3 nachgeführt nach der Umstellung auf 200 mm) gegen `doc/Stefan_FSL9.docx`: Abschnitte 1–12 (bevorzugte Ausführung) und Ansprüche 1–5 und 10 (Detektionssystem). Die Anforderungs-IDs A1–A36 sind in diesem Report vergeben; FSL9 selbst hat keine IDs.

| Nachweisart | Bedeutung |
| --- | --- |
| Code | am Quelltext nachvollzogen, Referenz `Datei:Zeile` relativ zu `Core/SDS_110/` |
| Host-Test | gemessen mit `make -C test/host check` bzw. `measure` (Simulator, x86) |
| Board | gemessen am STM32F746G-DISCO, Simulationsbetrieb (Debug- und Release-Build gleich schnell) |

| Status | Bedeutung |
| --- | --- |
| Erfüllt | Wert wie in FSL9 umgesetzt |
| Teilweise | umgesetzt, aber mit Abweichung im Detail |
| Nicht erfüllt | Wert fehlt oder weicht grundlegend ab |
| Offen | noch nicht geprüft oder Klärung nötig |
| Nicht im Umfang | gehört nicht zur Firmware SDS_110 |

Nicht geprüft wurde der Hardware-Pfad mit echten Mikrofonen. Er ist durch die Blocker 2–4 in `doc/Analyse_Befunde.md` gesperrt.

## Traceability-Matrix

Je Zeile ein zugesicherter Wert aus FSL9 mit Ist-Wert, Code-Referenz (relativ zu `Core/SDS_110/`) und Status.

| ID | FSL9 | Anforderung | Zugesichert | Ist | Code-Referenz | Status |
| --- | --- | --- | --- | --- | --- | --- |
| A1 | §1, Anspr. 1/10 | Anzahl Sensoreinheiten | N ≥ 2 (Multilateration ab 3) | 1 Einheit | `SDS_110_Config.hpp:12` | Nicht erfüllt |
| A2 | §1 | Mikrofone je Einheit | M = 8, regelmäßiges Oktagon | 8, Oktagon | `SDS_110_Config.hpp:13, Sensor_Unit_112/Microphone_Array_114.cpp:23` | Erfüllt |
| A3 | §1 | Arraydurchmesser | 200 mm | 200 mm (Radius 0,10 m, seit 27.09.2026; vorher 400 mm) | `SDS_110_Config.hpp:38` | Erfüllt |
| A4 | §1 | Abtastung | PDM, 48 kHz, synchron | 48 000 Hz; Board 47 991 Hz (186 ppm, Befund 5) | `SDS_110_Config.hpp:14, Sensor_Unit_112/Sampling_Circuitry_116.cpp configureSai()` | Erfüllt |
| A5 | §1 | Vorverarbeitung 118 | AGC, Bandpass 80 Hz–8 kHz, adaptive Rauschunterdrückung | Butterworth-HPF/LPF 80–8000 Hz, NS, AGC | `SDS_110_Config.hpp:15–16, Sensor_Unit_112/Pre_Processor_118.hpp:5–12` | Erfüllt |
| A6 | §1 | Synchronisation der Einheiten | IEEE 1588 PTP, ≤ 10 µs | DWT-Laufzeit in µs; UTC-Abgleich vom PC über USB (Typ 7, ~1 ms); geplant: GNSS-PPS in HW-Version 2 | `Infrastructure/Utils/TimeBase.hpp:9–11, Infrastructure/Utils/UtcClock.hpp:24` | Nicht erfüllt |
| A7 | §1 | Positionen der Einheiten | vermessen auf 0,1 m, gespeichert | Standort in lokalen Koordinaten Ost/Nord/Oben (mm) relativ zum Ursprung [0, 0, 0] über USB Id 10 vom PC, Rückmeldung Id 6; Grundwert Ursprung (t_local_position, seit 29.09.2026 statt WGS84/GNSS). Für die Lokalisation gilt weiter ein Referenzpunkt (0, 0, 0); relative Positionen mehrerer Einheiten mit PC-Monitor T7 | `Infrastructure/Utils/LocalPosition.hpp, USBTask.cpp handlePosition, Processing_Module_120.cpp:37` | Teilweise |
| A8 | §2 | Frames | 64 ms, 50 % Überlappung | 3072 Samples, Hop 1536 (Befund 17) | `SDS_110_Config.hpp:20, 28–29` | Erfüllt |
| A9 | §2 | STFT | 4096 Punkte | N_FFT = 4096, Hann-Fenster | `SDS_110_Config.hpp:21, Feature_Extraction_Module_122.cpp:65` | Erfüllt |
| A10 | §2 | Frequenzbänder | B = 64, Δf = 62,5 Hz, 80 Hz–4 kHz | 64 Bänder, 62,5 Hz, 80–4000 Hz | `SDS_110_Config.hpp:23–26, Feature_Extraction_Module_122.cpp:53` | Erfüllt |
| A11 | §2 | Referenzkanal | Mikrofon 1 | REF_MIC = 0 (erstes Mikrofon) | `SDS_110_Config.hpp:27` | Erfüllt |
| A12 | §2 | Merkmale | log-Power je Band, MFCC Ordnung 13, spektraler Fluss, AM-Spektrum über 0,5 s | log-Power je Band, Log-Mel 40 statt MFCC 13, Fluss, AM-Tiefe (std/mean) über 0,5 s statt Spektrum | `Feature_Extraction_Module_122.cpp:83–138, SDS_110_Config.hpp:73, 77` | Teilweise |
| A13 | §3 | Modell | CNN mit B = 64 Sigmoid-Ausgängen | s(t) vom HBD; MLP 845→48→64→64 (Sigmoid) nur im Schatten | `Machine_Learning_Module_124/ML124_Config.hpp:21, Machine_Learning_Module_124.cpp:34, 130` | Teilweise |
| A14 | §3, Anspr. 3 | Training | gleiche Hardware, UAVs 20–200 m, harmonische Negative, Label je Band aus Harmonischen ±½ Band, BCE | synthetische Drohnen + Umweltaufnahmen, Label σ(SNR_b / 3 dB), BCE | `ML_Test app/train_ml124/ml124_data.py, train.py` | Teilweise |
| A15 | §3 | Generalisierung | Hold-out-AUC je Band 0,65–0,95 für nicht trainierten Typ | Modell ohne 6-Rotor-Drohnen vorhanden, Wert hier nicht geprüft | `ML_Test model/ml124/k5_h48_d3_ohne6rot` | Offen |
| A16 | §3 (optional) | Glättung s(t) | 3 Frames | 6 Frames (0,19 s) | `SDS_110_Config.hpp:80, Machine_Learning_Module_124.cpp:99` | Teilweise |
| A17 | §4, Anspr. 4 | Selektionsschwelle | θ_sel = 0,5 | 0,5 | `SDS_110_Config.hpp:83, Correlation_Processing_Module_126.cpp:60` | Erfüllt |
| A18 | §4 | Mindestzahl Bänder | weniger als B_min = 3 Bänder: kein TDOA | füllt auf die 3 stärksten Bänder auf und berechnet TDOA; Report nur bei ≥ 3 Bändern über θ_sel | `Correlation_Processing_Module_126.cpp:70–76, Infrastructure/Model/SDS_Data.cpp:33, Processing_Module_120.cpp:110` | Teilweise |
| A19 | §4, Anspr. 4 | Gewicht | g(p) = p | p^γ mit γ = 1, × Feedback-Faktor | `SDS_110_Config.hpp:85, Correlation_Processing_Module_126.cpp:82` | Erfüllt |
| A20 | §4, Anspr. 2 | Gemeinsame Selektion | einmal abgeleitet, für alle Einheiten gleich | eine Selektion für alle 28 Paare je Frame | `Correlation_Processing_Module_126.cpp:218 (prepareBins einmal je Frame)` | Erfüllt |
| A21 | §5, Anspr. 1(e) | GCC-PHAT | nur ausgewählte Bins, gewichtet, übrige 0 | Schnellpfad ±32 Lags oder IFFT, gleiche Formel (t_gcc_direct) | `Correlation_Processing_Module_126.cpp:118–216` | Erfüllt |
| A22 | §5 | Lag-Bereich | τ bis ±d_ij / c je Paar | für alle Paare ±(größter Mikrofonabstand / c) × 1,1 | `Correlation_Processing_Module_126.cpp:28` | Teilweise |
| A23 | §5 | Schallgeschwindigkeit | temperaturkorrigiert | c = 331,3 · √(1 + T/273,15) aus der Lufttemperatur im Sync-Kommando (USB Typ 7, −40…+60 °C); ohne Temperatur 343 m/s; wirkt auf 126, 128 und Simulator (t_sound_speed) | `Infrastructure/Utils/SoundSpeed.hpp, Processing_Module_120.cpp:88, Correlation_Processing_Module_126.cpp:98` | Erfüllt |
| A24 | §5 | Interpolation | Parabel um das Maximum | Parabel | `Correlation_Processing_Module_126.cpp:201` | Erfüllt |
| A25 | §5 | Peak-Ratio | verwerfen unter 1,5 | PEAK_RATIO_MIN = 1,5 | `SDS_110_Config.hpp:88, Correlation_Processing_Module_126.cpp:209` | Erfüllt |
| A26 | §5 | Intra-Unit-Peilung | Kreuzkorrelation der Mikrofone, kein Beamforming | TDOA-Least-Squares über 28 Paare | `Correlation_Processing_Module_126.cpp:218–270` | Erfüllt |
| A27 | §6, Anspr. 1(f) | Lokalisation | N ≥ 3 Multilateration, N = 2 TDOA + zwei Peilungen | N = 1: Peilung + Pegel-Distanz (nicht in FSL9); Multilateration vorhanden, ungenutzt. Entscheidung 28.09.2026 für mehrere Einheiten: nur TDOA/Peilung und µs-Zeit übertragen, Lokalisation auf dem PC (PC-Monitor T7), keine Inter-Unit-GCC-PHAT | `Localisation_Module_128.cpp:26, 67, SDS_110_Config.hpp:104` | Nicht erfüllt |
| A28 | §6, FIG. 5 | Referenzpunkt, Azimut | Zentroid der Einheiten, Azimut ab Nord | Azimut 0° = Nord, im Uhrzeigersinn, Mikrofon 0 = Nord (festgelegt 28.09.2026); Referenzpunkt = Arraymitte (bei einer Einheit gleich dem Zentroid); gilt für 126, 128, Simulator und LCD (t_azimuth); Nordabgleich der aufgestellten Einheit über USB Id 9 (Offset in 128, Modus CALIBRATE, PC-Monitor Tab Calibrate) | `Infrastructure/Utils/Azimuth.hpp, Correlation_Processing_Module_126.cpp:284, Localisation_Module_128.cpp:58, USBTask.cpp:handleAzimuthOffset` | Erfüllt |
| A29 | §7, Anspr. 1(g) | Zeitstempel im Report | UTC, µs | UnitReport id 5: u64 µs + Zeitquelle; UTC nach Abgleich (USB Typ 7), bis GNSS nur ~1 ms genau; PC-Monitor liest id 5 noch nicht (Befund 33) | `Data_Interface_140/Candidate_Report_140.hpp:33, Output_Interface_130.cpp:20, 52, USBTask.cpp:84` | Teilweise |
| A30 | §7, Anspr. 1(g) | Inhalt des Reports | φ, r, Qualität (Paare, Residuum), ausgewählte Bänder + p_b | UnitReport: Peilung, Residuum, Paare, Pegel, Bänder + p_b; φ/r im CandidateReport auf dem PC | `Data_Interface_140/Candidate_Report_140.hpp:30–41` | Teilweise |
| A31 | §7 | Report-Rate, Format | ≈ 30 Reports/s, feste Binärstruktur | Binärstruktur 128 Byte (t_unit_report); Proc 31,3 ms < 32-ms-Frame-Takt (0,7 ms Reserve), mit echten Mikrofonen nicht nachgewiesen | `Output_Interface_130.cpp, LCDTask.cpp (Zeitanzeige)` | Teilweise |
| A32 | §7, Anspr. 10 | Keine Trajektorie im SDS | SDS bildet keine Trajektorie | keine Tracking-Funktion in der Firmware | – | Erfüllt |
| A33 | §10 | Feedback Schwelle/Gewicht | ŝ_b > 0,6: θ = 0,3, Gewicht × (1 + ŝ_b) | wie gefordert; Feedback über USB Id 8 von der Tracking-Einheit (PC-Monitor), Zurücksetzen nach 2 s ohne Feedback oder mit Flags = 0 (t_feedback) | `Correlation_Processing_Module_126.cpp:45–53, Output_Interface_130.cpp pollFeedback, Infrastructure/Utils/FeedbackCodec.hpp` | Erfüllt |
| A34 | §10 | Feedback Suchfenster | TDOA-Fenster ±2 ms um die Vorhersage | TDOA_WINDOW_S definiert, ungenutzt | `SDS_110_Config.hpp:131` | Nicht erfüllt |
| A35 | §11 | Ohne UAV | keine Selektion, keine Korrelation, kein Report | kein Report (0 % bei Rauschen, m_selection); Korrelation läuft wegen A18 immer | `Correlation_Processing_Module_126.cpp:70` | Teilweise |
| A36 | §8–9, Anspr. 6–8, 11 | Tracking-Einheit | ŝ mit α = 0,2, Kosinus > 0,7, χ²-Gate, 3 Reports, 2 s | nicht Teil der Firmware; umgesetzt im PC-Monitor `app/tracking/` (Traceability dort, P14–P21) | – | Nicht im Umfang |

## Rechenzeit und Ressourcen am Board

FSL9 sichert ≈ 30 Reports/s zu, also einen Frame je 32 ms (Hop 1536 Samples bei 48 kHz). Das Processing Module hält das mit 31,3 ms knapp ein (0,7 ms Reserve); mit dem Simulator sind es rund 45 ms. Beiträge: Array 200 mm (GCC-PHAT ±32 statt ±64 Lags, K GCC 8,5 → 4,9 ms) und SRP-Referenzscan standardmäßig aus (2,3 → 0 ms); zusammen Proc 37 → 31,3 ms.

Board-Messung STM32F746G-DISCO, LCD-Zeitzeilen, 27.09.2026 (master `1786e2a`, Array 200 mm, SRP-Referenzscan aus, Simulationsbetrieb; Debug und Release gleich). Simulator aus einer früheren Messung (`d046577`):

| Stufe | ms je Frame |
| --- | --- |
| P – 118 + Analysefenster | 7 |
| F – 122 (8 FFT + Merkmale) | 15 |
| M – 124 (HBD + MLP im Schatten) | 4 |
| K – 126 Selektion | 0,1 |
| K – 126 GCC (28 Paar-Korrelationen, ±32 Lags) | 4,9 (vorher 8,5 bei ±64) |
| K – 126 SRP-Referenzscan (nur Vergleich, standardmäßig aus; ein: 2,3) | 0,0 |
| **Processing Module gesamt** | **31,3** (Summe der Stufen 31,0) |
| Simulator (nur Testbetrieb) | 13,4 |
| **mit Simulator** | **44,7** |

Mit echten Mikrofonen fällt der Simulator weg, das Budget muss also das Processing Module allein einhalten. Ausgangswert vor der Optimierung war 380 ms je Hop. Der Release-Build (`-Os`) ist nicht schneller als der Debug-Build (`-O0`): Alle rechenintensiven Dateien (118, 122, 124 mit HBD, 126, Simulator) binden `Infrastructure/Utils/DspOptimize.hpp` ein, das auf dem Board `-O2` erzwingt. Die Build-Wahl ist für die Rechenzeit damit ohne Belang.

| Ressource | Belegt | Verfügbar |
| --- | --- | --- |
| Flash | 489 kB (46,7 %) | 1 MB |
| RAM (DTCM + SRAM1) | 288 kB (92,7 %) | 304 kB |
| SDRAM | 390 kB (6,2 %) | 6 MB (+ 2 MB Framebuffer) |

FSL9 nennt keine Grenzen für Speicher. Der interne RAM ist durch die Verlagerung von 122, 124 und 126 weitgehend belegt.

## Abweichungen und nächste Schritte

Zuerst anzugehen ist die Rechenzeit. Der Arraydurchmesser ist seit 27.09.2026 auf 200 mm umgestellt (A3 erfüllt). Die übrigen Punkte hängen an der Hardware (mehrere Einheiten, PTP) oder sind kleine Code-Änderungen.

| Prio | IDs | Abweichung | Maßnahme | Bezug |
| --- | --- | --- | --- | --- |
| 1 | A31 | Proc 31,3 ms, nur 0,7 ms Reserve; Rate mit echten Mikrofonen nicht nachgewiesen | mit der 8-Mikrofon-Platine übersprungene Hops zählen; für Reserve 122 (15 ms, 8 FFT) und 118 (7 ms) prüfen; SRP-Referenzscan bleibt aus (USB Typ 6), Release-Build bringt nichts (`DspOptimize.hpp`) | Befund 28, `doc/Host_Tests.md` |
| 2 | A18, A35 | TDOA auch bei < 3 Bändern | bei weniger als B_min Bändern keine Korrelation rechnen, wie FSL9 §4 | `Correlation_Processing_Module_126.cpp:70` |
| 3 | A29, A6 | UTC nur ~1 ms genau (USB), keine µs-Synchronisation der Einheiten | Firmware sendet µs (id 5) und nimmt UTC an (Typ 7): PC-Monitor anpassen; HW-Version 2: GNSS-PPS stellt den µs-Zähler (Zeitquelle 2), WiFi nur für Daten (Software-PTP über WiFi erreicht ≤ 10 µs nicht sicher) | Befunde 16, 33 |
| 4 | A1, A7, A27 | eine Einheit, Pegel-Distanz | Multilateration mit N ≥ 3 Einheiten, wenn die Hardware vorliegt; Positionen der Einheiten konfigurieren | Blocker 1–4 |
| 5 | A22, A34 | Lag-Fenster global, Fenster aus Feedback ungenutzt | Fenster je Paar aus d_ij; `TDOA_WINDOW_S` um die Vorhersage anwenden (c ist seit 28.09.2026 temperaturkorrigiert, A23) | Befunde 26, 34 |
| 6 | A12–A16 | Merkmale, Modell, Labels, Glättung weichen ab | entscheiden, ob die Abweichungen als Variante nach §12 gelten; sonst MFCC 13, CNN und Harmonischen-Labels umsetzen | `doc/Vergleich_HBD_ML124.md` |
| 7 | A15 | Hold-out-AUC nicht geprüft | Auswertung des Modells ohne 6-Rotor-Drohnen in ML_Test gegen 0,65–0,95 prüfen | ML_Test `docs/Training_ML124.md` |

Die Befundnummern beziehen sich auf `doc/Analyse_Befunde.md`.
