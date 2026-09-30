// Host-Stub: nur was SDRAMSelfTest braucht
#pragma once
#include <cstdint>
typedef enum { HAL_SDRAM_STATE_RESET = 0, HAL_SDRAM_STATE_READY = 1, HAL_SDRAM_STATE_BUSY = 2 } HAL_SDRAM_StateTypeDef;
typedef struct { HAL_SDRAM_StateTypeDef State; } SDRAM_HandleTypeDef;
extern int g_flushes;
inline void SCB_CleanInvalidateDCache(void) { ++g_flushes; }
