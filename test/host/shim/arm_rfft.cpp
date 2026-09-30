// arm_rfft_fast_f32 über kiss_fft (komplex), inkl. CMSIS-Packing und 1/N bei der IFFT
#include "arm_math.h"
extern "C" {
#include "kiss_fft.h"
}
void arm_rfft_fast_f32(const arm_rfft_fast_instance_f32* s, float* p, float* out, uint8_t inv)
{
    const int N = s->fftLen;
    static std::vector<kiss_fft_cpx> a, b; a.resize(N); b.resize(N);
    static kiss_fft_cfg fwd = nullptr, bwd = nullptr; static int cfgN = 0;
    if (cfgN != N) { fwd = kiss_fft_alloc(N, 0, nullptr, nullptr); bwd = kiss_fft_alloc(N, 1, nullptr, nullptr); cfgN = N; }
    if (!inv) {
        for (int i = 0; i < N; ++i) { a[i].r = p[i]; a[i].i = 0; }
        kiss_fft(fwd, a.data(), b.data());
        out[0] = b[0].r; out[1] = b[N / 2].r;
        for (int k = 1; k < N / 2; ++k) { out[2 * k] = b[k].r; out[2 * k + 1] = b[k].i; }
    } else {
        a[0].r = p[0]; a[0].i = 0; a[N / 2].r = p[1]; a[N / 2].i = 0;
        for (int k = 1; k < N / 2; ++k) { a[k].r = p[2 * k]; a[k].i = p[2 * k + 1]; a[N - k].r = p[2 * k]; a[N - k].i = -p[2 * k + 1]; }
        kiss_fft(bwd, a.data(), b.data());
        for (int i = 0; i < N; ++i) out[i] = b[i].r / N;
    }
}
