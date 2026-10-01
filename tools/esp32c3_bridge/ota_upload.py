#!/usr/bin/env python3
"""
ota_upload.py – Firmware der ESP32-C3-Brücke per WLAN aktualisieren (OTA).

  python ota_upload.py 192.168.4.1 build/sds110_esp32c3_bridge.bin
  python ota_upload.py 192.168.4.1:3335 app.bin --key sds110-ota

Protokoll (main/main.cpp, otaSession): Zeile "OTA <größe> <schlüssel>\\n", danach das App-Image.
Antwort "OK" (die Brücke startet neu) oder "ERR <grund>". Startet die neue Firmware das WLAN nicht,
kehrt die Brücke nach 60 s zur vorigen zurück (Rollback).
"""
import argparse
import socket
import sys

OTA_PORT = 3335


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", help="host[:port] der Brücke (Port 3335)")
    ap.add_argument("image", help="App-Image, z. B. build/sds110_esp32c3_bridge.bin")
    ap.add_argument("--key", default="sds110-ota", help="CONFIG_BRIDGE_OTA_KEY (Standard sds110-ota)")
    args = ap.parse_args()

    data = open(args.image, "rb").read()
    if not data or data[0] != 0xE9:
        sys.exit(f"{args.image}: kein ESP-App-Image (erstes Byte 0xE9 erwartet)")
    host, _, port = args.target.partition(":")
    with socket.create_connection((host, int(port or OTA_PORT)), timeout=10) as s:
        s.sendall(f"OTA {len(data)} {args.key}\n".encode("ascii"))
        step = 64 * 1024
        try:
            for off in range(0, len(data), step):
                s.sendall(data[off:off + step])
                print(f"\r{min(off + step, len(data)) * 100 // len(data):3d} %", end="", flush=True)
        except OSError:                                   # Brücke hat abgelehnt und geschlossen
            pass
        print()
        s.settimeout(60)                                  # Prüfung des Images dauert einige Sekunden
        try:
            reply = s.recv(128).decode("ascii", "replace").strip()
        except OSError as e:
            reply = f"Verbindung abgebrochen: {e}"
    print(reply or "keine Antwort")
    sys.exit(0 if reply == "OK" else 1)


if __name__ == "__main__":
    main()
