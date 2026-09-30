/*
 * crc32.hpp  (Infrastructure/Utils) – unverändert aus SDS/Utils/crc32.hpp, Namespace sds110
 * Reflektiertes CRC32 (Poly 0xEDB88320), kompatibel zum PC-Monitor (Python zlib.crc32).
 */
#pragma once
#include <cstdint>
#include <cstddef>

namespace sds110 {

class CRC32 {
public:
    static uint32_t computeCRC32(const uint8_t* data, size_t len)
    {
        constexpr uint32_t poly = 0xEDB88320;
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; bit++)
                crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
        }
        return crc ^ 0xFFFFFFFF;
    }
    template<typename T> static uint32_t compute(const T& msg)
    { return computeCRC32(reinterpret_cast<const uint8_t*>(&msg), sizeof(T)); }
    template<typename T> static uint32_t compute_no_crc(const T& msg)
    { return computeCRC32(reinterpret_cast<const uint8_t*>(&msg), sizeof(T) - sizeof(uint32_t)); }
};

} // namespace sds110
