# ADAU7118 + STM32F746NG  
## Designentscheidungen, Architektur und technische Begründungen

Dieses Dokument beschreibt die vollständige, zusammenhängende Designarchitektur für die Integration des ADAU7118 (8‑Kanal PDM‑→ PCM Wandler) mit einem STM32F746NG auf einem eigenen PCB. Der Fokus liegt auf deterministischem Verhalten, stabiler DMA‑Verarbeitung, klarer TDM‑Struktur und direkter Kompatibilität mit einer SRP‑PHAT‑Pipeline. Alle Entscheidungen sind so getroffen, dass sie Pfad‑B‑konform, reproduzierbar und ohne versteckte Zustände implementierbar sind.

---

## 1. Zielsetzung

Das System soll acht Mikrofone über den ADAU7118 erfassen, die Daten als PCM über ein TDM‑Interface an den STM32F746 übertragen und dort deterministisch weiterverarbeiten. Die Architektur muss stabil, latenzarm und für Echtzeit‑Signalverarbeitung geeignet sein. Der STM32F746 übernimmt die Rolle des TDM‑Slaves, verarbeitet die Daten über DMA‑Double‑Buffering und stellt sie der SRP‑PHAT‑Pipeline bereit.

---

## 2. Auswahl des ADAU7118

Der ADAU7118 ist ein kompakter 8‑Kanal PDM‑→ PCM‑Wandler, der vollständig über I²C konfiguriert wird. Er unterstützt TDM‑8 und I²S‑Stereo, liefert 24‑Bit‑PCM‑Samples und besitzt zwei PDM‑Clock‑Domains, die flexibel konfigurierbar sind. Die Registerstruktur ist klar dokumentiert und kompatibel mit dem Linux‑Treiber, wodurch eine stabile und minimalistische Initialisierung möglich ist.

---

## 3. Auswahl des STM32F746NG

Der STM32F746NG ist ideal für dieses Design, da er I²S‑Extended‑Mode unterstützt, der TDM‑8 und TDM‑16 verarbeiten kann. Seine DMA‑Engine arbeitet nativ mit 32‑Bit‑Transfers, was perfekt zu den 32‑Bit‑Slots des ADAU7118 passt. Mit 216 MHz Core‑Takt und einer leistungsfähigen Busmatrix bietet der F746 ausreichend Rechenleistung für SRP‑PHAT und andere DSP‑Algorithmen.

---

## 4. Audio‑Interface‑Design

### TDM‑8 Frame Layout

Der ADAU7118 liefert acht PCM‑Kanäle in einem TDM‑Frame. Jeder Frame besteht aus acht Slots, die jeweils 32 Bit breit sind. Die Slot‑Zuordnung ist linear:

- Slot 0 → Kanal 0  
- Slot 1 → Kanal 1  
- Slot 2 → Kanal 2  
- Slot 3 → Kanal 3  
- Slot 4 → Kanal 4  
- Slot 5 → Kanal 5  
- Slot 6 → Kanal 6  
- Slot 7 → Kanal 7  

### Slot‑Breite: 32 Bit

Die Entscheidung für 32‑Bit‑Slots basiert auf mehreren technischen Vorteilen:

- Der STM32F746 DMA arbeitet nativ mit 32‑Bit‑Words.  
- Keine Bit‑Shifts über Frame‑Grenzen.  
- Perfekte Alignment‑Grenzen für DMA‑Double‑Buffering.  
- Die 24‑Bit‑PCM‑Daten des ADAU7118 lassen sich durch einen einfachen Right‑Shift extrahieren.  
- Ein TDM‑Frame umfasst exakt 256 Bit, was die Clock‑Berechnung vereinfacht.

---

## 5. Clocking‑Design

Der ADAU7118 erzeugt die Audio‑Clocks, während der STM32F746 als Slave arbeitet. Dies reduziert die Komplexität des PCB‑Layouts und vermeidet PLL‑Synchronisationsprobleme.

- **FSYNC:** 48 kHz  
- **BCLK:** 12.288 MHz (48 kHz × 256 Bit)  

Diese Clock‑Topologie ist stabil, kompatibel und minimiert Jitter.

---

## 6. DMA‑Design

Das DMA‑Design basiert auf einem Double‑Buffer‑Ansatz, der zwei TDM‑Frames hintereinander speichert. Jeder Frame besteht aus acht 32‑Bit‑Wörtern.

Der Ablauf ist deterministisch:

1. DMA füllt Frame 0 → Interrupt „Half Complete“  
2. DMA füllt Frame 1 → Interrupt „Complete“  
3. HFSM verarbeitet die Frames ohne Race Conditions  
4. SRP‑PHAT erhält konstante Datenraten  

Dieser Ansatz garantiert Echtzeitfähigkeit und verhindert Datenverlust.

---

## 7. I²C‑Register‑Design

Die Initialisierung des ADAU7118 basiert auf dem offiziellen Linux‑Treiber. Die minimalistische Konfiguration ist bewusst gewählt, um deterministisches Verhalten sicherzustellen.

| Register | Wert | Bedeutung |
|---------|------|-----------|
| 0x00 | 0x01 | Power‑Up |
| 0x01 | 0x10 | Decimation Ratio 16 |
| 0x02 | 0xB4 | PDM Clock Map |
| 0x03 | 0x0F | Enable Inputs 0–7 |
| 0x04 | 0x02 | TDM Mode, 32‑Bit Slots |

Diese Werte sind stabil, kompatibel und für UAV‑Audio‑Frontends optimiert.

---

## 8. Datenpfad‑Design

Der ADAU7118 liefert 24‑Bit‑PCM‑Samples innerhalb eines 32‑Bit‑Slots. Die oberen acht Bits sind Padding. Die Extraktion erfolgt durch: pcm24 = raw >> 8


Dies ist effizient, vermeidet Maskierung und ist kompatibel mit:

- Floating‑Point‑SRP‑PHAT  
- Fixed‑Point‑Q1.23‑SRP‑PHAT  
- Ringbuffer‑Designs  
- Echtzeit‑DSP‑Pipelines  

---

## 9. HFSM‑Integration

Die Startsequenz ist bewusst minimal gehalten:

1. ADAU7118_Init()  
2. HAL_Delay(10)  
3. ADAU7118_Start()  

Diese Reihenfolge garantiert:

- stabile I²C‑Initialisierung  
- keine Race Conditions zwischen I²C und DMA  
- deterministische HFSM‑Startzustände  

---

## 10. PCB‑Designentscheidungen

### TDM‑Leitungen

- BCLK → STM32 I²S_CK  
- FSYNC → STM32 I²S_WS  
- SDATA → STM32 I²S_SD  
- GND → gemeinsame Masse  

### PDM‑Clock‑Routing

Der ADAU7118 besitzt zwei PDM‑Clock‑Domains. Die Zuordnung erfolgt über das Register `adi,pdm-clk-map`. Dies ermöglicht flexible Mikrofon‑Layouts.

### Power Rails

| Rail | Spannung |
|------|----------|
| DVDD | 1.1–1.98 V |
| IOVDD | 1.7–3.63 V |

Diese Spannungen sind stabil und kompatibel mit typischen UAV‑Power‑Designs.

---

## 11. Zusammenfassung der wichtigsten Entscheidungen

- TDM‑8 als Audio‑Interface  
- 32‑Bit‑Slots für perfekte DMA‑Kompatibilität  
- STM32F746 als Slave für stabile Clocking‑Topologie  
- Double‑Buffer‑DMA für deterministische Echtzeitverarbeitung  
- Minimaler I²C‑Init‑Pfad basierend auf Linux‑Treiber  
- 24‑Bit‑PCM‑Extraktion durch Right‑Shift  
- Pfad‑B‑konforme Treiberstruktur  
- SRP‑PHAT‑kompatible Datenpfade  
- PCB‑Layout optimiert für UAV‑Audio‑Frontends  

---

## 12. Weiterführende Schritte

Optional können folgende Module ergänzt werden:

- SDS_MicrophoneBuffer (8‑Kanal Ringbuffer, lock‑free)  
- SRP‑PHAT‑Adapter (float oder Q1.23)  
- Vollständige STM32CubeIDE‑Projektstruktur  
- HFSM‑Audio‑Init‑State  

Diese Module können auf Wunsch ebenfalls als vollständige Pfad‑B‑Dateien generiert werden.

