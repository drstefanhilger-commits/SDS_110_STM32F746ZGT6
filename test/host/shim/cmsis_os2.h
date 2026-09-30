// Host-Shim: CMSIS-RTOS2-Stubs (einfädig)
#pragma once
#include <cstdint>
typedef void* osMutexId_t;
struct osMutexAttr_t { const char* name; uint32_t attr_bits; void* cb_mem; uint32_t cb_size; };
typedef int osStatus_t;
constexpr osStatus_t osOK = 0;
constexpr uint32_t osWaitForever = 0xFFFFFFFFu;
inline osMutexId_t osMutexNew(const osMutexAttr_t*) { static int m; return &m; }
// Fehlerinjektion für Host-Tests (t_sds_data): die nächsten n Acquire liefern osErrorTimeout
inline int g_osMutexFailNext = 0;
inline int g_osMutexAcquireCalls = 0;
constexpr osStatus_t osErrorTimeout = -2;
constexpr uint32_t osMutexPrioInherit = 0x00000002U;
inline osStatus_t osMutexAcquire(osMutexId_t, uint32_t) { ++g_osMutexAcquireCalls; if (g_osMutexFailNext > 0) { --g_osMutexFailNext; return osErrorTimeout; } return osOK; }
inline osStatus_t osMutexRelease(osMutexId_t) { return osOK; }
inline uint32_t osKernelGetTickCount() { return 0; }
typedef void* osMessageQueueId_t;
struct osMessageQueueAttr_t { const char* name; uint32_t attr_bits; void* cb_mem; uint32_t cb_size; void* mq_mem; uint32_t mq_size; };
// Message-Queues: nur für SDS_Data (Host-Test t_sds_data); Put verwirft, Get liefert nichts
constexpr osStatus_t osErrorResource = -3;
inline osMessageQueueId_t osMessageQueueNew(uint32_t, uint32_t, const osMessageQueueAttr_t*) { static int q; return &q; }
inline osStatus_t osMessageQueuePut(osMessageQueueId_t, const void*, uint8_t, uint32_t) { return osOK; }
inline osStatus_t osMessageQueueGet(osMessageQueueId_t, void*, uint8_t*, uint32_t) { return osErrorResource; }
