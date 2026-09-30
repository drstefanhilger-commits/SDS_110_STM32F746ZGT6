# Vergleich HBD ↔ ML in Modul 124 (Arbeitspaket 7)

Stand 27.09.2026. Umsetzung von Abschnitt 8 des Trainingskonzepts
(ML_Test `docs/Trainingskonzept_ML124.md`): Das Modell `k5_h48_d3` (ML_Test
`model/ml124/k5_h48_d3`, Export AP5) läuft in Modul 124 und wird mit den Host-Messungen gegen den
HBD verglichen.

**Ergebnis:** Das Modell erfüllt die Abnahme **nicht**. Es selektiert auf den Simulator-Drohnen
überwiegend Bänder ohne Drohnenenergie, die Peilung fällt aus. 124 bleibt deshalb in der Stufe
**Schatten**: s(t) kommt weiter vom HBD, das MLP rechnet mit, und der Abgleich steht auf dem LCD.

**Nachmessung 28.09.2026** (200-mm-Array, Merkmalsversion `3a685d01f22f3eea`, Modell neu exportiert, Gewichte unverändert): Mit `ML124_MODE = Ml` bleibt die Peilung unbrauchbar. `m_bearing_drone` bei 30 dB: 37 % gültig, 9,8 Paare, Peak-Ratio-Median 1,05, Median-Fehler 94°. Bei 20 dB: 78 % gültig, Median-Fehler 94°. FSL9 verlangt ML in 124; dafür muss das Modell neu trainiert werden (Befund 38, AP 2). 124 bleibt bis zur bestandenen Abnahme in der Stufe Schatten.

---

## 1. Stufen und Aufruf

| Stufe | `Ml124Mode` | s(t) aus | MLP läuft | Zweck |
|---|---|---|---|---|
| 0 | `Hbd` | HBD | nein | bisheriges Verhalten |
| 1 | `Shadow` | HBD | ja | Abgleich HBD ↔ ML im Betrieb, ohne Wirkung auf 126/128 (**Standard**) |
| 2 | `Ml` | MLP (mit HBD-Gate und Glättung) | ja | Einsatz nach bestandener Abnahme |

Einstellung: `ML124_MODE` in
`Core/SDS_110/Processing_Module_120/Machine_Learning_Module_124/ML124_Config.hpp`. Die Datei
geht nicht in die Merkmalsversion ein, ein Umschalten erzwingt also kein Neutraining. Zur Laufzeit
schaltet `Machine_Learning_Module_124::setMode()`.

LCD (DETECT, CALIBRATE): Zeile „124 Stufe“, in der Stufe Schatten zusätzlich
`ML Det xx% Bx.xx dpx.xx`, jeweils seit dem Start:

- `Det`: Anteil der Frames mit gleicher Detektionsentscheidung (≥ `B_MIN` Bänder über θ_sel).
- `B`: Jaccard-Index der selektierten Bänder.
- `dp`: mittleres |p_HBD − p_ML|.

Host-Messungen mit wählbarer Stufe:

```bash
make -C test/host
ML124_MODE=0 build/test_host/m_selection      # HBD
ML124_MODE=2 build/test_host/m_selection      # ML
ML124_MODE=2 build/test_host/m_selection 650 noise
ML124_MODE=2 build/test_host/m_bearing_drone
ML124_MODE=2 build/test_host/m_confidence
```

---

## 2. Abnahme (Konzept Abschnitt 8)

| Kriterium | Messung | Vorgabe | HBD | ML | erfüllt |
|---|---|---|---|---|---|
| Reports Drohne 3 dB / 0 dB | `m_selection` | ≥ HBD | 100 % / 56 % | 39 % / 5 % | nein |
| Fehlalarm-Reports Wind, Stille, Einzelton | `m_selection`, `… 650 noise` | 0 % | 0 % | 0 % | ja |
| Anteil selektierter Bänder mit Harmonischer | `m_selection` (20 dB) | ≥ 85 % | 87 % | 0 % | nein |
| Peilfehler Median / 95 % bei 10 dB | `m_bearing_drone` | ≤ HBD | 0,88° / 3,1° | 1,7° / 9,9° bei 4 % gültig | nein |
| C++ ↔ Python | `t_ml124` | ≤ 1e-5 | – | 5,4·10⁻⁷ | ja |
| Flash | Linker | ≤ 200 kB | – | +197 KiB (Modell 193,7 kB) | ja |
| Rechenzeit je Frame auf dem Board | Task-Statistik | ≤ 2 ms | – | nicht gemessen (ProcessingTask auf dem Board noch aus) | offen |
| Band-Güte auf Hold-out | Python (AP4) | – | AUC 0,54 | AUC 0,90 / F1 0,87 | (siehe 3.2) |

### 2.1 `m_selection` (6 Richtungen, ab 3,2 s)

| Szenario | detected HBD / ML | Bänder HBD / ML | davon Harmonische HBD / ML | Peilung gültig HBD / ML | Report HBD / ML |
|---|---|---|---|---|---|
| DroneSweep 20 dB | 100 / 98 % | 8,0 / 29,2 | 87 / 0 % | 100 / 0 % | 100 / 0 % |
| DroneStatic 20 dB | 100 / 97 % | 8,0 / 28,5 | 87 / 0 % | 100 / 0 % | 100 / 0 % |
| DroneStatic 10 dB | 100 / 100 % | 8,0 / 64,0 | 87 / 17 % | 100 / 12 % | 100 / 12 % |
| DroneStatic 3 dB | 100 / 61 % | 5,0 / 19,2 | 94 / 26 % | 100 / 76 % | 100 / 39 % |
| DroneStatic 0 dB | 57 / 5 % | 2,8 / 0,5 | 99 / 34 % | 95 / 99 % | 56 / 5 % |
| Einzelton 20 dB | 1 / 0 % | 0,1 / 0,0 | – | 18 / 96 % | 0 / 0 % |
| Wind 20 dB | 0 / 0 % | 0 / 0 | – | 85 / 1 % | 0 / 0 % |
| Stille 20 dB | 0 / 0 % | 0 / 0 | – | 35 / 0 % | 0 / 0 % |

Langlauf `m_selection 650 noise`: 0 Reports in beiden Stufen.

### 2.2 `m_bearing_drone` (DroneStatic, 12 Richtungen)

| SNR | gültig HBD / ML | Paare HBD / ML | Fehler Median HBD / ML | Fehler 95 % HBD / ML |
|---|---|---|---|---|
| 30 dB | 100 / 0 % | 28,0 / 0,0 | 0,19° / – | 0,67° / – |
| 20 dB | 100 / 0 % | 28,0 / 0,7 | 0,43° / – | 1,28° / – |
| 10 dB | 100 / 4 % | 27,3 / 4,1 | 0,88° / 1,70° | 3,14° / 9,94° |
| 6 dB | 100 / 15 % | 27,2 / 8,4 | 1,17° / 14,65° | 4,17° / 153,8° |
| 3 dB | 99 / 82 % | 26,8 / 17,6 | 1,55° / 13,75° | 5,31° / 138,8° |
| 0 dB | 90 / 98 % | 23,7 / 19,7 | 2,21° / 20,70° | 7,52° / 122,1° |

`m_confidence`: Konfidenz-Median der Drohne bei 10 dB 0,68 (HBD) gegenüber 0,25 (ML), ab 3 dB
0,02 (ML).

---

## 3. Ursache

### 3.1 Befund am Simulator (DroneStatic 20 dB, f0 180 Hz, 8 Harmonische)

Mittlere MLP-Ausgabe (ungeglättet, vor dem Gate), Frames 100–199:

- Bänder mit Harmonischer (1, 4, 7, 10, 13, 16, 18, 21): p ≈ 0,20–0,38.
- Bänder oberhalb ca. 1,4 kHz, ohne Drohnenenergie: p ≈ 0,50–0,60.

Die Eingänge liegen nicht extrem außerhalb der Trainingsverteilung. Die mittleren z-Werte liegen
zwischen −2,4 und +1,1, in den hohen Bändern um −1,2 (leiser als im Training). Die Werte stimmen
mit der Python-Rechnung überein (`t_ml124`). Es handelt sich also nicht um einen
Integrationsfehler, sondern um das gelernte Verhalten.

### 3.2 Erklärung

- **Unterschiedliche Drohnenmodelle.** Die Trainingsdrohnen (`gen_dronen_sim.py`) haben
  BPF-Harmonische bis 8 kHz und breitbandiges Rotorrauschen, dazu Motor- und Wellenharmonische. Die
  Simulator-Drohne von SDS_110 (`Signal_Simulator`) hat 8 Harmonische bis ca. 1,4 kHz und weißes
  Rauschen. Das Modell hat gelernt, dass Drohnen breitbandig sind und die hohen Bänder dominieren.
  Mit dieser Erwartung markiert es genau die Bänder, in denen die Simulator-Drohne nichts hat. Die
  schmalen Harmonischen in den unteren Bändern erkennt es nicht, dort überwiegen im Training die
  Umweltgeräusche.
- **Breitband-Selektion schadet der Peilung.** Das Label σ(SNR_b / 3 dB) markiert auch die
  breitbandige Drohnenenergie (Training_ML124 § 4, Punkt 3). Bänder ohne kohärente Phase machen die
  quellkonditionierte GCC-PHAT unbrauchbar: Ratio-Median 1,0–1,2 statt ≈ 15.
- **Die Band-Güte in AP4 bewertet etwas anderes.** AUC und F1 aus AP4 messen die Übereinstimmung
  mit dem Label auf Gemischen aus derselben Synthese. Sie sagen nichts darüber, ob die selektierten
  Bänder peilbar sind, und nichts über Drohnen außerhalb dieses Modells.

---

## 4. Entscheidung und nächste Schritte

1. **Stufe Schatten bleibt Standard.** Die Stufe ML wird nicht aktiviert, bis ein Modell die
   Abnahme in Abschnitt 2 erfüllt. Die Stufe Schatten ändert s(t) nicht; `m_selection` liefert
   bitgenau die HBD-Referenz, `t_ml124` prüft das.
2. **Label auf tonale Anteile beschränken** (Training_ML124 § 4, Punkt 3), z. B. nur Bänder mit
   BPF-, Wellen- oder Motorharmonischen über dem Rest, oder Band-SNR nur der tonalen Komponente.
   Damit wird das Ziel dasselbe wie beim HBD (peilbare Bänder).
3. **Trainingsdrohnen verbreitern:** Drohnen mit wenigen Harmonischen und ohne Breitbandanteil,
   wie der Simulator, in `gen_dronen_sim.py` aufnehmen; BPF bis ca. 700 Hz (Befund 38).
4. **Abnahme an einer realistischen Drohne im Simulator:** `Signal_Simulator` um das
   Drohnenmodell aus `gen_dronen_sim` bzw. `SDS_SimDrone` erweitern (Konzept 5.2), damit AP7 nicht
   nur am Simulator-Kamm misst. Die Simulator-Drohne allein würde den HBD begünstigen.
5. Danach neu trainieren (AP4), exportieren (AP5, `export.py`), Header übernehmen, `t_ml124` und
   diese Messungen wiederholen.
6. **Board:** Rechenzeit in der Stufe Schatten messen (Task-Statistik „120“), sobald der
   ProcessingTask auf dem Board läuft (`main.c`, Blocker 1–4).
