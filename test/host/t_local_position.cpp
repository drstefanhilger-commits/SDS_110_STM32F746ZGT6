/*
 * t_local_position – Standort der Einheit in lokalen Koordinaten (FSL9 §1, A7), USB Id 10 / Nachricht Id 6
 *
 * Prüft (Infrastructure/Utils/LocalPosition.hpp):
 *   1. Grundwert: Ursprung [0, 0, 0], nicht gesetzt
 *   2. Id 10: Ost/Nord/Oben in mm, Vorzeichen; Bytes wie im PC-Monitor
 *      (app/model/SDSUSBModel.py build_position_message, tests/test_position.py)
 *   3. Grenzen: |Ost|, |Nord| > 100 km, Oben außerhalb −1000 … +10000 m, falsche Länge -> verworfen
 *   4. Flags = 0 -> zurück auf den Ursprung
 *   5. Nachricht Id 6: Nutzlast little-endian (Unit, Flags, Ost, Nord, Oben)
 * Aufruf: build/test_host/t_local_position
 */
#include <cstdio>
#include <cstring>
#include "Infrastructure/Utils/LocalPosition.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

static void put32(uint8_t* b, int32_t v) { const uint32_t u = static_cast<uint32_t>(v); b[0] = u >> 24; b[1] = u >> 16; b[2] = u >> 8; b[3] = u; }
static void cmd(uint8_t* m, int32_t e, int32_t n, int32_t u, uint8_t flags)
{
    const uint8_t h[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x0A, 0x00, 0x00, 0x1C};
    std::memcpy(m, h, 8); put32(m + 8, e); put32(m + 12, n); put32(m + 16, u);
    m[20] = flags; m[21] = m[22] = m[23] = 0; put32(m + 24, 0);
}

int main()
{
    // 1) Grundwert
    const LocalPosition def;
    check(def.eastMm == 0 && def.northMm == 0 && def.upMm == 0 && !def.set, "Grundwert: Ursprung [0, 0, 0], nicht gesetzt");

    // 2) Id 10
    uint8_t m[28];
    LocalPosition p;
    cmd(m, 123456, -78900, 5500, 1);
    check(LocalPositionCodec::decode(m, 28, p) && p.set && p.eastMm == 123456 && p.northMm == -78900 && p.upMm == 5500 &&
          p.eastM() > 123.45f && p.eastM() < 123.46f, "Id 10: Ost 123,456 m, Nord −78,9 m, Oben 5,5 m");
    static const uint8_t pc[24] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x0A, 0x00, 0x00, 0x1C, 0x00, 0x01, 0xE2, 0x40,
                                    0xFF, 0xFE, 0xCB, 0xCC, 0x00, 0x00, 0x15, 0x7C, 0x01, 0x00, 0x00, 0x00 };
    check(std::memcmp(m, pc, 24) == 0, "Bytes des PC-Monitors gleich");

    // 3) Grenzen
    LocalPosition q; q.eastMm = 7; q.set = true;
    cmd(m, 100000001, 0, 0, 1);  bool r1 = LocalPositionCodec::decode(m, 28, q);
    cmd(m, 0, -100000001, 0, 1); bool r2 = LocalPositionCodec::decode(m, 28, q);
    cmd(m, 0, 0, 10000001, 1);   bool r3 = LocalPositionCodec::decode(m, 28, q);
    cmd(m, 0, 0, -1000001, 1);   bool r4 = LocalPositionCodec::decode(m, 28, q);
    cmd(m, 0, 0, 0, 1);          bool r5 = LocalPositionCodec::decode(m, 24, q);
    check(!r1 && !r2 && !r3 && !r4 && !r5 && q.eastMm == 7, "außerhalb der Grenzen / falsche Länge verworfen, alter Wert bleibt");
    cmd(m, -100000000, 100000000, -1000000, 1);
    check(LocalPositionCodec::decode(m, 28, q) && q.set, "Grenzwerte ±100 km, −1000 m gültig");

    // 4) Flags 0
    cmd(m, 5000, 6000, 7000, 0);
    check(LocalPositionCodec::decode(m, 28, q) && !q.set && q.eastMm == 0 && q.northMm == 0 && q.upMm == 0,
          "Flags 0 -> zurück auf den Ursprung");

    // 5) Nachricht Id 6
    uint8_t out[16];
    LocalPositionCodec::encodeReport(p, 4660, out);
    int32_t v[3]; std::memcpy(v, out + 4, 12);
    const uint16_t unit = static_cast<uint16_t>(out[0] | (out[1] << 8));
    check(unit == 4660 && out[2] == 0 && out[3] == 1 && v[0] == 123456 && v[1] == -78900 && v[2] == 5500,
          "Id 6: Unit, gesetzt, Ost, Nord, Oben (little-endian)");
    LocalPositionCodec::encodeReport(LocalPosition{}, 1, out);
    check(out[3] == 0 && out[4] == 0 && out[8] == 0 && out[12] == 0, "Id 6 Grundwert: nicht gesetzt, Ursprung");
    std::printf(g_fail ? "=== FEHLER\n" : "=== alle Prüfungen bestanden\n");
    return g_fail;
}
