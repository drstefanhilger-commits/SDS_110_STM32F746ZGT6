// Host-Stub: PRIMASK-Sperre als globaler Mutex nachgebildet (für Logger-Test)
#pragma once
#include <cstdint>
#include <mutex>
inline std::recursive_mutex& g_irqLock() { static std::recursive_mutex m; return m; }
inline uint32_t __get_PRIMASK() { return 0; }
inline void __disable_irq() { g_irqLock().lock(); }
inline void __set_PRIMASK(uint32_t) { g_irqLock().unlock(); }
