# SDS_110 – Firmware der akustischen Sensoreinheit (Patent Stefan_FSL5, FIG. 1)

Modulnummern entsprechen den Bezugszeichen des Patents.

> **STM32F746ZGT6-Board (dieses Repo):** Port aus dem Repo SDS_110 (Discovery-Board) ohne LCD und
> ohne externes SDRAM. Unterschiede, Speicherbilanz und offene Punkte: `doc/Portierung_STM32F746ZGT6.md`.

## Zuordnung Patent -> Hardware
| Patent | Gerät | Inhalt |
|---|---|---|
| Sensoreinheit 112-n | **STM32F746-Board (dieses Repo)** | 114 Mikrofonarray, 116 Abtastung, 118 Vorverarbeitung |
| Vorverlagerte Stufen von 120 | STM32F746-Board | 122 Merkmale, 124 s(t) (HBD-ML), 126 Selektion/Gewichtung + Intra-Unit-Peilung |
| Processing Module 120 | **PC-Monitor, Komponente A** | 126 Inter-Unit-GCC-PHAT, 128 hyperbolische Lokalisation (≥ [3] Paare), 130 Candidate Report |
| Tracking Unit 150 | **PC-Monitor, Komponente B** (getrennt von A, Schnittstelle 140) | 152–162, Trajektorie, Feedback ŝ / x̂ |

Eine Einheit kennt nur sich selbst (Intra-Unit). Die Unterscheidung mehrerer Einheiten
erfolgt über die Unit-ID im UnitReport auf dem PC.

## Datenfluss
Nachrichtenformate Board ⇄ PC-Monitor: `doc/ICD_SDS_PC_Monitor.md`.
```
Board (112-n)                                   PC-Monitor
114 Mic ─ 116 SAI/DMA ─ 118 AGC/BP/NS ─ 122 STFT ─ 124 s(t) ─ 126 S(t),w, Peilung
                                                          │
                                                   UnitReport (USB, id 5)
                                                          ▼
                                    120: 126(e) Inter-Unit GCC-PHAT ─ 128 Hyperbeln ─ 130
                                                          │ CandidateReport (140)
                                                          ▼
                                                   150 Tracking ──► Feedback ŝ, x̂ ──► 126
```
Framing: 114 liefert Hops von 32 ms, 118 verarbeitet jeden Hop, `Frame_Assembler` bildet daraus
Analyse-Frames von 64 ms mit 50 % Überlappung (31,25 Frames/s) für 122–126.

Regel (Claim 10): Kein Modul in SDS 110 bildet eine Trajektorie.
Werte in eckigen Klammern ([3], [48 kHz], [10 µs] …) sind im Patententwurf noch offene
Werte des Ausführungsbeispiels; die Claims nennen keine Zahl.
