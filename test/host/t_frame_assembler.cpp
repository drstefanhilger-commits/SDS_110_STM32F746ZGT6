/*
 * t_frame_assembler – Analysefenster mit 50 % Überlappung, Hop-Pool im Blockgleitkomma
 *
 * Bezug: Befund 17; STM32F746ZGT6-Port (kein SDRAM): Hops als 16-Bit-Blockgleitkomma, der
 * Frame_Assembler hält Zeiger auf Hops des 114-Pools und gibt sie zurück.
 * Prüft/misst: Füllen, Schieben um einen Hop, Zeitstempel = ältester Hop, Neubeginn bei Lücke in der
 * Hop-Folge, reset(); Rückgabe der Hops an 114 (releaseOldest, Lücke, reset); MicFrame::encode/decode
 * (Fehler < 2^-15 des Blockmaximums), Rohdaten bis 2^15 exakt
 * Aufruf: make check  bzw.  build/test_host/t_frame_assembler
 * Beschreibung und Referenzergebnisse: doc/Host_Tests.md
 */
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include "Sensor_Unit_112/Frame_Assembler.hpp"
using namespace sds110;

static int fail = 0;
static void check(const char* what, bool ok) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); fail += !ok; }

// Rohwert (pcm24, < 2^15 -> verlustfrei) für Hop-Nummer n, Kanal ch, Sample i
static int32_t pcm(uint32_t n, uint32_t ch, uint32_t i) { return static_cast<int32_t>((n % 8) * 4000 + ch * 400 + i % 400) - 16000; }
static uint64_t g_t = 1000000ull;
static uint32_t g_n = 0;                         // Hop-Nummer des Inhalts (unabhängig von frame_id)

// einen Hop über pushBlock() erzeugen und abholen (wie 116 -> 114)
static MicFrame* makeHop()
{
    auto& arr = Microphone_Array_114::instance();
    static int32_t blk[DMA_BLOCK_SAMPLES * NUM_MICS];
    for (uint32_t b = 0; b < BLOCKS_PER_HOP; ++b) {
        for (uint32_t s = 0; s < DMA_BLOCK_SAMPLES; ++s)
            for (uint32_t ch = 0; ch < NUM_MICS; ++ch)
                blk[s * NUM_MICS + ch] = static_cast<int32_t>(static_cast<uint32_t>(pcm(g_n, ch, b * DMA_BLOCK_SAMPLES + s)) << 8);
        arr.pushBlock(blk, DMA_BLOCK_SAMPLES, g_t + b * 2667);
    }
    g_t += 32000; ++g_n;
    return arr.acquireReadable();
}

static bool frameIs(const AnalysisFrame& f, uint32_t nOld, uint64_t tOld)
{
    if (f.time_utc_us != tOld) return false;
    for (uint32_t ch = 0; ch < NUM_MICS; ++ch)
        for (uint32_t k = 0; k < FRAME_SAMPLES; ++k) {
            const uint32_t n = nOld + k / HOP_SAMPLES, i = k % HOP_SAMPLES;
            if (f.sample(ch, k) != static_cast<float>(pcm(n, ch, i)) / 8388608.0f) return false;
        }
    float buf[FRAME_SAMPLES];
    f.decode(2, buf);
    for (uint32_t k = 0; k < FRAME_SAMPLES; ++k) if (buf[k] != f.sample(2, k)) return false;
    return true;
}

// freie Puffer im 114-Pool zählen (alle holen, wieder freigeben)
static uint32_t freeBuffers()
{
    auto& arr = Microphone_Array_114::instance();
    MicFrame* got[NUM_MIC_FRAMES]; uint32_t n = 0;
    while (n < NUM_MIC_FRAMES) { MicFrame* h = makeHop(); if (!h) break; got[n++] = h; }
    for (uint32_t k = 0; k < n; ++k) arr.release(got[k]);
    return n;
}

int main()
{
    static Frame_Assembler fa;
    auto& arr = Microphone_Array_114::instance();

    MicFrame* h = makeHop(); const uint64_t t5 = h->time_utc_us; const uint32_t n5 = g_n - 1;
    check("Hop 1: Fenster noch nicht voll", !fa.push(h));
    h = makeHop(); const uint64_t t6 = h->time_utc_us;
    check("Hop 2: Frame [1|2], Zeit von Hop 1, Werte exakt", fa.push(h) && frameIs(fa.frame(), n5, t5));
    h = makeHop();
    check("Hop 3: Frame [2|3] (um einen Hop geschoben)", fa.push(h) && frameIs(fa.frame(), n5 + 1, t6));
    fa.releaseOldest();
    check("releaseOldest(): Fenster hält noch einen Hop", fa.held() == 1);

    // Lücke: ein Hop wird geholt und ohne Assembler verworfen -> frame_id springt
    h = makeHop(); arr.release(h);
    h = makeHop();
    check("Hop nach Lücke: Neubeginn (kein Frame über die Lücke)", !fa.push(h) && fa.held() == 1);
    h = makeHop();
    check("danach wieder Frames", fa.push(h));
    fa.reset();
    check("reset(): alle Hops an 114 zurück", fa.held() == 0 && freeBuffers() == NUM_MIC_FRAMES);

    // Pool: das Fenster belegt höchstens HOPS_PER_FRAME Puffer
    for (int k = 0; k < 5; ++k) { fa.push(makeHop()); }
    check("Fenster hält höchstens 2 Puffer", fa.held() == Frame_Assembler::HOPS_PER_FRAME
                                            && freeBuffers() == NUM_MIC_FRAMES - Frame_Assembler::HOPS_PER_FRAME);
    fa.reset();

    // encode/decode: Zufallssignal mit stark wechselndem Pegel je Block
    static MicFrame m; static float x[HOP_SAMPLES], y[HOP_SAMPLES];
    std::srand(7);
    double worst = 0.0;
    for (int rep = 0; rep < 50; ++rep) {
        for (uint32_t i = 0; i < HOP_SAMPLES; ++i) {
            const float lvl = std::pow(10.0f, -static_cast<float>((i / DMA_BLOCK_SAMPLES + rep) % 9));
            x[i] = lvl * (2.0f * std::rand() / RAND_MAX - 1.0f);
        }
        m.encode(1, x); m.decode(1, y);
        for (uint32_t b = 0; b < BLOCKS_PER_HOP; ++b) {
            float mx = 0; for (uint32_t i = b * DMA_BLOCK_SAMPLES; i < (b + 1) * DMA_BLOCK_SAMPLES; ++i) mx = std::fmax(mx, std::fabs(x[i]));
            for (uint32_t i = b * DMA_BLOCK_SAMPLES; i < (b + 1) * DMA_BLOCK_SAMPLES; ++i)
                worst = std::fmax(worst, std::fabs(double(y[i]) - x[i]) / mx);
        }
    }
    std::printf("encode/decode: größter Fehler relativ zum Blockmaximum %.3g (2^-15 = %.3g)\n", worst, 1.0 / 32768);
    check("encode/decode: Fehler < 2^-15 des Blockmaximums", worst < 1.0 / 32768);
    return fail;
}
