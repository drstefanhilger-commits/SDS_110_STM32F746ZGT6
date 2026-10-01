/*
 * UartSelfTestCore.hpp  (Infrastructure/Utils)
 *
 * Logik des Selbsttests der PC-Verbindung über USART1/CP2102N (SDS110_UART_SELFTEST,
 * SDS_110_Board.h; Hardware-Schleife in Infrastructure/Driver/UartSelfTest.cpp).
 *
 *   - Antworten sind Logger-Nachrichten Id 99 im ICD-Format (doc/ICD_SDS_PC_Monitor.md 5.4), damit
 *     PC-Monitor und test/pc/uart_link_test.py sie ohne Sondermodus lesen
 *   - jedes empfangene Kommando (CommandAssembler: Magic, Länge, Rest über Paketgrenzen) wird mit
 *     "ECHO id=<id> len=<len> crc=OK|BAD c=<CRC hex>" beantwortet; die CRC der Kommandos wird hier
 *     geprüft (in der Firmware noch nicht, Befund 12)
 *   - verworfene Bytes (Müll, falsche Länge, veralteter Rest) -> "ERR n=<anzahl> <hex>"
 *   - Herzschlag "UART-SELFTEST t=<ms> rx=<bytes> cmd=<n> bad=<n> err=<n> ore=<n> fe=<n> drop=<n>"
 * Reine Logik ohne Hardware (Host-Test t_uart_selftest).
 */
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "Infrastructure/Utils/CommandAssembler.hpp"

namespace sds110 {

class UartSelfTestCore {
public:
    static constexpr uint32_t MSG_LEN  = 144;                // Logger-Nachricht (ICD 5.4)
    static constexpr uint32_t TEXT_LEN = 128;
    static constexpr uint8_t  LOG_ID   = 99;

    UartSelfTestCore() { initCrcTable(); }

    /// Logger-Nachricht Id 99: Magic, len_id, timestamp 0, Text mit Nullbytes, CRC32 (alles LE)
    void buildLog(uint8_t out[MSG_LEN], const char* text) const
    {
        std::memset(out, 0, MSG_LEN);
        putLe32(out + 0, 0xDEADBEEFu);
        putLe32(out + 4, (static_cast<uint32_t>(LOG_ID) << 24) | MSG_LEN);
        const size_t n = std::strlen(text);
        std::memcpy(out + 12, text, (n < TEXT_LEN) ? n : TEXT_LEN);
        putLe32(out + MSG_LEN - 4, crc32(out, MSG_LEN - 4));
    }

    /**
     * Empfangene Bytes auswerten; reply(text) wird für jedes Kommando und jeden Fehler gerufen.
     * rxUs = Laufzeit in µs (für den 20-ms-Rest-Timeout des CommandAssembler).
     */
    template<class Reply>
    void onRx(const uint8_t* data, uint32_t n, uint64_t rxUs, Reply&& reply)
    {
        rxBytes_ += n;
        asm_.push(data, n, rxUs);
        const uint8_t* p;
        uint32_t len;
        char text[TEXT_LEN];
        for (;;) {
            const auto r = asm_.next(p, len);
            if (r == CommandAssembler::Result::None) break;
            if (r == CommandAssembler::Result::Error) {
                ++errors_;
                int k = std::snprintf(text, sizeof(text), "ERR n=%lu", static_cast<unsigned long>(len));
                for (uint32_t i = 0; i < len && k > 0 && k + 4 < static_cast<int>(sizeof(text)); ++i)
                    k += std::snprintf(text + k, sizeof(text) - k, " %02X", p[i]);
                reply(text);
                continue;
            }
            ++commands_;
            const uint32_t got = (static_cast<uint32_t>(p[len - 4]) << 24) | (p[len - 3] << 16) |
                                 (p[len - 2] << 8) | p[len - 1];             // CRC des PC: big-endian
            const bool ok = (got == crc32(p, len - 4));
            if (!ok) ++crcBad_;
            std::snprintf(text, sizeof(text), "ECHO id=%u len=%lu crc=%s c=%08lX", p[4],
                          static_cast<unsigned long>(len), ok ? "OK" : "BAD", static_cast<unsigned long>(got));
            reply(text);
        }
    }

    /// Text des Herzschlags; ore/fe/drop zählt die Hardware-Schleife
    void heartbeat(char* text, size_t size, uint32_t ms, uint32_t ore, uint32_t fe, uint32_t drop) const
    {
        std::snprintf(text, size, "UART-SELFTEST t=%lu rx=%lu cmd=%lu bad=%lu err=%lu ore=%lu fe=%lu drop=%lu",
                      static_cast<unsigned long>(ms), static_cast<unsigned long>(rxBytes_),
                      static_cast<unsigned long>(commands_), static_cast<unsigned long>(crcBad_),
                      static_cast<unsigned long>(errors_), static_cast<unsigned long>(ore),
                      static_cast<unsigned long>(fe), static_cast<unsigned long>(drop));
    }

    /// Tabellen-CRC32 (wie CRC32::computeCRC32, zlib.crc32): kurz genug, um zwischen zwei
    /// empfangenen Bytes (10,8 µs bei 921600 Baud) eine Antwort zu bauen
    uint32_t crc32(const uint8_t* d, uint32_t n) const
    {
        uint32_t c = 0xFFFFFFFFu;
        for (uint32_t i = 0; i < n; ++i) c = table_[(c ^ d[i]) & 0xFFu] ^ (c >> 8);
        return c ^ 0xFFFFFFFFu;
    }

    uint32_t rxBytes() const { return rxBytes_; }
    uint32_t commands() const { return commands_; }
    uint32_t crcBad() const { return crcBad_; }
    uint32_t errors() const { return errors_; }

private:
    static void putLe32(uint8_t* p, uint32_t v)
    {
        p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8);
        p[2] = static_cast<uint8_t>(v >> 16); p[3] = static_cast<uint8_t>(v >> 24);
    }
    void initCrcTable()
    {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int b = 0; b < 8; ++b) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);
            table_[i] = c;
        }
    }

    CommandAssembler asm_;
    uint32_t table_[256] = {};
    uint32_t rxBytes_ = 0, commands_ = 0, crcBad_ = 0, errors_ = 0;
};

} // namespace sds110
