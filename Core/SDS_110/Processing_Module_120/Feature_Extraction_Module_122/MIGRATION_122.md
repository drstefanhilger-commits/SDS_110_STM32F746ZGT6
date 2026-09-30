# Migration 118 + 122

| Neu                                   | Aus SDS                                   | Änderung |
|---------------------------------------|-------------------------------------------|----------|
| Pre_Processor_118.{hpp,cpp}           | SDS_Params (SDS_BANDPASS_LOW/HIGH)        | neu: Biquad-Bandpass 80–8000 Hz, NS, AGC |
| Feature_Extraction_Module_122.{hpp,cpp} | ML/FFTProcessor, MelFilterbank, MelSpectrogram, SDS_Data::computeMelFeatures | 256 → 4096 Punkte; Mel-Tabelle → Laufzeit |

## Speicher (statisch, je Instanz)
Stand 25.09.2026: `MEL_MAX_BINS_PER_BAND` = 128 (oberstes Mel-Band bei 8 kHz ist ~110 Bins breit) → 122 ≈ 86 kB
(gemessen mit `sizeof`); die Instanz liegt als Mitglied von Processing_Module_120 im SDRAM.
Ursprünglich: window 12 kB, buf+fftOut 32 kB, mag+prevMag 16 kB, Mel sparse 40×512×4 = 80 kB, AM 4 kB → ~145 kB.
- 118: < 1 kB.

## Laufzeit (F746 @ 216 MHz, grob)
- 4096-Punkt-RFFT ≈ 0,25 ms; Bandpass 8 × 3072 Samples × 2 Biquads ≈ 0,5 ms; Mel/Flux/AM < 0,2 ms.
- Bei 30 Frames/s (Hop 32 ms) ist 122 für den Referenzkanal unkritisch. 8 Kanäle FFT (für 126) ≈ 2 ms.

## Offene Punkte
Stand 25.09.2026 – Details und Commits in `doc/Analyse_Befunde.md`.
1. **CubeAI-Modell**: trainiert auf 40 Log-Mel aus 256-Punkt-FFT/48 kHz – mit der neuen Filterbank nicht mehr identisch. Neutraining nötig (ohnehin für B=64 Sigmoid-Ausgänge, Abschnitt 3).
   *Stand: Neues Modell `HBD_ML_Model_Data.hpp` (171 Merkmale → 64 Ausgänge) liegt vor, ist aber nicht eingebunden
   (Befund 20). Es braucht zusätzlich `cep_f0_hz/350` und `cep_peak`, die 122 nicht berechnet; Aktivierungen und
   Merkmalsdefinitionen aus dem Trainingsskript (`export_model.py`) fehlen im Repo.*
2. `band_am_depth` ist Modulationstiefe (std/mean), nicht das volle AM-Spektrum. Falls das Modell das Spektrum braucht: 16-Punkt-RFFT über amHist_ je Band ergänzen.
3. ~~Frame-Overlap 50 %~~ – *erledigt (Befund 17)*: 114 liefert Hops (1536), 118 verarbeitet jeden Hop einmal,
   `Frame_Assembler` setzt je Hop einen Frame aus den letzten 2 Hops zusammen (31,25 Frames/s).
4. NS arbeitet frameweise im Zeitbereich (Breitband-Gain). Eine spektrale Variante gehört in 122 vor die Bandleistung.
