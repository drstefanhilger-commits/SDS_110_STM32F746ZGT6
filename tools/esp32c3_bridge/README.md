# ESP32-C3-Brücke: SDS_110 ⇄ PC-Monitor über WLAN

Firmware für den **ESP32-C3-MINI-1-N4** auf dem SDS_110-Board (STM32F746ZGT6). Sie reicht die
Bytes zwischen USART3 des STM32 und einem TCP-Client am PC unverändert durch. Rahmen, Magic und
CRC bleiben wie in `doc/ICD_SDS_PC_Monitor.md`. Dieser Weg ersetzt USB, das auf dem Board nicht
funktioniert.

```
STM32F746 USART3 ──1 Mbaud 8N1──► ESP32-C3 UART0 ──WLAN──► PC-Monitor
  PD8 TX → RXD0 (Pin 30, GPIO20)        TCP 3333    Daten
  PD9 RX ← TXD0 (Pin 31, GPIO21)        UDP 3334    Ankündigung und Status
  PC13 EN_ESP_CTRL → R40 → EN           TCP 3335    Firmware-Update (OTA)
```

Seite STM32: `SDS110_PC_UART 3` (Standard) in `Core/SDS_110/SDS_110_Board.h`, siehe
`doc/Portierung_STM32F746ZGT6.md`, Abschnitt 8.

## Verhalten

| Thema | Verhalten |
| --- | --- |
| WLAN | Access Point `SDS110-xxxx` (xxxx = MAC-Ende), WPA2, Passwort `sds110-wlan`, Brücke unter **192.168.4.1**. Alternativ Anmeldung in einem vorhandenen WLAN (menuconfig). |
| Daten | TCP-Port **3333**, ein PC zur Zeit, TCP_NODELAY. Ohne PC werden die Daten des STM32 verworfen. |
| Ankündigung | UDP-Broadcast alle 2 s auf Port **3334**: `SDS110-BRIDGE <ip> 3333 <mac> <version> pc=<0/1> up=<B> down=<B> drop=<B>`. Das ist zugleich die Statusanzeige, denn die Logs (USB-Serial/JTAG) sind auf dem Board nicht zugänglich. |
| Update | TCP-Port **3335** mit Schlüssel (`ota_upload.py`). Startet die neue Firmware das WLAN nicht innerhalb von 60 s, kehrt die Brücke zur vorigen zurück. |
| UART0 | Die Firmware gibt nie Text auf UART0 aus. Nur das ROM des ESP32-C3 meldet sich beim Booten mit 115200 Baud; die STM32-Firmware verwirft deshalb die ersten 1,5 s (`SDS110_ESP_BOOT_MS`). |

Einstellungen über `idf.py menuconfig` → **SDS110 Bridge**: Betriebsart AP/STA, SSID, Passwort,
Ports, OTA-Schlüssel, Baudrate (muss zu `SDS110_ESP_UART_BAUD` passen), GPIOs.

**Passwort und OTA-Schlüssel vor dem Einsatz ändern.** Die Kommandos an den STM32 haben noch
keine CRC-Prüfung (Befund 12).

## Bauen

ESP-IDF ab 5.3 (getestet mit 5.4.2):

```bash
cd tools/esp32c3_bridge
idf.py set-target esp32c3
idf.py menuconfig        # optional: SDS110 Bridge
idf.py build             # -> build/bootloader/bootloader.bin, build/partition_table/…, build/sds110_esp32c3_bridge.bin
```

Alternativ PlatformIO: `pio run` (gleiche Quellen, `platformio.ini`).

## Erstes Flashen (ohne USB)

Die USB-Pins IO18/IO19 des Moduls sind auf dem Board nicht beschaltet, und UART0 hängt am
STM32. Es gibt zwei Wege:

**A – nur mit dem ST-LINK (empfohlen):** Die Flasher-Firmware `tools/esp32c3_flasher` für den
STM32 enthält die Images und schreibt sie über USART3 in den ESP. Bauen und Ablauf stehen in
`tools/esp32c3_flasher/README.md`.

**B – mit einem USB-UART-Adapter (3,3 V)**, z. B. CP2102 oder FT232:

1. **STM32 im Reset halten**, damit PD8/PD9 hochohmig sind. Das geht z. B. so:
   - in STM32CubeProgrammer mit „Hardware reset“ verbinden und den Reset halten,
   - oder NRST auf GND legen.

   EN des ESP muss dabei über seinen eigenen Pull-up High bleiben (R36, bitte nachmessen).
2. Adapter anschließen:
   - **TX** → Netz `ESP_UART_TX` (PD8, Modul-Pin 30 RXD0)
   - **RX** → Netz `ESP_UART_RX` (PD9, Modul-Pin 31 TXD0)
   - **GND** → GND
3. SW3 (BOOT) halten, SW2 (EN) kurz drücken, SW3 loslassen. IO8 muss High sein.
4. Flashen:
   ```bash
   cd build
   python -m esptool --chip esp32c3 -p COMx -b 460800 --before no_reset --after no_reset write_flash "@flash_args"
   ```
5. Adapter abziehen, STM32 freigeben, ESP neu starten (SW2).

## Update per WLAN

```bash
idf.py build
python ota_upload.py 192.168.4.1 build/sds110_esp32c3_bridge.bin --key sds110-ota
```

## Am PC

```bash
python sds_link_test.py --discover   # Brücke suchen, Sync senden, Nachrichten anzeigen
python sds_link_test.py 192.168.4.1 -q
```

Im PC-Monitor genügt mit pyserial statt `serial.Serial("COM5", 921600)`:

```python
ser = serial.serial_for_url("socket://192.168.4.1:3333", timeout=0.1)
```

## Grenzen

- **READ-Streaming** (Id 2, 1,6 MB/s) geht nicht, die UART schafft etwa 100 kB/s. DETECT braucht in der Spitze etwa 17 kB/s.
- **UTC aus Sync (Id 7)** ist über WLAN nur auf einige ms genau (Laufzeitschwankung). Für die µs-Synchronisation ist GNSS-PPS vorgesehen.
- Wird nur der ESP zurückgesetzt (SW2), während der STM32 läuft, landen die ROM-Meldungen beim STM32. Sie setzen dann einmal das Fehler-Flag. Dauerhaft abschalten lässt sich die ROM-Ausgabe per eFuse; das ist **nicht umkehrbar**:
  ```bash
  python -m espefuse --chip esp32c3 -p COMx burn_efuse UART_PRINT_CONTROL 3
  ```
