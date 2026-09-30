# Harness – Test ohne Mikrofone

## Signal_Simulator (On-Target)
Erzeugt synthetische 8-Kanal-Signale und speist sie über `Microphone_Array_114::pushBlock()`
ein (gleicher Pfad und gleiches Rohformat wie der SAI-DMA: 24 Bit linksbündig im 32-Bit-Slot).
Aktiv, wenn `SDS_Data::simulation != 0` (Standard 1; USB-Kommando Typ 3 schaltet um und wählt das
Szenario: 1 = `SIM_SCENARIO_ID`, 2 + k = Szenario k, siehe `SimScenario.hpp`).
Der ProcessingTask erzeugt pro Durchlauf einen Hop (`generateHop()`, 1536 Samples = 32 ms);
jedes Quellsample wird genau einmal erzeugt (phasenkontinuierlich über die Aufrufe).

Standardszenario und Grundwerte: `SIM_SCENARIO_ID`, `SIM_F0_HZ`, `SIM_SNR_DB` in SDS_110_Config.hpp.
Feinere Parameter: `Signal_Simulator::instance().params()` (Azimut, Distanz, Harmonische,
Blattpass-AM, Pegel, Sweep-Schritte).

| Szenario (USB Id 3 = 2 + Nr.) | Erwartung im LCD (DETECT / READ) |
|---|---|
| 0 DroneSweep  | HBD `DRONE`, f0 ≈ SIM_F0_HZ, Azimut folgt "True Azimuth" (±3° grün), s(t)-Balken an den Harmonischen |
| 1 DroneStatic | wie oben, fest – zum Abstimmen von HBD_BAND_SNR_DB / perBandSnrDb / THETA_SEL |
| 2 SingleTone  | f0 gefunden, aber Konsistenz < 5 Harmonische -> kein `DRONE`; kaum selektierte Bänder |
| 3 WindNoise   | f0 springt am 80-Hz-Rand (Schwäche des Pegel-f0-Schätzers), Score klein, kein `DRONE` |
| 4 Silence     | nur Mikrofonrauschen; Noise-Floor folgt dem Rauschpegel (dBFS), kein `DRONE`, p_b klein |
| 5 FlyBy       | gerader Überflug 5 s (15 m/s, Bahn um den lokalen Ursprung mit kürzestem Abstand 30 m, SNR bei 30 m SIM_SNR_DB; Azimut/Distanz von der Position der Einheit, USB Id 10), dann 5 s nur Rauschen; wiederholt, Kurs je Durchgang +45°. Im Flug `DRONE` und Azimut folgt "True Azimuth", in der Pause "True Azimuth Pause" und kein `DRONE` |

Parameter-Sweeps: SIM_SNR_DB von 30 dB abwärts senken, bis `DRONE` ausfällt -> Empfindlichkeit;
SIM_F0_HZ über 80..350 Hz -> Bandabdeckung der Harmonischen bis 4 kHz (h ≤ 8 bei f0 ≤ 500 Hz).

## Host-Harness (WSL, geplant)
122/124/126/128 sind portables C++ (CMSIS-DSP-Quellen). Mit demselben Generator oder WAV-Dateien
lassen sich Parameter auf dem PC in Schleifen durchtesten. Noch nicht angelegt.
