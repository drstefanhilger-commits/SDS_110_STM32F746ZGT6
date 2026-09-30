/*
 * t_hop_gap – verworfene DMA-Blöcke in 114 und Lücken im Hop-Strom (Befunde 27, 42)
 *
 * Prüft:
 *   1. Laufen alle NUM_MIC_FRAMES Puffer voll (Leser holt nichts ab), verwirft pushBlock() Blöcke;
 *      der nächste Hop nach der Lücke trägt eine übersprungene frame_id (vorher: fortlaufend)
 *   2. Frame_Assembler verbindet die Hops vor und nach der Lücke nicht (kein Frame [alt|neu])
 *   3. ohne Verwerfen bleibt die Folge lückenlos
 *   4. Befund 42: Kanalabstand in MicFrame kein Vielfaches von 1 KB (Cache-Sätze)
 * Aufruf: build/test_host/t_hop_gap
 */
#include <cstdio>
#include <cstring>
#include <vector>
#include "Sensor_Unit_112/Microphone_Array_114.hpp"
#include "Sensor_Unit_112/Frame_Assembler.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }
static int32_t blk[DMA_BLOCK_SAMPLES * NUM_MICS];
static uint64_t g_t = 0;

static void pushHop(Microphone_Array_114& arr)
{
    for (uint32_t b = 0; b < HOP_SAMPLES; b += DMA_BLOCK_SAMPLES) {
        arr.pushBlock(blk, DMA_BLOCK_SAMPLES, g_t);
        g_t += 1000000ULL * DMA_BLOCK_SAMPLES / SAMPLE_RATE_HZ;
    }
}

static std::vector<uint32_t> drain(Microphone_Array_114& arr, Frame_Assembler& fa, int& frames)
{
    std::vector<uint32_t> ids;
    while (MicFrame* h = arr.acquireReadable()) {
        ids.push_back(h->frame_id);
        frames += fa.push(h);                  // Besitz an den Frame_Assembler
        fa.releaseOldest();                    // wie 120 nach den Spektren
    }
    return ids;
}

int main()
{
    auto& arr = Microphone_Array_114::instance();
    static Frame_Assembler fa;
    int frames = 0;
    // 3) Normalbetrieb: je Hop abholen -> lückenlos
    std::vector<uint32_t> ids;
    for (int k = 0; k < 4; ++k) { pushHop(arr); auto v = drain(arr, fa, frames); ids.insert(ids.end(), v.begin(), v.end()); }
    bool seq = ids.size() == 4;
    for (size_t i = 1; i < ids.size(); ++i) seq = seq && ids[i] == ids[i - 1] + 1;
    check(seq && frames == 3 && arr.droppedFrames() == 0, "ohne Verwerfen: frame_id fortlaufend, Frames ab dem 2. Hop");

    // 1) Leser hängt: freie Puffer voll (einer hält der Frame_Assembler), weitere Hops werden verworfen
    const uint32_t lastBefore = ids.back();
    for (int k = 0; k < 4; ++k) pushHop(arr);                 // NUM_MIC_FRAMES − 1 Hops fertig, Rest verworfen
    const uint32_t dropped = arr.droppedFrames();
    frames = 0;
    auto held = drain(arr, fa, frames);                        // die fertigen Hops
    pushHop(arr); pushHop(arr); pushHop(arr);                  // nach der Lücke (1. Block noch verworfen)
    auto after = drain(arr, fa, frames);
    std::printf("vorher bis %u, gehalten", lastBefore);
    for (auto i : held) std::printf(" %u", i);
    std::printf(", nach der Lücke");
    for (auto i : after) std::printf(" %u", i);
    std::printf(" (verworfene Blöcke %u)\n", dropped);
    const size_t nHeld = NUM_MIC_FRAMES - 1;                  // ein Puffer liegt im Analysefenster
    const bool heldSeq = held.size() == nHeld && held[0] == lastBefore + 1 && held[nHeld - 1] == held[0] + nHeld - 1;
    check(dropped > 0 && heldSeq, "volle Puffer: Blöcke verworfen, gehaltene Hops fortlaufend");
    check(after.size() >= 2 && after[0] > held.back() + 1 && after[1] == after[0] + 1,
          "nach verworfenen Blöcken: frame_id übersprungen (Lücke erkennbar), danach fortlaufend");

    // 2) Frame_Assembler: nach der Lücke kein Frame aus [letzter gehaltener | erster neuer]
    // held: je Hop ein Frame (Fenster lief durch), after: erster Hop füllt neu, zweiter liefert den Frame
    check(frames == static_cast<int>(nHeld) + static_cast<int>(after.size()) - 1, "Frame_Assembler beginnt nach der Lücke neu (kein Frame über die Lücke)");

    // 4) Befund 42
    const size_t stride = sizeof(MicFrame::q[0]);
    std::printf("MicFrame-Kanalabstand %zu Byte (mod 1024 = %zu)\n", stride, stride % 1024);
    check(stride % 1024 != 0 && stride >= sizeof(int16_t) * HOP_SAMPLES, "Kanalabstand kein Vielfaches von 1 KB");
    return g_fail;
}
