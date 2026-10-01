# ESP32-C3-Flasher (STM32F746ZGT6)

Kleine eigene STM32-Firmware, die die Brücken-Firmware (`tools/esp32c3_bridge`) in den
ESP32-C3-MINI-1-N4 schreibt, **nur mit dem ST-LINK**. Ein USB-UART-Adapter ist nicht nötig.
Hintergrund: USB funktioniert auf dem Board nicht, die USB-Pins IO18/IO19 des ESP sind nicht
beschaltet, und UART0 des ESP hängt an USART3 des STM32.

```
ST-LINK ──SWD──► STM32 (Flasher + ESP-Images im Flash) ──USART3──► ESP32-C3 ROM-Bootloader
                    PC13 ──R40──► EN          SW3 (BOOT, IO9) von Hand
```

Die Images (Bootloader, Partitionstabelle, otadata, Anwendung) werden beim Bauen aus dem
Build der Brücke eingebettet (`mkimages.py`, etwa 820 kB). Der Flasher spricht das serielle
Protokoll des ESP-ROM wie esptool (`src/EspRomLoader.hpp`):

- SYNC, SPI_ATTACH, Flash-Parameter 4 MB
- Wechsel auf 460800 Baud
- je Image FLASH_BEGIN (Löschen), FLASH_DATA in Blöcken zu 1 kB, danach Prüfung per MD5 im ESP-Flash

## Bauen

```bash
cd tools/esp32c3_bridge && idf.py set-target esp32c3 && idf.py build   # erst die Brücke
cd ../esp32c3_flasher && make                                          # -> build/esp32c3_flasher.elf/.hex
make test                                                              # Protokoll-Test auf dem PC
```

`make` braucht `arm-none-eabi-gcc` (wie `wsl/Makefile`, z. B. `make TOOLCHAIN=~/opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi`)
und Python 3. Ein anderer Build der Brücke geht mit `make ESP_BUILD=/pfad/zum/build`.

## Ablauf am Board

1. `build/esp32c3_flasher.elf` (oder `.hex`) mit dem ST-LINK aufspielen, z. B. mit
   STM32CubeProgrammer. Das **ersetzt die SDS-Firmware** vorübergehend.
2. **SW3 (BOOT) gedrückt halten.** Der Flasher setzt den ESP über EN zurück, bis er im
   Download-Modus antwortet. Solange blinkt **RUN**.
3. Leuchtet **COMM**, ist der ESP verbunden: **SW3 loslassen.**
4. Der Flasher schreibt die Images, **COMM blinkt**. Das dauert etwa 30 s.
5. Danach startet er den ESP neu und liest dessen Boot-Meldung mit:
   - **RUN leuchtet:** fertig, der ESP startet die Brücke. Das WLAN `SDS110-xxxx` erscheint.
   - **ERROR blinkt:** Der ESP steht noch im Download-Modus. SW3 loslassen.
   - **ERROR leuchtet:** Fehler. Die Meldung kommt über SWO. Für einen neuen Versuch den STM32 zurücksetzen und SW3 halten.
6. Wieder die SDS-Firmware aufspielen (Projekt im Hauptordner, `SDS110_PC_UART 3`).

Die Meldungen gehen über **SWO** (ITM-Port 0, PB3) mit dem Kerntakt **16 MHz** (HSI). In
STM32CubeIDE: Debug-Konfiguration → Serial Wire Viewer mit 16 MHz Core Clock, Konsole „SWV ITM
Data“, Port 0. Ohne SWO reichen die LEDs.

| LED | Bedeutung |
| --- | --- |
| RUN blinkt | wartet auf den Download-Modus (SW3 halten) |
| COMM an | verbunden, SW3 loslassen |
| COMM blinkt | schreibt |
| RUN an | fertig |
| ERROR blinkt | ESP noch im Download-Modus, SW3 loslassen |
| ERROR an | Fehler (SWO) |

## Hinweise

- **Takt:** Der Flasher läuft mit HSI 16 MHz ohne PLL und braucht den Quarz nicht. USART3 läuft ebenfalls aus HSI: 115200 Baud zum Verbinden, 460800 zum Schreiben, Fehler 0,6 %.
- **Strapping-Pin IO8:** muss beim Reset High sein, sonst startet der Download-Modus nicht.
- **Platz:** Die Anwendung der Brücke darf höchstens etwa 950 kB groß werden, sonst passt sie nicht mehr in das 1 MB Flash des STM32. Dann bricht der Link mit einem Überlauf von FLASH ab.
- **Spätere Updates** der Brücke gehen per WLAN (`tools/esp32c3_bridge/ota_upload.py`). Den Flasher braucht man nur für das erste Mal oder wenn OTA nicht mehr geht.
