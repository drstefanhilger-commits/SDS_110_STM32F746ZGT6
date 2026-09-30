/*
 * Microphone_Array_114.cpp
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board (pushBlock je DMA-Block)
#include "Microphone_Array_114.hpp"
#include <cmath>
#include <cstring>

namespace sds110 {

MicFrame Microphone_Array_114::frames_[NUM_MIC_FRAMES];   // internes RAM (BFP int16, ~25 kB je Hop)

// ---------------------------------------------------------------- MicFrame (Blockgleitkomma)
void MicFrame::encode(uint32_t ch, const float* src)
{
    int16_t* dst = q[ch];
    for (uint32_t b = 0; b < BLOCKS_PER_HOP; ++b) {
        const uint32_t i0 = b * DMA_BLOCK_SAMPLES, i1 = i0 + DMA_BLOCK_SAMPLES;
        float m = 0.0f;
        for (uint32_t i = i0; i < i1; ++i) { const float a = std::fabs(src[i]); if (a > m) m = a; }
        int e = 0;
        if (m > 0.0f && std::isfinite(m)) {
            int E; std::frexp(m, &E);                               // m < 2^E
            e = E - 15;                                             // |x| · 2^-e < 2^15
            if (e < -128) e = -128;
            if (e > 127)  e = 127;
        }
        exp[ch][b] = static_cast<int8_t>(e);
        const float inv = std::ldexp(1.0f, -e);
        for (uint32_t i = i0; i < i1; ++i) {
            long v = std::lrint(src[i] * inv);
            dst[i] = static_cast<int16_t>(v > 32767 ? 32767 : (v < -32767 ? -32767 : v));
        }
    }
}

void MicFrame::encodeRawBlock(const int32_t* interleaved, uint32_t idx, uint32_t n)
{
    // Rohwert raw = pcm24 << 8, normiert raw / 2^31 (PCM_RAW_FULL_SCALE): q = raw >> e, exp = e − 31
    static_assert(PCM_RAW_FULL_SCALE == 2147483648.0f, "114: Exponent setzt Normierung 2^-31 voraus");
    const uint32_t b = idx / DMA_BLOCK_SAMPLES;
    for (uint32_t ch = 0; ch < NUM_MICS; ++ch) {
        uint32_t m = 0;
        for (uint32_t s = 0; s < n; ++s) {
            const int32_t r = interleaved[s * NUM_MICS + ch];
            const uint32_t a = r < 0 ? 0u - static_cast<uint32_t>(r) : static_cast<uint32_t>(r);
            if (a > m) m = a;
        }
        int e = 0;
        while ((m >> e) > 32767u) ++e;
        exp[ch][b] = static_cast<int8_t>(e - 31);
        int16_t* dst = q[ch] + idx;
        for (uint32_t s = 0; s < n; ++s) dst[s] = static_cast<int16_t>(interleaved[s * NUM_MICS + ch] >> e);
    }
}

Microphone_Array_114& Microphone_Array_114::instance()
{
    static Microphone_Array_114 inst;
    return inst;
}

Microphone_Array_114::Microphone_Array_114()
{
    // Oktagon: Mic m bei Winkel m * 45°, gegen den Uhrzeigersinn ab +x
    for (uint32_t m = 0; m < NUM_MICS; ++m) {
        const float a = static_cast<float>(m) * (2.0f * 3.14159265f / NUM_MICS);
        pos_[m] = { MIC_RADIUS_M * std::cos(a), MIC_RADIUS_M * std::sin(a), 0.0f };
    }

    osMutexAttr_t attr{};
    attr.name = "Mic114Mutex";
    mutex_ = osMutexNew(&attr);

    for (auto& f : frames_) {
        f.state = FrameState::Free;
        f.writeIndex = 0;
        f.frame_id = 0;
        f.time_utc_us = 0;
        f.processed = false;
        std::memset(f.q, 0, sizeof(f.q));
        std::memset(f.exp, 0, sizeof(f.exp));
    }
    active_ = acquireFree();
}

MicFrame* Microphone_Array_114::acquireFree()
{
    for (auto& f : frames_) {
        if (f.state == FrameState::Free) {
            f.state = FrameState::Writing;
            f.writeIndex = 0;
            f.processed = false;
            f.frame_id = nextId_++;
            return &f;
        }
    }
    return nullptr;
}

void Microphone_Array_114::pushBlock(const int32_t* interleaved, uint32_t samplesPerMic, uint64_t time_utc_us)
{
    MicFrame* f = active_;
    if (!f) {
        // Kein freier Puffer: Block verwerfen, später erneut versuchen. Befund 27: beim ersten
        // verworfenen Block eine frame_id überspringen, damit der Frame_Assembler (und der
        // PC-Monitor im Modus READ) die Lücke erkennt und keine zeitlich getrennten Hops verbindet.
        if (!dropping_) { ++nextId_; dropping_ = true; }
        active_ = acquireFree();
        ++dropped_;
        return;
    }
    dropping_ = false;
    if (f->writeIndex == 0) f->time_utc_us = time_utc_us;

    // Blockgleitkomma: je Aufruf genau ein DMA-Block (116: DMA_BLOCK_SAMPLES, Simulator ebenso)
    uint32_t idx = f->writeIndex;
    if (samplesPerMic != DMA_BLOCK_SAMPLES || idx % DMA_BLOCK_SAMPLES != 0) { ++dropped_; return; }
    if (idx < HOP_SAMPLES) {
        f->encodeRawBlock(interleaved, idx, samplesPerMic);
        idx += samplesPerMic;
    }
    f->writeIndex = idx;

    if (idx >= HOP_SAMPLES) {
        f->state = FrameState::Ready;
        latest_  = f;
        active_  = acquireFree();          // nullptr -> nächster Block wird verworfen
    }
}

MicFrame* Microphone_Array_114::acquireReadable()
{
    osMutexAcquire(mutex_, osWaitForever);
    MicFrame* res = nullptr;
    // ältesten READY-Frame nehmen
    for (auto& f : frames_) {
        if (f.state == FrameState::Ready && (!res || f.frame_id < res->frame_id))
            res = &f;
    }
    if (res) res->state = FrameState::Reading;
    osMutexRelease(mutex_);
    return res;
}

void Microphone_Array_114::release(MicFrame* f)
{
    if (!f) return;
    osMutexAcquire(mutex_, osWaitForever);
    f->state = FrameState::Free;
    f->writeIndex = 0;
    osMutexRelease(mutex_);
}

} // namespace sds110
