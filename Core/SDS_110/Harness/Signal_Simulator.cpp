/*
 * Signal_Simulator.cpp
 */
#include "Infrastructure/Utils/DspOptimize.hpp"   // zuerst: -O2 auf dem Board
#include "Signal_Simulator.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"
#include <cmath>
#include <cstring>

namespace sds110 {

namespace {
// ~N(0,1) als Summe von 4 Gleichverteilten (Irwin-Hall, Varianz 1, Grenzen ±3,46σ): 2 Schritte
// xorshift32 (alle Bits gleich gut, anders als die unteren Bits eines LCG) liefern 4 × 16 Bit,
// Summe als Ganzzahl, eine Wandlung. Vorher Box-Muller, danach 4 LCG-Schritte mit je einer Wandlung.
SDS110_FORCE_INLINE float gauss(uint32_t& state)
{
    uint32_t x = state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    const uint32_t a = x;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    state = x;
    const uint32_t sum = (a & 0xFFFFu) + (a >> 16) + (x & 0xFFFFu) + (x >> 16);   // 0 … 4·65535
    constexpr float k = 1.7320508f / 65536.0f;            // sqrt(12/4) / 2^16
    return static_cast<float>(sum) * k - 2.0f * 1.7320508f;
}

// Zeiger um einen Schritt drehen; Im = sin(Phase) vor dem Schritt
SDS110_FORCE_INLINE void rotate(float& re, float& im, float sr, float si)
{
    const float r = re * sr - im * si; im = re * si + im * sr; re = r;
}
} // namespace

Signal_Simulator& Signal_Simulator::instance() { static Signal_Simulator inst; return inst; }

void Signal_Simulator::init(const SimParams& p)
{
    p_ = p;
    primed_ = false;
    for (uint32_t h = 0; h < MAX_HARM; ++h) { oscRe_[h] = 1.0f; oscIm_[h] = 0.0f; }   // Phase 0
    amRe_ = 1.0f; amIm_ = 0.0f;
    std::memset(pinkState_, 0, sizeof(pinkState_));
    active_ = true; flyT_ = 0.0f; flyCycle_ = 0;
}

float Signal_Simulator::noise() { return gauss(rng_); }

void Signal_Simulator::updateOscillators()
{
    constexpr float twoPi = 6.2831853f;
    const float fs = static_cast<float>(SAMPLE_RATE_HZ);
    for (uint32_t h = 0; h < MAX_HARM; ++h) {
        const float d = twoPi * p_.f0_hz * static_cast<float>(h + 1) / fs;
        stepRe_[h] = std::cos(d); stepIm_[h] = std::sin(d);
        const float r = 1.0f / std::sqrt(oscRe_[h] * oscRe_[h] + oscIm_[h] * oscIm_[h]);
        oscRe_[h] *= r; oscIm_[h] *= r;                   // Rundungsdrift je Hop zurücksetzen
    }
    const float d = twoPi * p_.bpf_mod_hz / fs;
    amStepRe_ = std::cos(d); amStepIm_ = std::sin(d);
    const float r = 1.0f / std::sqrt(amRe_ * amRe_ + amIm_ * amIm_);
    amRe_ *= r; amIm_ *= r;
}

void Signal_Simulator::advanceSweep()
{
    p_.azimuth_deg += p_.sweep_az_step;
    if (p_.azimuth_deg >= 360.0f) {
        p_.azimuth_deg -= 360.0f;
        p_.distance_m += p_.sweep_dist_step;
        if (p_.distance_m > 100.0f) p_.distance_m = 20.0f;
    }
}

bool Signal_Simulator::flyByPosition(const SimParams& p, float t, uint32_t cycle, const float unitENU[3],
                                     float& azDeg, float& distM)
{
    if (t >= p.flyby_flight_s) return false;
    constexpr float d2r = 3.14159265f / 180.0f;
    // Lokales System Nord/Ost: Richtung d der Bahn, kürzester Abstand zum Ursprung links der Bahn
    // (Ost-Kurs: nördlich des Ursprungs)
    const float tr = (p.flyby_track_deg + static_cast<float>(cycle) * p.flyby_track_step_deg) * d2r;
    const float dn = std::cos(tr), de = std::sin(tr);
    const float s  = p.flyby_speed_mps * (t - 0.5f * p.flyby_flight_s);   // Weg ab dem kürzesten Abstand
    const float n  =  de * p.flyby_cpa_m + dn * s - unitENU[1];         // relativ zur Einheit
    const float e  = -dn * p.flyby_cpa_m + de * s - unitENU[0];
    const float u  =  p.flyby_alt_m - unitENU[2];
    azDeg = Azimuth::wrap360(std::atan2(e, n) / d2r);                   // Richtung in der Ebene
    distM = std::sqrt(n * n + e * e + u * u);
    return true;
}

// FlyBy: Position für den nächsten Hop (Zeit am Hop-Anfang), dann Zeit weiterschalten
void Signal_Simulator::advanceFlyBy()
{
    float az, dist;
    active_ = flyByPosition(p_, flyT_, flyCycle_, unit_, az, dist);
    if (active_) { p_.azimuth_deg = az; p_.distance_m = dist; }
    flyT_ += HOP_S;
    const float period = p_.flyby_flight_s + p_.flyby_pause_s;
    if (period > 0.0f && flyT_ >= period) { flyT_ -= period; ++flyCycle_; }
}

// Ein Quellsample (phasenkontinuierlich), Amplitude ohne 1/r
float Signal_Simulator::sourceSample()
{
    float s = 0.0f;
    switch (p_.scenario) {
    case SimScenario::DroneSweep:
    case SimScenario::DroneStatic:
    case SimScenario::FlyBy: {
        // Harmonische mit fallender Amplitude (1/h) und Blattpass-AM
        const float am = 1.0f + 0.5f * amIm_;
        rotate(amRe_, amIm_, amStepRe_, amStepIm_);
        for (uint8_t h = 0; h < p_.harmonics && h < MAX_HARM; ++h) {
            s += oscIm_[h] / static_cast<float>(h + 1);
            rotate(oscRe_[h], oscIm_[h], stepRe_[h], stepIm_[h]);
        }
        s *= am * 0.5f;
        break; }
    case SimScenario::SingleTone:
        s = oscIm_[0];
        rotate(oscRe_[0], oscIm_[0], stepRe_[0], stepIm_[0]);
        break;
    case SimScenario::WindNoise: {
        // 1/f-Näherung (Paul-Kellet-Filter, 3 Pole)
        const float w = noise();
        pinkState_[0] = 0.99765f * pinkState_[0] + w * 0.0990460f;
        pinkState_[1] = 0.96300f * pinkState_[1] + w * 0.2965164f;
        pinkState_[2] = 0.57000f * pinkState_[2] + w * 1.0526913f;
        s = (pinkState_[0] + pinkState_[1] + pinkState_[2] + w * 0.1848f) * 0.3f;
        break; }
    case SimScenario::Silence:
    default:
        s = 0.0f;
        break;
    }
    return s;
}

void Signal_Simulator::generateHop(uint64_t time_utc_us)
{
    if (p_.scenario == SimScenario::DroneSweep) advanceSweep();
    if (p_.scenario == SimScenario::FlyBy)      advanceFlyBy();
    else                                        active_ = true;

    // --- Fernfeld-Verzögerung je Mikrofon: τ_m = -(p_m · u) / c ---
    // azimuth_deg: 0° = Nord, im Uhrzeigersinn (Azimuth.hpp) -> Richtung u im Array-System
    float ux, uy;
    Azimuth::toArray(p_.azimuth_deg, ux, uy);
    for (uint32_t m = 0; m < NUM_MICS; ++m) {
        const Vec3& pm = array_.position(m);
        delaySamples_[m] = -(pm.x * ux + pm.y * uy) / c_ * SAMPLE_RATE_HZ;
    }

    // --- Quellsignal mit Vorlauf ---
    float amp = p_.source_level / (p_.distance_m > 1.0f ? p_.distance_m : 1.0f);   // 1/r
    float noiseAmp = amp * std::pow(10.0f, -p_.snr_db / 20.0f);
    if (p_.scenario == SimScenario::FlyBy) {
        // Rauschen fest (snr_db am kürzesten Abstand), Quelle in der Pause stumm
        noiseAmp = p_.source_level / (p_.flyby_cpa_m > 1.0f ? p_.flyby_cpa_m : 1.0f) * std::pow(10.0f, -p_.snr_db / 20.0f);
        if (!active_) amp = 0.0f;
    }

    // Nur neue Samples erzeugen: beim ersten Aufruf das ganze Fenster, danach um HOP_SAMPLES schieben
    updateOscillators();
    uint32_t first = 0;
    if (primed_) { std::memmove(src_, src_ + HOP_SAMPLES, sizeof(float) * 2 * GUARD); first = 2 * GUARD; }
    for (uint32_t n = first; n < HOP_SAMPLES + 2 * GUARD; ++n) src_[n] = sourceSample() * amp;
    primed_ = true;

    // --- pro Mikrofon: verzögert + eigenes Rauschen, blockweise wie der DMA ---
    // Die Verzögerung ist je Hop konstant: Ganzzahl-Anteil und Bruch einmal je Mikrofon statt je
    // Sample (pos = n + GUARD − τ_m = n + base_m + fr_m, 0 ≤ fr_m < 1). |τ_m| < GUARD, daher
    // liegt i0 = n + base_m immer im Puffer.
    // Format wie die Hardware (116): 24-bit PCM linksbündig im 32-bit-Slot (pcm24 << 8)
    constexpr float toPcm24 = static_cast<float>(1 << 23);
    int   base[NUM_MICS];
    float w0[NUM_MICS], w1[NUM_MICS];
    for (uint32_t m = 0; m < NUM_MICS; ++m) {
        const float p = static_cast<float>(GUARD) - delaySamples_[m];
        base[m] = static_cast<int>(std::floor(p));
        w1[m] = p - static_cast<float>(base[m]);
        w0[m] = 1.0f - w1[m];
    }
    // innere Schleife ohne Funktionsaufrufe (auch im Debug-Build, siehe DspOptimize.hpp)
    uint32_t rng = rng_;
    for (uint32_t b0 = 0; b0 < HOP_SAMPLES; b0 += DMA_BLOCK_SAMPLES) {
        for (uint32_t s = 0; s < DMA_BLOCK_SAMPLES; ++s) {
            const uint32_t n = b0 + s;
            int32_t* out = block_ + s * NUM_MICS;
            for (uint32_t m = 0; m < NUM_MICS; ++m) {
                const float* q = src_ + n + base[m];
                float v = q[0] * w0[m] + q[1] * w1[m] + noiseAmp * gauss(rng);
                v = (v > 0.999f) ? 0.999f : ((v < -0.999f) ? -0.999f : v);
                const int32_t pcm24 = static_cast<int32_t>(v * toPcm24);
                out[m] = static_cast<int32_t>(static_cast<uint32_t>(pcm24) << 8);
            }
        }
        rng_ = rng;
        array_.pushBlock(block_, DMA_BLOCK_SAMPLES, time_utc_us + static_cast<uint64_t>(b0) * 1000000ULL / SAMPLE_RATE_HZ);
    }
}

} // namespace sds110
