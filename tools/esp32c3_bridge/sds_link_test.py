#!/usr/bin/env python3
"""
sds_link_test.py – Verbindung SDS_110 <-> PC prüfen, über WLAN (ESP32-C3-Brücke) oder Kabel (CP2102N, falls nutzbar).

Sendet nach dem Verbinden einen Sync (Id 7, UTC und Temperatur „unbekannt“) und zeigt die
Nachrichten der Firmware an (doc/ICD_SDS_PC_Monitor.md, Abschnitt 5). Alle 5 s eine Statistik.

  python sds_link_test.py --discover            # Brücke im Netz suchen (UDP-Ankündigung, Port 3334)
  python sds_link_test.py 192.168.4.1           # über WLAN, Brücke im AP-Betrieb (Port 3333)
  python sds_link_test.py 192.168.4.1:3333 -q   # nur Statistik
  python sds_link_test.py COM5                  # über CP2102N (pyserial nötig), 921600 Baud

Für den PC-Monitor selbst genügt mit pyserial statt serial.Serial("COM5", 921600):
  serial.serial_for_url("socket://192.168.4.1:3333", timeout=0.1)
"""
import argparse
import socket
import struct
import sys
import time
import zlib

MAGIC_RX = b"\xEF\xBE\xAD\xDE"          # SDS -> PC: uint32 0xDEADBEEF little-endian
MAGIC_TX = b"\xDE\xAD\xBE\xEF"          # PC -> SDS
KNOWN_LEN = {1: 32, 2: 532, 5: 144, 6: 144, 99: 144}
NAMES = {1: "Detect", 2: "Read", 5: "UnitReport", 6: "Standort", 99: "Logger"}
TCP_PORT = 3333
DISCOVERY_PORT = 3334


def command(cmd_id: int, payload: bytes) -> bytes:
    """Kommando PC -> SDS (ICD 4): Magic, Id, Gesamtlänge u24 BE, Nutzlast, CRC32 BE."""
    length = 8 + len(payload) + 4
    head = MAGIC_TX + bytes([cmd_id]) + length.to_bytes(3, "big")
    body = head + payload
    return body + struct.pack(">I", zlib.crc32(body))


def cmd_sync(utc_us: int, temp_centi: int = -0x8000) -> bytes:
    """Id 7 (ICD 4.2): UTC in µs, Temperatur in 0,01 °C (0x8000 = unbekannt)."""
    return command(7, struct.pack(">Qh2x", utc_us, temp_centi))


def cmd_value(cmd_id: int, value: int) -> bytes:
    """Id 1–6, 9 (ICD 4.1): ein u32-Wert."""
    return command(cmd_id, struct.pack(">I", value & 0xFFFFFFFF))


class Parser:
    """Nachrichten SDS -> PC aus dem Bytestrom lösen (Synchronisation über Magic und Länge)."""

    def __init__(self):
        self.buf = bytearray()
        self.crc_errors = 0
        self.skipped = 0

    def feed(self, data: bytes):
        self.buf += data
        while True:
            i = self.buf.find(MAGIC_RX)
            if i < 0:
                keep = min(len(self.buf), 3)
                self.skipped += len(self.buf) - keep
                del self.buf[:len(self.buf) - keep]
                return
            if i:
                self.skipped += i
                del self.buf[:i]
            if len(self.buf) < 8:
                return
            len_id = struct.unpack_from("<I", self.buf, 4)[0]
            length, msg_id = len_id & 0xFFFFFF, len_id >> 24
            if KNOWN_LEN.get(msg_id) != length:
                self.skipped += 1
                del self.buf[:1]
                continue
            if len(self.buf) < length:
                return
            msg = bytes(self.buf[:length])
            del self.buf[:length]
            if zlib.crc32(msg[:-4]) != struct.unpack_from("<I", msg, length - 4)[0]:
                self.crc_errors += 1
                continue
            yield msg_id, msg


def describe(msg_id: int, m: bytes) -> str:
    ts = struct.unpack_from("<I", m, 8)[0]
    if msg_id == 1:
        mic, azi, dist, conf = struct.unpack_from("<Ifff", m, 12)
        return f"Detect     t={ts} unit={mic} azi={azi:6.1f}° r={dist:6.1f} m conf={conf:.2f}"
    if msg_id == 5:
        unit, t_us, src, bearing, resid, pairs, nsel, level = struct.unpack_from("<HQBffBBf", m, 12)
        return (f"UnitReport unit={unit} t_us={t_us} src={src} bearing={bearing:6.1f}° "
                f"pairs={pairs} bands={nsel} level={level:.3g}")
    if msg_id == 6:
        unit, _, flags, e, n, u = struct.unpack_from("<HBBiii", m, 12)
        return f"Standort   unit={unit} gesetzt={flags & 1} O={e / 1000:.3f} N={n / 1000:.3f} H={u / 1000:.3f} m"
    if msg_id == 99:
        return "Logger     " + m[12:140].split(b"\0", 1)[0].decode("ascii", "replace")
    if msg_id == 2:
        mic, block, hop = struct.unpack_from("<BBH", m, 12)
        return f"Read       mic={mic} block={block} hop={hop}"
    return f"Id {msg_id}"


def discover(timeout: float) -> str:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", DISCOVERY_PORT))
    s.settimeout(timeout)
    try:
        while True:
            data, _ = s.recvfrom(256)
            parts = data.decode("ascii", "replace").split()
            if len(parts) >= 3 and parts[0] == "SDS110-BRIDGE":
                print(f"Brücke gefunden: {parts[1]}:{parts[2]}  {' '.join(parts[3:])}")
                return f"{parts[1]}:{parts[2]}"
    except socket.timeout:
        sys.exit(f"keine Brücke in {timeout:.0f} s gefunden (UDP-Port {DISCOVERY_PORT}, Firewall?)")
    finally:
        s.close()


class Link:
    """TCP (host[:port]) oder serielle Schnittstelle (COMx, /dev/tty*)."""

    def __init__(self, target: str, baud: int):
        self.sock = self.ser = None
        if target.upper().startswith("COM") or target.startswith("/dev/"):
            import serial  # pyserial
            self.ser = serial.Serial(target, baud, timeout=0.1)
        else:
            host, _, port = target.partition(":")
            self.sock = socket.create_connection((host, int(port or TCP_PORT)), timeout=5)
            self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.sock.settimeout(0.1)

    def write(self, data: bytes):
        if self.sock:
            self.sock.sendall(data)
        else:
            self.ser.write(data)

    def read(self) -> bytes:
        if self.ser:
            return self.ser.read(4096)
        try:
            data = self.sock.recv(4096)
        except socket.timeout:
            return b""
        if not data:
            raise ConnectionError("Brücke hat die Verbindung geschlossen")
        return data


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("target", nargs="?", help="host[:port] der Brücke oder COM-Port")
    ap.add_argument("--discover", action="store_true", help="Brücke per UDP-Ankündigung suchen")
    ap.add_argument("--baud", type=int, default=921600, help="Baudrate bei COM-Port (Standard 921600)")
    ap.add_argument("--mode", type=int, choices=(1, 2, 3), help="Kommando Id 2 senden (1 DETECT, 2 CALIBRATE, 3 READ)")
    ap.add_argument("--sim", type=int, help="Kommando Id 3 senden (0 Mikrofone, 1…7 Simulator-Szenario)")
    ap.add_argument("-q", "--quiet", action="store_true", help="nur Statistik ausgeben")
    args = ap.parse_args()

    target = discover(10) if args.discover or not args.target else args.target
    link = Link(target, args.baud)
    print(f"verbunden mit {target}")
    link.write(cmd_sync(time.time_ns() // 1000))
    if args.mode:
        link.write(cmd_value(2, args.mode))
    if args.sim is not None:
        link.write(cmd_value(3, args.sim))

    parser = Parser()
    counts, nbytes = {}, 0
    t_stat = t_sync = time.monotonic()
    try:
        while True:
            data = link.read()
            nbytes += len(data)
            for msg_id, msg in parser.feed(data):
                counts[msg_id] = counts.get(msg_id, 0) + 1
                if not args.quiet and msg_id != 2:
                    print(describe(msg_id, msg))
            now = time.monotonic()
            if now - t_sync >= 60:                     # ICD 4.2: etwa jede Minute neu synchronisieren
                link.write(cmd_sync(time.time_ns() // 1000))
                t_sync = now
            if now - t_stat >= 5:
                dt = now - t_stat
                rates = ", ".join(f"{NAMES.get(k, k)} {v / dt:.1f}/s" for k, v in sorted(counts.items()))
                print(f"-- {nbytes / dt / 1024:.1f} kB/s | {rates or 'keine Nachrichten'} | "
                      f"CRC-Fehler {parser.crc_errors}, übersprungen {parser.skipped} B")
                counts, nbytes, t_stat = {}, 0, now
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
