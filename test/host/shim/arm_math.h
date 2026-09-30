// Host-Shim: minimale CMSIS-DSP-Teilmenge für den SDS_110-Host-Test
#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <vector>
typedef float float32_t;
#ifndef PI
#define PI 3.14159265358979f
#endif
struct arm_rfft_fast_instance_f32 { uint16_t fftLen; };
inline void arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32* s, uint16_t n) { s->fftLen = n; }
void arm_rfft_fast_f32(const arm_rfft_fast_instance_f32* s, float* p, float* pOut, uint8_t ifftFlag);
inline void arm_mult_f32(const float* a, const float* b, float* d, uint32_t n) { for (uint32_t i = 0; i < n; ++i) d[i] = a[i] * b[i]; }
inline void arm_dot_prod_f32(const float* a, const float* b, uint32_t n, float* r) { float s = 0; for (uint32_t i = 0; i < n; ++i) s += a[i] * b[i]; *r = s; }
inline void arm_rms_f32(const float* a, uint32_t n, float* r) { double s = 0; for (uint32_t i = 0; i < n; ++i) s += a[i] * a[i]; *r = (float)std::sqrt(s / n); }
inline void arm_scale_f32(const float* a, float k, float* d, uint32_t n) { for (uint32_t i = 0; i < n; ++i) d[i] = a[i] * k; }
struct arm_biquad_cascade_df2T_instance_f32 { uint8_t numStages; float* pState; const float* pCoeffs; };
inline void arm_biquad_cascade_df2T_init_f32(arm_biquad_cascade_df2T_instance_f32* S, uint8_t n, const float* c, float* st)
{ S->numStages = n; S->pCoeffs = c; S->pState = st; std::memset(st, 0, sizeof(float) * 2 * n); }
inline void arm_biquad_cascade_df2T_f32(const arm_biquad_cascade_df2T_instance_f32* S, const float* in, float* out, uint32_t n)
{
    const float* src = in;
    for (uint8_t st = 0; st < S->numStages; ++st) {
        const float* c = S->pCoeffs + 5 * st; float* z = S->pState + 2 * st;
        for (uint32_t i = 0; i < n; ++i) {
            const float x = src[i]; const float y = c[0] * x + z[0];
            z[0] = c[1] * x + c[3] * y + z[1];
            z[1] = c[2] * x + c[4] * y;
            out[i] = y;
        }
        src = out;
    }
}
