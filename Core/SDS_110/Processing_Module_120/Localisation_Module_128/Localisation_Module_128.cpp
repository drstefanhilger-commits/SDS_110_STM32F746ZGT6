/*
 * Localisation_Module_128.cpp
 */
#include "Localisation_Module_128.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"
#include <cmath>

namespace sds110 {

void Localisation_Module_128::init(const Vec3* pos, uint32_t n)
{
    numUnits_ = (n > 8) ? 8 : n;
    ref_ = {0, 0, 0};
    for (uint32_t i = 0; i < numUnits_; ++i) { units_[i] = pos[i]; ref_.x += pos[i].x; ref_.y += pos[i].y; ref_.z += pos[i].z; }
    if (numUnits_) { ref_.x /= numUnits_; ref_.y /= numUnits_; ref_.z /= numUnits_; }
}

float Localisation_Module_128::calibrate(float az) const
{
    az = (az + azOffset_) * azScale_;
    while (az < 0.0f)    az += 360.0f;
    while (az >= 360.0f) az -= 360.0f;
    return az;
}

// ---------------------------------------------------------------- Abschnitt 6
bool Localisation_Module_128::solve(const TdoaMeasurement* t, uint32_t n, CandidateLocation& out)
{
    out = CandidateLocation{};
    uint32_t valid = 0;
    for (uint32_t k = 0; k < n; ++k) if (t[k].valid) ++valid;
    if (valid < MIN_PAIRS || numUnits_ < 3) return false;

    // Start: Zentroid + Richtung des stärksten Paars, 50 m
    float x = ref_.x, y = ref_.y + 50.0f;
    for (uint32_t it = 0; it < LS_ITERATIONS; ++it) {
        float H00 = 0, H01 = 0, H11 = 0, g0 = 0, g1 = 0, res = 0;
        for (uint32_t k = 0; k < n; ++k) {
            if (!t[k].valid) continue;
            const Vec3& ui = units_[t[k].i]; const Vec3& uj = units_[t[k].j];
            const float dix = x - ui.x, diy = y - ui.y, djx = x - uj.x, djy = y - uj.y;
            const float ri = std::sqrt(dix*dix + diy*diy) + 1e-6f, rj = std::sqrt(djx*djx + djy*djy) + 1e-6f;
            const float f  = (ri - rj) - c_ * t[k].tdoa_s;      // Residuum (m)
            const float jx = dix / ri - djx / rj, jy = diy / ri - djy / rj;  // Jacobi
            const float w  = t[k].peak_ratio;
            H00 += w * jx * jx; H01 += w * jx * jy; H11 += w * jy * jy;
            g0  += w * jx * f;  g1  += w * jy * f;  res += w * f * f;
        }
        const float det = H00 * H11 - H01 * H01;
        if (std::fabs(det) < 1e-12f) return false;
        const float dx = -( H11 * g0 - H01 * g1) / det;
        const float dy = -(-H01 * g0 + H00 * g1) / det;
        x += dx; y += dy;
        out.ls_residual = std::sqrt(res / valid);
        if (std::fabs(dx) + std::fabs(dy) < 1e-3f) break;
    }
    const float rx = x - ref_.x, ry = y - ref_.y;
    out.azimuth_deg    = calibrate(Azimuth::fromArray(rx, ry));   // 0° = Nord, im Uhrzeigersinn
    out.distance_m     = std::sqrt(rx * rx + ry * ry);
    out.accepted_pairs = static_cast<uint8_t>(valid);
    // Residuum in m -> Samples: r / c · fs
    out.confidence     = candidateConfidence(valid, n, out.ls_residual / c_ * SAMPLE_RATE_HZ);
    out.valid = true;
    return true;
}

// ---------------------------------------------------------------- Einzel-Unit
bool Localisation_Module_128::fromBearing(const Bearing& b, float levelA, CandidateLocation& out)
{
    out = CandidateLocation{};
    if (!b.valid) return false;
    out.azimuth_deg    = calibrate(b.azimuth_deg);
    out.accepted_pairs = b.valid_pairs;
    out.ls_residual    = b.residual;
    out.confidence     = candidateConfidence(b.valid_pairs, NUM_MIC_PAIRS, b.residual * SAMPLE_RATE_HZ);
    out.distance_m     = (SINGLE_UNIT_LEVEL_DISTANCE && levelA > 0.0f) ? LEVEL_DIST_K_REF / (levelA + LEVEL_DIST_EPS) : 0.0f;
    out.valid = true;
    return true;
}

} // namespace sds110
