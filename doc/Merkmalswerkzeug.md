# Merkmalswerkzeug `sds_features`

Arbeitspaket 1 des Trainingskonzepts für Modul 124 (`ML_Test/docs/Trainingskonzept_ML124.md`,
Abschnitt 4). Berechnet die Merkmale je Analyse-Frame aus einer WAV-Datei mit dem **unveränderten
Board-Code** – Training und Board verwenden damit garantiert dieselben Merkmale.

---

## 1. Bauen und prüfen

```bash
make -C tools/features           # -> build/tools/sds_features
make -C tools/features check     # Prüfungen (Abschnitt 5)
make -C tools/features version   # Merkmalsversion ausgeben
```

Voraussetzung: `g++` mit C++17 (Linux/WSL). Übersetzt werden die Quellen aus `Core/SDS_110`
gegen die Stubs aus `test/host/shim` (CMSIS-DSP, CMSIS-RTOS2), FFT über `Core/Lib/kiss_fft`.

---

## 2. Aufruf

```bash
build/tools/sds_features [--hbd] [--channel N] <eingabe.wav> <ausgabe-präfix>
build/tools/sds_features [--hbd] --label <drohne.wav> <umwelt.wav> <ausgabe-präfix>
```

| Option | Bedeutung |
|---|---|
| `--hbd` | zusätzlich den heutigen HBD-Zustand und dessen s(t) ausgeben (Vergleichsgrundlage) |
| `--channel N` | bei mehrkanaligen Dateien (≠ 8 Kanäle) Kanal N als Referenzmikrofon (Standard 0) |
| `--label` | Label-Modus (Arbeitspaket 3): zwei Anteile mischen, zusätzlich Band-SNR ausgeben (Abschnitt 4a) |

**Eingabe**: WAV mit **48 kHz** (andere Raten werden abgelehnt – vorher umtasten), PCM 16/24/32 Bit
oder float32, auch WAVE_FORMAT_EXTENSIBLE. Mono bzw. Kanal N wird als Referenzmikrofon
(`REF_MIC`) eingespeist, die übrigen Mikrofone mit 0 – die Merkmale von 122 hängen nur vom
Referenzkanal ab. Eine Datei mit genau 8 Kanälen (z. B. spätere Board-Aufnahme) wird vollständig
eingespeist.

**Exit-Code**: 0 ok, 1 Aufruf, 2 Eingabe (Format, Abtastrate), 3 Ausgabe.

**Eine Datei je Aufruf**: Jeder Prozess beginnt wie ein Board-Start (AGC, Rauschboden, Filter,
Puffer, HBD-Anlaufzeit). So beeinflussen sich Dateien nicht gegenseitig; das Werkzeug braucht
dafür keine Änderung am Board-Code. Laufzeit ca. 0,03–0,07 s je Datei (5–10 s Audio).

---

## 3. Verarbeitung (wie auf dem Board)

1. WAV-Samples → 24-bit-PCM, **linksbündig im 32-bit-Slot** (Format von ADAU7118/SAI, Befund 6);
   16 Bit werden exakt erweitert, float wird auf [−1, 1) begrenzt und auf 24 Bit gerundet.
2. `Microphone_Array_114::pushBlock()` in DMA-Blöcken zu `DMA_BLOCK_SAMPLES` (128) →
   Hops zu `HOP_SAMPLES` (1536, 32 ms).
3. Je Hop `Pre_Processor_118::process()` (Bandpass 80–8000 Hz, NS, AGC als Rampe), dann
   `Frame_Assembler::push()` → Analyse-Frames zu 3072 Samples (64 ms, 50 % Überlappung).
4. Je Frame `Feature_Extraction_Module_122::process()` (Hann, 4096er FFT, Referenzkanal).
5. Optional `Machine_Learning_Module_124::infer()` (HBD → s(t)).

Der erste Frame entsteht nach zwei Hops; ein unvollständiger letzter DMA-Block bzw. Hop entfällt.

---

## 4. Ausgabe

**`<präfix>.npy`** – float32, Form `[Frames, Spalten]`, eine Zeile je Analyse-Frame (31,25 je s):

| Spalten | Anzahl | Inhalt |
|---|---|---|
| `t_s` | 1 | Zeit des **ersten** Samples des Frames in s (Frame dauert 64 ms) |
| `band_log_power_00…63` | 64 | ln(Σ \|X\|²) je Band (62,5 Hz ab 80 Hz), Referenzkanal nach 118 |
| `mel_00…39` | 40 | Log-Mel (80–8000 Hz) |
| `spectral_flux` | 1 | positive Betragsänderung zum Vorframe, normiert |
| `band_am_depth_00…63` | 64 | Modulationstiefe je Band (std/mean über 16 Frames ≈ 0,5 s) |
| mit `--hbd`: `hbd_f0_hz`, `hbd_score`, `hbd_snr_db`, `hbd_consistent`, `hbd_detected` | 5 | HBD-Zustand |
| mit `--hbd`: `hbd_s_p_00…63` | 64 | s(t) des heutigen HBD (inkl. Glättung und Gate) |

Ohne `--hbd` 170 Spalten, mit `--hbd` 239.

**`<präfix>.json`** – Metadaten: `feature_version`, `sds110_git`, Werkzeugversion, Eingabedatei
(Rate, Kanäle, Bits), Kanalzuordnung, Sample- und Frame-Zahl, Parameter der Kette (Frame, Hop,
N_FFT, Bänder, Mel, Bandpass, AM-Historie) und die Spaltennamen in Reihenfolge.

Lesen in Python:
```python
import json, numpy as np
X = np.load("clip.npy"); meta = json.load(open("clip.json"))
feat = X[:, 1:170]; t = X[:, 0]; cols = meta["columns"]
```

---

## 4a. Label-Modus (`--label`)

Für die Band-SNR-Labels (Trainingskonzept 5.1) nimmt das Werkzeug Drohnen- und Umweltanteil
getrennt entgegen (je mono bzw. Kanal 0, 48 kHz, gleiche Länge; sonst gilt die kürzere):

1. **Gemisch**: Beide Anteile werden nach der Wandlung als 24-bit-Werte addiert. Samples außerhalb
   des 24-bit-Bereichs werden begrenzt und gezählt (`clipped_samples`). Das Gemisch läuft
   unverändert durch die Board-Kette; die **Merkmale** stammen daraus und sind bitgleich mit dem
   normalen Modus auf dem Gemisch.
2. **Anteile**: Jeder Anteil läuft durch einen eigenen `Pre_Processor_118`, nur mit Bandpass
   (NS und AGC aus, also linear). Darauf wird je Hop exakt die Verstärkungsrampe angewendet, die
   118 im Gemisch berechnet hat: von `frameCenterGain()` (g0) bis `appliedGain()` (g1). Danach
   folgen je Anteil ein eigener `Frame_Assembler` und eine eigene 122.
3. **Labels**: `snr_db_b` = 10·log10(P_b(Drohne) / P_b(Umwelt)), berechnet aus den
   `band_log_power`-Werten beider Anteile und auf ±80 dB begrenzt.

Zusätzliche Spalten: `snr_db_00…63` (64) am Ende, also 234 bzw. mit `--hbd` 303 Spalten.
Zusätzliche JSON-Felder:

| Feld | Inhalt |
|---|---|
| `mode` | `"label"` |
| `input_drone`, `input_noise` | Eingabedateien |
| `clipped_samples` | Anzahl der begrenzten Samples |
| `max_reconstruction_error` | Selbstkontrolle: größter Fehler \|Gemisch − (Drohne + Umwelt)\| nach 118 |
| `max_abs_mixture_after_118` | Bezugsgröße für diesen Fehler |

Der Rekonstruktionsfehler ist ohne Bandpass exakt 0. Mit Bandpass entsteht er aus der Rundung der
float32-Biquads (80-Hz-Hochpass); typisch sind etwa 10⁻⁴ relativ.

---

## 5. Merkmalsversion

`feature_version` = erste 16 Hex-Zeichen des SHA-256 über den Quelltext von
`SDS_110_Config.hpp`, `Microphone_Array_114`, `Pre_Processor_118`, `Frame_Assembler` und
`Feature_Extraction_Module_122` (Liste `FEATURE_SRCS` im Makefile). Jede Änderung an der
Merkmalskette ergibt eine neue Version; Änderungen an anderen Modulen nicht. Das Training hält die
Version fest, der Modell-Export übernimmt sie (Trainingskonzept, Abschnitte 4 und 7); passen
Firmware und Modell nicht zusammen, ist ein Neutraining nötig. `sds110_git` ist zusätzlich der
git-Stand (`git describe --always --dirty`).

Stand 26.09.2026: `837ff89cbda34b21`.

---

## 6. Prüfungen (`make -C tools/features check`)

| Prüfung | Kriterium | Ergebnis |
|---|---|---|
| Gleichheit mit der Board-Kette | Simulator (DroneStatic, 10 dB, 6,4 s) im Prozess durch 114→118→Frame_Assembler→122; Rohwerte des Referenzkanals als 24-bit-WAV → `sds_features` muss **bitgleiche** Merkmale und Zeitstempel liefern | 199 Frames × 170 Spalten, 0 Abweichungen |
| Plausibilität | 1-kHz-Sinus → Maximum von `band_log_power` in Band 14 | Band 14 |
| Reproduzierbarkeit | zweiter Lauf auf dieselbe WAV → identische `.npy` | identisch |
| Label-Modus: Merkmale | Gemisch = Board-WAV + 0; Merkmale bitgleich mit dem normalen Modus | 199 Frames, 0 Abweichungen |
| Label-Modus: Rekonstruktion | \|Gemisch − (Drohne + Umwelt)\| nach 118 < 10⁻³ · max\|Gemisch\| | 9,2·10⁻⁶ bei 0,136 (6,8·10⁻⁵ relativ) |
| Label-Modus: Symmetrie | identische Anteile → SNR überall 0 dB | 0 Abweichungen |
| Label-Modus: Trennung | 1 kHz als Drohne, 2 kHz als Umwelt | Band 14 +80 dB, Band 30 −80 dB |

Zusätzlich auf echten Daten (`ML_Test/data/48kHz`) geprüft: synthetische Drohne, echte
DDS-Drohne, ESC-50 (5 s) und DDS-Hubschrauber – 155 bzw. 311 Frames, Abstand exakt 32 ms,
keine NaN, Spaltenzahl = JSON.

---

## 7. Grenzen

- Nur Merkmale des Referenzkanals (so wie 122 sie liefert); für mehrkanalige Auswertungen (126)
  ist das Werkzeug nicht gedacht.
- Die ersten Frames einer Datei enthalten das Einschwingen von AGC, Rauschboden und AM-Historie,
  mit `--hbd` zusätzlich die HBD-Anlaufzeit (3 s). Im Training entweder verwerfen oder Dateien
  am Stück (z. B. aneinandergehängt) verarbeiten.
- Abtastrate fest 48 kHz (Board: 47 991 Hz, Befund 5; Abweichung 186 ppm vernachlässigbar).
