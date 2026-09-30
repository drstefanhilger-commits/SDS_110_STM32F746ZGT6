/*
 * ADAU7118_Registers.hpp
 * Registermap ADAU7118 (8-Kanal PDM -> TDM). Übernommen aus adua7118Driver.c.
 * Werte mit Datenblatt (Analog Devices ADAU7118, Rev. B) abgleichen.
 */
#pragma once
#include <cstdint>

namespace sds110::adau7118 {

enum Reg : uint8_t {
    POWER           = 0x00,
    PLL_CTRL        = 0x01,
    MODE_CTRL       = 0x02,
    DECIMATOR_CTRL  = 0x03,
    CHANNEL_ENABLE  = 0x04,
    LR_SWAP         = 0x05,
    FORMAT          = 0x06,
    MISC            = 0x07,
};

struct RegVal { uint8_t reg; uint8_t val; };

/// Initialsequenz: TDM-8, 32-bit Slots, Slave-Mode (Takt von SAI), HPF an, 0 dB
constexpr RegVal INIT_SEQUENCE[] = {
    { POWER,          0x01 },   // Power-Up
    { PLL_CTRL,       0x00 },   // Slave Mode (PLL aus)
    { MODE_CTRL,      0x30 },   // TDM, 32-bit Slots
    { DECIMATOR_CTRL, 0x00 },   // HPF an, Gain 0 dB
    { CHANNEL_ENABLE, 0xFF },   // alle 8 Kanäle
    { LR_SWAP,        0x00 },
    { FORMAT,         0x01 },   // TDM-Format
    { MISC,           0x00 },
};

} // namespace sds110::adau7118
