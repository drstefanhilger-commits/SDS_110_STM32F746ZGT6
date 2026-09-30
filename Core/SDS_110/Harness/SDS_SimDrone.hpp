/*
 * SDS_SimDrone.hpp
 *
 *  Created on: Sep 22, 2026
 *      Author: 310004
 */

#pragma once
#include <vector>
#include <cstdint>
#include <cmath>

class SDS_SimDrone
{
public:
    SDS_SimDrone(float sampleRate);

    std::vector<float> synthesizeParrotDrone(
        float duration,
        float f0 = 118.0f,
        uint32_t nHarmonics = 15,
        float baseMag = 2.2f,
        float decayMag = 1.2f,
        float roughness = 1.47f,
        float amDepth = 0.48f,
        float fmDepth = 0.15f,
        float noiseLevel = 0.15f,
        float jitterAmount = 0.068f,
        float driftAmount = 0.05f,
        float windNoise = 0.023f
    );

private:
    float fs;

    float randn();
};
