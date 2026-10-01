#!/usr/bin/env python3
"""
mkimages.py – ESP32-C3-Images aus dem Build der Brücke in die Flasher-Firmware einbetten.

Liest <esp-build>/flash_args (von idf.py build erzeugt: Flash-Größe und je Zeile Adresse + Datei)
und schreibt eine Assemblerdatei mit den Images (.incbin) und der Tabelle g_espImages
(Adresse, Daten, Größe, Name, MD5) für src/main.cpp.

  python3 mkimages.py ../esp32c3_bridge/build build/esp_images.S
"""
import hashlib
import pathlib
import sys

SIZES = {"1MB": 1 << 20, "2MB": 2 << 20, "4MB": 4 << 20, "8MB": 8 << 20, "16MB": 16 << 20}


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    build = pathlib.Path(sys.argv[1]).resolve()
    out = pathlib.Path(sys.argv[2])
    args_file = build / "flash_args"
    if not args_file.exists():
        sys.exit(f"{args_file} fehlt – zuerst die Brücke bauen (idf.py build in tools/esp32c3_bridge)")

    lines = [l.split() for l in args_file.read_text().splitlines() if l.strip()]
    opts, entries = lines[0], lines[1:]
    flash_size = SIZES[opts[opts.index("--flash_size") + 1]] if "--flash_size" in opts else 4 << 20

    images = []
    for off_txt, rel in entries:
        path = build / rel
        data = path.read_bytes()
        off = int(off_txt, 0)
        if off + len(data) > flash_size:
            sys.exit(f"{rel}: endet hinter dem Flash ({flash_size} B)")
        if rel.endswith(".bin") and "ota_data" not in rel and "partition" not in rel and data[:1] != b"\xE9":
            sys.exit(f"{rel}: kein ESP-Image (erstes Byte 0xE9 erwartet)")
        images.append((off, path, len(data), rel, hashlib.md5(data).hexdigest()))
    images.sort()
    for (o1, _, s1, n1, _), (o2, _, _, n2, _) in zip(images, images[1:]):
        if o1 + s1 > o2:
            sys.exit(f"{n1} überlappt {n2}")

    s = ["/* erzeugt von mkimages.py aus " + str(args_file) + " – nicht bearbeiten */",
         '    .section .rodata.esp_images,"a"']
    for i, (_, path, _, _, _) in enumerate(images):
        s += ["    .balign 4", f"esp_img{i}:", f'    .incbin "{path.as_posix()}"', f"esp_img{i}_end:"]
    s += ["    .balign 4", "    .global g_espImages", "g_espImages:"]
    for i, (off, _, _, _, md5) in enumerate(images):
        s += [f"    .word 0x{off:X}, esp_img{i}, esp_img{i}_end - esp_img{i}, esp_name{i}",
              f'    .ascii "{md5}"', "    .byte 0, 0, 0, 0"]
    s += ["    .global g_espImageCount", "g_espImageCount:", f"    .word {len(images)}",
          "    .global g_espFlashSize", "g_espFlashSize:", f"    .word {flash_size}"]
    for i, (_, _, _, name, _) in enumerate(images):
        s += [f"esp_name{i}:", f'    .asciz "{name}"']
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(s) + "\n")
    total = sum(i[2] for i in images)
    print(f"{len(images)} Images, {total} B, Flash {flash_size >> 20} MB -> {out}")
    for off, _, size, name, md5 in images:
        print(f"  0x{off:06X} {size:8d} B  {md5}  {name}")


if __name__ == "__main__":
    main()
