/*
 * Logger.cpp
 *
 *  Created on: Aug 30, 2026
 *      Author: 310004
 */

#include "Logger.hpp"
#include "stm32f7xx.h"   // __disable_irq / PRIMASK

Logger& Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::Logger()
{
    head = 0;
    tail = 0;
}

void Logger::write(const char* fmt, ...)
{
    char temp[MAX_MSG + 1];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(temp, sizeof(temp), fmt, args);   // liefert die ungekürzte Länge
    va_end(args);

    if (len <= 0) return;
    const uint32_t n = (static_cast<uint32_t>(len) > MAX_MSG) ? MAX_MSG : static_cast<uint32_t>(len);

    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t h = head.load(std::memory_order_relaxed);
    const uint32_t t = tail.load(std::memory_order_acquire);
    const uint32_t used = (h + LOG_BUFFER_SIZE - t) % LOG_BUFFER_SIZE;
    if (n > LOG_BUFFER_SIZE - 1 - used) {                // passt nicht: ganz verwerfen
        __set_PRIMASK(primask);
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    for (uint32_t i = 0; i < n; i++)
        buffer[(h + i) % LOG_BUFFER_SIZE] = static_cast<uint8_t>(temp[i]);
    head.store((h + n) % LOG_BUFFER_SIZE, std::memory_order_release);
    __set_PRIMASK(primask);
}

int Logger::read(uint8_t* dst, int maxLen)
{
    uint32_t t = tail.load(std::memory_order_relaxed);
    uint32_t h = head.load(std::memory_order_acquire);

    if (t == h)
        return 0;

    int count = 0;

    while (t != h && count < maxLen)
    {
        dst[count++] = buffer[t];
        t = (t + 1) % LOG_BUFFER_SIZE;
    }

    tail.store(t, std::memory_order_release);
    return count;
}

