/*
 * t_scaling – Rohdaten-Skalierung in 114
 *
 * Bezug: Befund 6
 * Prüft/misst: pushBlock() mit Rohwerten im SAI/ADAU7118-Format (24 Bit linksbündig im 32-Bit-Slot) -> float [-1, 1)
 * STM32F746ZGT6: 114 speichert 16-Bit-Blockgleitkomma. |pcm24| < 2^15 im Block: exakt (wie float);
 * lautere Blöcke: Abweichung < 1 LSB der 16-Bit-Mantisse (2^-15 des Blockmaximums).
 * Aufruf: make check  bzw.  build/test_host/t_scaling
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include "Sensor_Unit_112/Microphone_Array_114.hpp"
using namespace sds110;
static int32_t hw(int32_t pcm24) { return static_cast<int32_t>(static_cast<uint32_t>(pcm24) << 8); }
int main()
{
    auto& arr = Microphone_Array_114::instance();
    const struct { const char* name; int32_t pcm24; float expect; } cases[] = {
        { "+Vollaussteuerung", 0x7FFFFF, 0.99999988f }, { "-Vollaussteuerung", -0x800000, -1.0f },
        { "+0,5", 0x400000, 0.5f }, { "-0,25", -0x200000, -0.25f }, { "1 LSB", 1, 1.0f / 8388608.0f }, { "0", 0, 0.0f },
        { "+32767 LSB", 32767, 32767.0f / 8388608.0f }, { "-12345 LSB", -12345, -12345.0f / 8388608.0f } };
    int fail = 0;
    for (const auto& c : cases) {
        static int32_t block[DMA_BLOCK_SAMPLES * NUM_MICS];
        for (uint32_t i = 0; i < DMA_BLOCK_SAMPLES * NUM_MICS; ++i) block[i] = hw(c.pcm24);
        // Blöcke nachschieben, bis ein 114-Puffer fertig ist (seit Befund 17 ein Hop, davor ein Frame) –
        // so läuft der Test auch als Gegenprobe gegen ältere Stände
        MicFrame* f = nullptr;
        for (int b = 0; b < 256 && !f; ++b) { arr.pushBlock(block, DMA_BLOCK_SAMPLES, 0); f = arr.acquireReadable(); }
        if (!f) { std::printf("kein Puffer fertig\n"); return 1; }
        const float v = f->sample(3, 100);
        // exakt, solange der Block in 16 Bit passt; sonst < 1 LSB der Mantisse (Blockmaximum / 2^15)
        const bool exact = c.pcm24 >= -32768 && c.pcm24 <= 32767;
        const float tol = exact ? 0.0f : std::fabs(c.expect) / 32768.0f;
        const bool ok = std::fabs(v - c.expect) <= tol;
        std::printf("%-18s roh 0x%08X -> %+.8f (soll %+.8f) %s\n", c.name, (unsigned)hw(c.pcm24), v, c.expect, ok ? "OK" : "FEHLER");
        fail += !ok; arr.release(f);
    }
    return fail;
}
