#include "SDS_SimDrone.hpp"

SDS_SimDrone::SDS_SimDrone(float sampleRate)
    : fs(sampleRate)
{
}

float SDS_SimDrone::randn()
{
    // Simple deterministic Gaussian-like noise
    static uint32_t seed = 1234567;
    seed = seed * 1103515245 + 12345;
    float u = ((seed >> 16) & 0x7FFF) / 32767.0f;
    float v = ((seed >>  0) & 0x7FFF) / 32767.0f;
    return std::sqrt(-2.0f * std::log(u + 1e-12f)) * std::cos(2.0f * M_PI * v);
}

std::vector<float> SDS_SimDrone::synthesizeParrotDrone(
    float duration,
    float f0,
    uint32_t nHarmonics,
    float baseMag,
    float decayMag,
    float roughness,
    float amDepth,
    float fmDepth,
    float noiseLevel,
    float jitterAmount,
    float driftAmount,
    float windNoise
)
{
    uint32_t N = static_cast<uint32_t>(fs * duration);
    std::vector<float> signal(N, 0.0f);

    // Time vector
    std::vector<float> t(N);
    for (uint32_t i = 0; i < N; i++)
        t[i] = static_cast<float>(i) / fs;

    // Drift modulation
    std::vector<float> drift(N);
    for (uint32_t i = 0; i < N; i++)
        drift[i] = 1.0f + driftAmount * std::sin(2.0f * M_PI * 0.12f * t[i]);

    // Main synthesis loop
    for (uint32_t k = 1; k <= nHarmonics; k++)
    {
        float fk = f0 * k;

        for (uint32_t i = 0; i < N; i++)
        {
            float am = 1.0f + amDepth * std::sin(2.0f * M_PI * f0 * t[i]);
            float fm = fk * (1.0f + fmDepth * std::sin(2.0f * M_PI * 0.8f * t[i]));

            float jitter = 1.0f + jitterAmount * randn();
            float fkEff = fm * jitter * drift[i];

            float ak = baseMag / static_cast<float>(k);

            signal[i] += ak * am * std::sin(2.0f * M_PI * fkEff * t[i]);
        }
    }

    // Filtered noise (AR-Filter)
    std::vector<float> noise(N);
    noise[0] = randn();
    float alpha = 0.2f;

    for (uint32_t i = 1; i < N; i++)
        noise[i] = alpha * noise[i - 1] + (1.0f - alpha) * randn();

    for (uint32_t i = 0; i < N; i++)
        signal[i] += decayMag * roughness * noiseLevel * noise[i];

    // Wind noise
    for (uint32_t i = 0; i < N; i++)
        signal[i] += windNoise * randn();

    // Normalize
    float maxVal = 1e-12f;
    for (float v : signal)
        if (std::fabs(v) > maxVal)
            maxVal = std::fabs(v);

    for (float& v : signal)
        v /= maxVal;

    return signal;
}
