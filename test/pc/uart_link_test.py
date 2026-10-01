#!/usr/bin/env python3
"""
uart_link_test.py – Test der PC-Verbindung SDS_110 (STM32F746ZGT6) <-> PC über USART1/CP2102N

Protokoll: doc/ICD_SDS_PC_Monitor.md (921600 Baud, 8N1, ohne Flusssteuerung).
Erkennt selbst, welche Firmware läuft:

  * Selbsttest (SDS110_UART_SELFTEST 1, Infrastructure/Driver/UartSelfTest.cpp):
    Herzschlag "UART-SELFTEST ..." als Logger Id 99; jedes Kommando kommt als
    "ECHO id=.. len=.. crc=OK c=<crc>" zurück.
  * normale Firmware: Standort Id 6 jede Sekunde; auf Kommando Id 10 (Standort) antwortet sie
    sofort mit Id 6 und denselben Koordinaten (ICD 4.5) – das ist der Rundlauftest.

Ablauf:
  1. Zuhören (--listen s): Nachrichten je Id, CRC-Fehler, übersprungene Bytes, Datenrate
  2. Rundlauf (--rounds n) mit drei Varianten je Runde:
       a) ein Kommando je Schreibvorgang
       b) ein Kommando auf zwei Schreibvorgänge verteilt (5 ms Pause, < 20 ms Rest-Timeout)
       c) zwei Kommandos in einem Schreibvorgang
  3. Normale Firmware: Standort zum Schluss auf den Ursprung zurücksetzen (Id 10, Flags 0),
     außer mit --keep-position. Der PC-Monitor sendet seinen Standort beim Verbinden ohnehin neu.

Aufruf:
  pip install pyserial
  python test/pc/uart_link_test.py --list                  # COM-Ports anzeigen (CP210x suchen)
  python test/pc/uart_link_test.py --port COM5             # Windows
  python test/pc/uart_link_test.py --port /dev/ttyUSB0     # Linux/WSL
  python test/pc/uart_link_test.py --simulate selftest     # ohne Hardware (Skript selbst prüfen)
  python test/pc/uart_link_test.py --simulate firmware

Exit-Code 0 = bestanden, 1 = fehlgeschlagen, 2 = Port nicht zu öffnen.
Vorher den PC-Monitor schließen: ein COM-Port kann nur von einem Programm geöffnet sein.
"""
import argparse
import random
import struct
import sys
import time
import zlib
from collections import Counter

BAUD = 921600
MAGIC_LE = b"\xEF\xBE\xAD\xDE"          # SDS -> PC: uint32 0xDEADBEEF little-endian
MAGIC_BE = b"\xDE\xAD\xBE\xEF"          # PC -> SDS
NAMES = {1: "Detect", 2: "Read", 3: "Log(alt)", 5: "UnitReport", 6: "Standort", 99: "Logger"}
KNOWN_LEN = {1: 32, 2: 532, 3: 48, 5: 144, 6: 144, 99: 144}


# ------------------------------------------------------------------ Kommandos PC -> SDS (ICD 4)
def command(cmd_id: int, payload: bytes) -> bytes:
    """Magic, Id, Gesamtlänge u24 BE, Nutzlast, CRC32 BE (zlib) über alles davor."""
    length = 8 + len(payload) + 4
    head = MAGIC_BE + bytes([cmd_id]) + length.to_bytes(3, "big") + payload
    return head + zlib.crc32(head).to_bytes(4, "big")


def cmd_position(east_mm: int, north_mm: int, up_mm: int, set_flag: bool = True) -> bytes:
    """Id 10, 28 Byte (ICD 4.5)."""
    return command(10, struct.pack(">iiiB3x", east_mm, north_mm, up_mm, 1 if set_flag else 0))


def cmd_srp(on: int) -> bytes:
    """Id 6, 16 Byte – für den Selbsttest ein harmloses Kurzkommando."""
    return command(6, struct.pack(">I", on))


# ------------------------------------------------------------------ Nachrichten SDS -> PC (ICD 5)
class Message:
    def __init__(self, raw: bytes, t: float):
        self.raw = raw
        self.t = t
        len_id = struct.unpack_from("<I", raw, 4)[0]
        self.id = len_id >> 24
        self.length = len_id & 0xFFFFFF
        self.crc_ok = struct.unpack_from("<I", raw, len(raw) - 4)[0] == zlib.crc32(raw[:-4])

    def text(self) -> str:
        """Logger Id 99: ASCII-Text der Nutzlast (Byte 12–139)."""
        return self.raw[12:140].split(b"\0", 1)[0].decode("ascii", "replace")

    def position(self):
        """Standort Id 6: (unit, flags, east, north, up)."""
        unit, _res, flags, e, n, u = struct.unpack_from("<HBBiii", self.raw, 12)
        return unit, flags, e, n, u


class StreamParser:
    """Bytestrom -> Nachrichten; synchronisiert auf das Magic, prüft Länge und CRC."""

    def __init__(self):
        self.buf = bytearray()
        self.skipped = 0          # Bytes außerhalb gültiger Rahmen (Resync)
        self.crc_errors = 0
        self.bytes_total = 0

    def feed(self, data: bytes, t: float):
        self.bytes_total += len(data)
        self.buf += data
        out = []
        while True:
            i = self.buf.find(MAGIC_LE)
            if i < 0:
                keep = 3 if len(self.buf) > 3 else len(self.buf)
                self.skipped += len(self.buf) - keep
                del self.buf[:len(self.buf) - keep]
                return out
            if i > 0:
                self.skipped += i
                del self.buf[:i]
            if len(self.buf) < 8:
                return out
            len_id = struct.unpack_from("<I", self.buf, 4)[0]
            mid, length = len_id >> 24, len_id & 0xFFFFFF
            if KNOWN_LEN.get(mid) != length:          # scheinbares Magic: 1 Byte weiter
                self.skipped += 1
                del self.buf[:1]
                continue
            if len(self.buf) < length:
                return out
            msg = Message(bytes(self.buf[:length]), t)
            del self.buf[:length]
            if msg.crc_ok:
                out.append(msg)
            else:
                self.crc_errors += 1


# ------------------------------------------------------------------ Verbindung
class Link:
    def __init__(self, port):
        self.port = port
        self.parser = StreamParser()
        self.log = []            # alle gültigen Nachrichten
        self.queue = []          # noch nicht von wait_for geprüfte Nachrichten

    def write(self, data: bytes):
        self.port.write(data)
        self.port.flush()

    def poll(self, timeout: float):
        """Bis timeout lesen; liefert neue Nachrichten."""
        end = time.monotonic() + timeout
        new = []
        while True:
            n = self.port.in_waiting
            data = self.port.read(n if n else 1)
            if data:
                new += self.parser.feed(data, time.monotonic())
            if time.monotonic() >= end:
                break
        self.log += new
        self.queue += new
        return new

    def wait_for(self, pred, timeout: float):
        """Erste Nachricht mit pred(msg) oder None; liefert (msg, Wartezeit in s)."""
        t0 = time.monotonic()
        while True:
            while self.queue:                     # in Reihenfolge; nicht passende verwerfen
                m = self.queue.pop(0)
                if pred(m):
                    return m, max(0.0, m.t - t0)
            if time.monotonic() - t0 >= timeout:
                return None, timeout
            self.poll(0.01)


# ------------------------------------------------------------------ Simulation ohne Hardware
class FakeDevice:
    """Stellt Selbsttest oder normale Firmware nach; Schnittstelle wie serial.Serial."""

    def __init__(self, mode: str):
        self.mode = mode
        self.out = bytearray()
        self.rx = bytearray()
        self.next_beat = time.monotonic()
        self.pos = (0, 0, 0, 0)
        self.timeout = 0.02

    @staticmethod
    def frame(mid: int, data: bytes) -> bytes:
        raw = MAGIC_LE + struct.pack("<II", (mid << 24) | 144, 0) + data.ljust(128, b"\0")
        return raw + struct.pack("<I", zlib.crc32(raw))

    def _tick(self):
        now = time.monotonic()
        if now >= self.next_beat:
            self.next_beat = now + 1.0
            if self.mode == "selftest":
                self.out += self.frame(99, b"UART-SELFTEST t=0 rx=0 cmd=0 bad=0 err=0 ore=0 fe=0 drop=0")
            else:
                self.out += self.frame(6, struct.pack("<HBBiii", 4711, 0, *self.pos))
                self.out += self.frame(99, b"SDS110 simuliert")
        while True:
            i = self.rx.find(MAGIC_BE)
            if i < 0 or len(self.rx) < i + 8:
                return
            del self.rx[:i]
            n = int.from_bytes(self.rx[5:8], "big")
            if len(self.rx) < n:
                return
            c, self.rx = bytes(self.rx[:n]), self.rx[n:]
            crc = int.from_bytes(c[-4:], "big")
            if self.mode == "selftest":
                ok = "OK" if crc == zlib.crc32(c[:-4]) else "BAD"
                self.out += self.frame(99, f"ECHO id={c[4]} len={n} crc={ok} c={crc:08X}".encode())
            elif c[4] == 10:
                e, no, u, fl = struct.unpack_from(">iiiB", c, 8)
                self.pos = (fl & 1, e, no, u) if fl & 1 else (0, 0, 0, 0)
                self.out += self.frame(6, struct.pack("<HBBiii", 4711, 0, *self.pos))

    @property
    def in_waiting(self):
        self._tick()
        return len(self.out)

    def read(self, n):
        self._tick()
        if not self.out:
            time.sleep(0.001)
        d, self.out = bytes(self.out[:n]), self.out[n:]
        return d

    def write(self, d):
        self.rx += d

    def flush(self):
        pass

    def reset_input_buffer(self):
        self.out.clear()

    def close(self):
        pass


# ------------------------------------------------------------------ Tests
class Result:
    def __init__(self):
        self.rows = []

    def add(self, ok: bool, name: str, detail: str = ""):
        self.rows.append((ok, name, detail))
        print(f"  {'ok  ' if ok else 'FAIL'} {name}" + (f"  ({detail})" if detail else ""))

    @property
    def passed(self):
        return all(ok for ok, _, _ in self.rows) and self.rows


def listen(link: Link, seconds: float, res: Result) -> str:
    print(f"\n[1] Zuhören {seconds:.0f} s")
    link.poll(seconds)
    link.queue.clear()
    p = link.parser
    ids = Counter(m.id for m in link.log)
    for mid, n in sorted(ids.items()):
        print(f"      Id {mid:3d} {NAMES.get(mid, '?'):11s} {n:5d}")
    for m in [m for m in link.log if m.id == 99][-3:]:
        print(f"      Logger: {m.text()}")
    rate = p.bytes_total / seconds if seconds else 0
    print(f"      {p.bytes_total} Byte ({rate / 1000:.1f} kB/s), CRC-Fehler {p.crc_errors}, übersprungen {p.skipped}")

    res.add(p.bytes_total > 0, "Daten vom Board empfangen",
            "" if p.bytes_total else "nichts – Port, Baudrate, Jumper JM1/JM2, Firmware geflasht?")
    res.add(len(link.log) > 0, "gültige ICD-Nachrichten",
            "" if link.log or not p.bytes_total else "nur Müll – Baudrate falsch (921600?)")
    res.add(p.crc_errors == 0, "keine CRC-Fehler", f"{p.crc_errors}")
    res.add(p.skipped == 0, "keine übersprungenen Bytes", f"{p.skipped}")

    if any(m.id == 99 and m.text().startswith("UART-SELFTEST") for m in link.log):
        mode = "selftest"
        for m in link.log:
            if m.id == 99 and "start" in m.text():
                print(f"      Start: {m.text()}")
        beats = [m for m in link.log if m.id == 99 and m.text().startswith("UART-SELFTEST t=")]
        res.add(len(beats) >= max(1, int(seconds) - 1), "Herzschlag ~1/s", f"{len(beats)} in {seconds:.0f} s")
        if beats:
            last = dict(kv.split("=") for kv in beats[-1].text().split()[1:])
            res.add(last.get("ore") == "0" and last.get("fe") == "0", "Board: kein Überlauf/Rahmenfehler",
                    f"ore={last.get('ore')} fe={last.get('fe')}")
    elif ids:
        mode = "firmware"
        res.add(ids[6] >= max(1, int(seconds) - 1), "Standort Id 6 ~1/s", f"{ids[6]} in {seconds:.0f} s")
    else:
        mode = "none"
    print(f"      erkannt: {dict(selftest='Selbsttest-Firmware', firmware='normale Firmware', none='nichts')[mode]}")
    return mode


def roundtrip_selftest(link: Link, rounds: int, res: Result):
    print(f"\n[2] Rundlauf Selbsttest, {rounds} Runden")
    lat, fails = [], []

    def expect(c: bytes, label: str):
        crc = int.from_bytes(c[-4:], "big")
        want = f"ECHO id={c[4]} len={len(c)} crc=OK c={crc:08X}"
        m, dt = link.wait_for(lambda m: m.id == 99 and m.text().startswith("ECHO"), 0.5)
        if m is None:
            fails.append(f"{label}: keine Antwort")
        elif m.text() != want:
            fails.append(f"{label}: '{m.text()}' statt '{want}'")
        else:
            lat.append(dt)

    for r in range(rounds):
        c = cmd_position(random.randint(-10**8 + 10, 10**8 - 10), random.randint(-10**8 + 10, 10**8 - 10), random.randint(-10**6, 10**7))
        link.write(c)
        expect(c, f"R{r} einzeln")
        link.write(c[:10]); time.sleep(0.005); link.write(c[10:])
        expect(c, f"R{r} geteilt")
        c2 = cmd_srp(r & 1)
        link.write(c2 + c)
        expect(c2, f"R{r} zusammen 1/2"); expect(c, f"R{r} zusammen 2/2")
    _report(res, lat, fails, rounds * 4)


def roundtrip_firmware(link: Link, rounds: int, res: Result):
    print(f"\n[2] Rundlauf normale Firmware (Id 10 -> Id 6), {rounds} Runden")
    lat, fails = [], []

    def expect(p, label: str):
        m, dt = link.wait_for(lambda m: m.id == 6 and m.position()[1] & 1 and m.position()[2:] == p, 0.5)
        if m is None:
            fails.append(f"{label}: kein Id 6 mit {p}")
        else:
            lat.append(dt)

    for r in range(rounds):
        p = (random.randint(-10**8 + 10, 10**8 - 10), random.randint(-10**8 + 10, 10**8 - 10), random.randint(-10**6, 10**7))
        c = cmd_position(*p)
        link.write(c)
        expect(p, f"R{r} einzeln")
        p = (p[0] + 1, p[1], p[2]); c = cmd_position(*p)
        link.write(c[:10]); time.sleep(0.005); link.write(c[10:])
        expect(p, f"R{r} geteilt")
        p2 = (p[0] + 2, p[1], p[2])
        link.write(cmd_position(*p) + cmd_position(*p2))
        expect(p2, f"R{r} zusammen")
    _report(res, lat, fails, rounds * 3)


def _report(res: Result, lat, fails, total):
    for f in fails[:10]:
        print(f"      {f}")
    if lat:
        lat_ms = sorted(x * 1000 for x in lat)
        print(f"      Antwortzeit min {lat_ms[0]:.1f} ms, Median {lat_ms[len(lat_ms) // 2]:.1f} ms, max {lat_ms[-1]:.1f} ms")
    res.add(not fails, "alle Kommandos beantwortet", f"{total - len(fails)}/{total}")


def open_port(args):
    if args.simulate:
        return FakeDevice(args.simulate)
    try:
        import serial
    except ImportError:
        sys.exit("pyserial fehlt: pip install pyserial")
    try:
        return serial.Serial(args.port, args.baud, bytesize=8, parity="N", stopbits=1,
                             timeout=0.02, rtscts=False, dsrdtr=False, xonxoff=False)
    except serial.SerialException as e:
        print(f"Port {args.port} nicht zu öffnen: {e}\n(PC-Monitor geschlossen? Mit --list die Ports anzeigen.)")
        sys.exit(2)


def main():
    ap = argparse.ArgumentParser(description="Test USART1/CP2102N-Verbindung SDS_110 <-> PC (ICD)")
    ap.add_argument("--port", help="COM-Port, z. B. COM5 oder /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=BAUD, help=f"Baudrate (Standard {BAUD}, SDS110_UART_BAUD)")
    ap.add_argument("--listen", type=float, default=3.0, help="Sekunden zuhören (Standard 3)")
    ap.add_argument("--rounds", type=int, default=10, help="Rundlauf-Runden (Standard 10)")
    ap.add_argument("--keep-position", action="store_true", help="normale Firmware: Standort nicht zurücksetzen")
    ap.add_argument("--simulate", choices=["selftest", "firmware"], help="ohne Hardware gegen ein Modell testen")
    ap.add_argument("--list", action="store_true", help="serielle Ports anzeigen")
    args = ap.parse_args()

    if args.list:
        from serial.tools import list_ports
        for p in list_ports.comports():
            print(f"{p.device:15s} {p.description}" + ("   <- CP210x" if "CP210" in (p.description or "") else ""))
        return 0
    if not args.port and not args.simulate:
        ap.error("--port oder --simulate angeben")

    port = open_port(args)
    print(f"Port {args.port or 'Simulation ' + args.simulate}, {args.baud} Baud 8N1")
    port.reset_input_buffer()
    link = Link(port)
    res = Result()
    try:
        mode = listen(link, args.listen, res)
        if mode == "selftest":
            roundtrip_selftest(link, args.rounds, res)
        elif mode == "firmware":
            roundtrip_firmware(link, args.rounds, res)
            if not args.keep_position:
                link.write(cmd_position(0, 0, 0, set_flag=False))
                print("      Standort auf Ursprung zurückgesetzt (Id 10, Flags 0)")
    finally:
        port.close()

    print("\nERGEBNIS:", "BESTANDEN" if res.passed else "FEHLGESCHLAGEN")
    return 0 if res.passed else 1


if __name__ == "__main__":
    sys.exit(main())
