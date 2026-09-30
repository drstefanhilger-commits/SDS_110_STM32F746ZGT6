/*
 * Logger.hpp
 *
 *  Created on: Aug 30, 2026
 *      Author: 310004
 */

#pragma once
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <atomic>

/*
 * Ringpuffer für Logtexte: beliebig viele Schreiber (Tasks und ISRs), ein Leser (LoggerTask).
 *  - write() formatiert außerhalb der Sperre (max. MAX_MSG Zeichen) und fügt die Meldung
 *    in einem kurzen kritischen Abschnitt (Interrupts gesperrt) ganz oder gar nicht ein.
 *  - Reicht der Platz nicht, wird die Meldung verworfen und gezählt (dropped()); ungelesene
 *    Daten werden nie überschrieben.
 */
class Logger
{
public:
    static constexpr uint32_t LOG_BUFFER_SIZE = 4096;
    static constexpr uint32_t MAX_MSG = 255;       // Zeichen je Meldung (ohne Nullbyte)

    static Logger& instance();

    void write(const char* fmt, ...);
    int read(uint8_t* dst, int maxLen);
    uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    Logger();
    Logger(const Logger&) = delete;

private:
    uint8_t buffer[LOG_BUFFER_SIZE];
    std::atomic<uint32_t> head;
    std::atomic<uint32_t> tail;
    std::atomic<uint32_t> dropped_{0};
};
