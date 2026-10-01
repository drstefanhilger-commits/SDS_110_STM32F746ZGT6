/*
 * EspRomLoader.hpp – Serielles Bootloader-Protokoll des ESP32-C3-ROM (Download-Modus), Teilmenge
 * zum Schreiben des Flash. Ohne Hardware-Abhängigkeit: Transport über EspTransport, damit die
 * Logik auf dem Host gegen ein simuliertes ROM geprüft werden kann (test/test_loader.cpp).
 *
 * Protokoll wie esptool 4.x (loader.py) ohne Stub:
 *   SLIP-Rahmen (0xC0, Escape 0xDB 0xDC / 0xDB 0xDD)
 *   Anfrage:  00 op len16 chk32 daten          (chk nur bei FLASH_DATA: 0xEF ^ alle Datenbytes)
 *   Antwort:  01 op len16 wert32 [daten] status fehler [2 Byte reserviert]
 *   SYNC 0x08, SPI_ATTACH 0x0D, SPI_SET_PARAMS 0x0B, CHANGE_BAUDRATE 0x0F,
 *   FLASH_BEGIN 0x02 (ROM löscht hier; ESP32-C3 erwartet das 5. Wort „verschlüsselt“),
 *   FLASH_DATA 0x03 (Blöcke zu 0x400, mit 0xFF aufgefüllt), SPI_FLASH_MD5 0x13 (32 Hex-Zeichen),
 *   FLASH_END 0x04
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace espflash {

class EspTransport {
public:
    virtual ~EspTransport() = default;
    virtual void write(const uint8_t* data, size_t len) = 0;
    virtual int readByte(uint32_t timeoutMs) = 0;       // 0…255, -1 = Timeout
    virtual void flushInput() = 0;
    virtual void setBaud(uint32_t baud) = 0;
    virtual void delayMs(uint32_t ms) = 0;
    virtual uint32_t millis() = 0;
};

enum class Result : uint8_t {
    Ok,
    Timeout,        // keine passende Antwort
    Status,         // ROM meldet Fehler (status != 0, Fehlercode in lastError())
    ShortResponse,  // Antwort ohne Statusbytes
    Md5Mismatch,    // geschrieben, aber MD5 im Flash weicht ab
};

inline const char* toString(Result r)
{
    switch (r) {
    case Result::Ok:            return "ok";
    case Result::Timeout:       return "Timeout";
    case Result::Status:        return "ROM meldet Fehler";
    case Result::ShortResponse: return "Antwort zu kurz";
    case Result::Md5Mismatch:   return "MD5 weicht ab";
    }
    return "?";
}

class EspRomLoader {
public:
    static constexpr uint8_t  OP_FLASH_BEGIN    = 0x02;
    static constexpr uint8_t  OP_FLASH_DATA     = 0x03;
    static constexpr uint8_t  OP_FLASH_END      = 0x04;
    static constexpr uint8_t  OP_SYNC           = 0x08;
    static constexpr uint8_t  OP_SPI_SET_PARAMS = 0x0B;
    static constexpr uint8_t  OP_SPI_ATTACH     = 0x0D;
    static constexpr uint8_t  OP_CHANGE_BAUD    = 0x0F;
    static constexpr uint8_t  OP_SPI_FLASH_MD5  = 0x13;
    static constexpr uint32_t BLOCK             = 0x400;   // FLASH_WRITE_SIZE des ROM
    static constexpr uint32_t DEFAULT_TIMEOUT   = 3000;    // ms, wie esptool
    static constexpr uint32_t SYNC_TIMEOUT      = 100;
    static constexpr uint32_t ERASE_MS_PER_MB   = 30000;   // ROM löscht in FLASH_BEGIN
    static constexpr uint32_t MD5_MS_PER_MB     = 8000;
    static constexpr int      BLOCK_ATTEMPTS    = 3;

    explicit EspRomLoader(EspTransport& t) : t_(t) {}

    uint8_t lastError() const { return lastError_; }
    uint8_t lastOp() const { return lastOp_; }

    /// SYNC bis zur ersten Antwort, höchstens attempts Versuche; danach die übrigen Antworten verwerfen
    Result sync(int attempts)
    {
        uint8_t d[36] = {0x07, 0x07, 0x12, 0x20};
        std::memset(d + 4, 0x55, 32);
        for (int i = 0; i < attempts; ++i) {
            t_.flushInput();
            uint32_t val = 0;
            if (command(OP_SYNC, d, sizeof(d), 0, SYNC_TIMEOUT, 0, &val, nullptr) == Result::Ok) {
                drain(100);                                    // ROM antwortet auf SYNC mehrfach
                return Result::Ok;
            }
        }
        return Result::Timeout;
    }

    Result spiAttach()
    {
        const uint8_t d[8] = {};                               // hspi_arg 0, is_legacy 0, reserviert
        return command(OP_SPI_ATTACH, d, sizeof(d), 0, DEFAULT_TIMEOUT);
    }

    Result setFlashParams(uint32_t totalSize)
    {
        uint8_t d[24];
        put32(d, 0);                                           // fl_id
        put32(d + 4, totalSize);
        put32(d + 8, 64 * 1024);                               // block
        put32(d + 12, 4 * 1024);                               // sector
        put32(d + 16, 256);                                    // page
        put32(d + 20, 0xFFFF);                                 // status mask
        return command(OP_SPI_SET_PARAMS, d, sizeof(d), 0, DEFAULT_TIMEOUT);
    }

    /// Baudrate wechseln: Antwort kommt noch mit der alten, danach beide Seiten umstellen
    Result changeBaud(uint32_t baud)
    {
        uint8_t d[8];
        put32(d, baud);
        put32(d + 4, 0);                                       // ROM: alte Baudrate 0
        const Result r = command(OP_CHANGE_BAUD, d, sizeof(d), 0, DEFAULT_TIMEOUT);
        if (r != Result::Ok) return r;
        t_.setBaud(baud);
        t_.delayMs(50);
        t_.flushInput();
        return Result::Ok;
    }

    Result flashBegin(uint32_t size, uint32_t offset)
    {
        uint8_t d[20];
        put32(d, size);                                        // erase_size
        put32(d + 4, blocks(size));
        put32(d + 8, BLOCK);
        put32(d + 12, offset);
        put32(d + 16, 0);                                      // nicht verschlüsselt (ESP32-C3)
        return command(OP_FLASH_BEGIN, d, sizeof(d), 0, timeoutPerMb(ERASE_MS_PER_MB, size));
    }

    /// ein Block, len ≤ BLOCK; wird mit 0xFF aufgefüllt
    Result flashBlock(const uint8_t* data, uint32_t len, uint32_t seq)
    {
        put32(blk_, BLOCK);
        put32(blk_ + 4, seq);
        put32(blk_ + 8, 0);
        put32(blk_ + 12, 0);
        std::memcpy(blk_ + 16, data, len);
        std::memset(blk_ + 16 + len, 0xFF, BLOCK - len);
        uint8_t chk = 0xEF;
        for (uint32_t i = 0; i < BLOCK; ++i) chk ^= blk_[16 + i];
        Result r = Result::Timeout;
        for (int a = 0; a < BLOCK_ATTEMPTS && r != Result::Ok; ++a)
            r = command(OP_FLASH_DATA, blk_, sizeof(blk_), chk, DEFAULT_TIMEOUT);
        return r;
    }

    /// MD5 eines Flash-Bereichs als 32 Hex-Zeichen (ROM)
    Result flashMd5(uint32_t addr, uint32_t size, char out[32])
    {
        uint8_t d[16];
        put32(d, addr);
        put32(d + 4, size);
        put32(d + 8, 0);
        put32(d + 12, 0);
        uint8_t resp[32];
        const Result r = command(OP_SPI_FLASH_MD5, d, sizeof(d), 0, timeoutPerMb(MD5_MS_PER_MB, size), 32,
                                 nullptr, resp);
        if (r == Result::Ok) std::memcpy(out, resp, 32);
        return r;
    }

    /// FLASH_END; reboot = ROM startet die Anwendung (sonst bleibt es im Download-Modus)
    Result flashEnd(bool reboot)
    {
        uint8_t d[4];
        put32(d, reboot ? 0 : 1);
        return command(OP_FLASH_END, d, sizeof(d), 0, DEFAULT_TIMEOUT);
    }

    /// Image schreiben und per MD5 prüfen; progress(bytesGeschrieben, gesamt) nach jedem Block
    template <typename Progress>
    Result writeImage(uint32_t offset, const uint8_t* data, uint32_t size, const char* md5Hex, Progress progress)
    {
        Result r = flashBegin(size, offset);
        if (r != Result::Ok) return r;
        const uint32_t n = blocks(size);
        for (uint32_t seq = 0; seq < n; ++seq) {
            const uint32_t off = seq * BLOCK;
            const uint32_t len = (size - off < BLOCK) ? size - off : BLOCK;
            r = flashBlock(data + off, len, seq);
            if (r != Result::Ok) return r;
            progress(off + len, size);
        }
        char md5[32];
        r = flashMd5(offset, size, md5);
        if (r != Result::Ok) return r;
        for (int i = 0; i < 32; ++i)
            if (lower(md5[i]) != lower(md5Hex[i])) return Result::Md5Mismatch;
        return Result::Ok;
    }

    static uint32_t blocks(uint32_t size) { return (size + BLOCK - 1) / BLOCK; }

private:
    static void put32(uint8_t* p, uint32_t v)
    {
        p[0] = static_cast<uint8_t>(v);
        p[1] = static_cast<uint8_t>(v >> 8);
        p[2] = static_cast<uint8_t>(v >> 16);
        p[3] = static_cast<uint8_t>(v >> 24);
    }
    static char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
    static uint32_t timeoutPerMb(uint32_t msPerMb, uint32_t size)
    {
        const uint32_t t = static_cast<uint32_t>(static_cast<uint64_t>(msPerMb) * size / 1000000u);
        return t < DEFAULT_TIMEOUT ? DEFAULT_TIMEOUT : t;
    }

    void slipByte(uint8_t b)
    {
        if (b == 0xC0)      { const uint8_t e[2] = {0xDB, 0xDC}; t_.write(e, 2); }
        else if (b == 0xDB) { const uint8_t e[2] = {0xDB, 0xDD}; t_.write(e, 2); }
        else                t_.write(&b, 1);
    }

    void sendPacket(uint8_t op, const uint8_t* data, uint16_t len, uint32_t chk)
    {
        const uint8_t end = 0xC0;
        uint8_t hdr[8] = {0x00, op, static_cast<uint8_t>(len), static_cast<uint8_t>(len >> 8)};
        put32(hdr + 4, chk);
        t_.write(&end, 1);
        for (uint8_t b : hdr) slipByte(b);
        for (uint16_t i = 0; i < len; ++i) slipByte(data[i]);
        t_.write(&end, 1);
    }

    /// einen SLIP-Rahmen lesen (ohne Begrenzer), Länge oder -1 bei Timeout bis deadline
    int readFrame(uint8_t* buf, int cap, uint32_t deadline)
    {
        for (;;) {                                             // Anfang suchen, Müll verwerfen
            const int c = readUntil(deadline);
            if (c < 0) return -1;
            if (c == 0xC0) break;
        }
        int n = 0;
        bool esc = false;
        for (;;) {
            const int c = readUntil(deadline);
            if (c < 0) return -1;
            if (c == 0xC0) {
                if (n == 0) continue;                          // zwei Begrenzer hintereinander
                return n;
            }
            uint8_t b = static_cast<uint8_t>(c);
            if (esc) {
                b = (b == 0xDC) ? 0xC0 : (b == 0xDD) ? 0xDB : b;
                esc = false;
            } else if (b == 0xDB) {
                esc = true;
                continue;
            }
            if (n < cap) buf[n] = b;
            ++n;
        }
    }

    int readUntil(uint32_t deadline)
    {
        const int32_t left = static_cast<int32_t>(deadline - t_.millis());
        if (left <= 0) return -1;
        return t_.readByte(static_cast<uint32_t>(left));
    }

    void drain(uint32_t ms)
    {
        const uint32_t deadline = t_.millis() + ms;
        while (readUntil(deadline) >= 0) {}
    }

    Result command(uint8_t op, const uint8_t* data, uint16_t len, uint32_t chk, uint32_t timeoutMs,
                   uint16_t respDataLen = 0, uint32_t* val = nullptr, uint8_t* respData = nullptr)
    {
        lastOp_    = op;
        lastError_ = 0;
        sendPacket(op, data, len, chk);
        const uint32_t deadline = t_.millis() + timeoutMs;
        for (int tries = 0; tries < 100; ++tries) {           // fremde Antworten (z. B. SYNC) überspringen
            const int n = readFrame(rx_, sizeof(rx_), deadline);
            if (n < 0) return Result::Timeout;
            if (n < 8 || n > static_cast<int>(sizeof(rx_)) || rx_[0] != 0x01 || rx_[1] != op) continue;
            const int dataLen = n - 8;
            const uint8_t* d = rx_ + 8;
            if (dataLen < respDataLen + 2) {
                if (dataLen >= 2 && d[0] != 0) { lastError_ = d[1]; return Result::Status; }
                return Result::ShortResponse;
            }
            if (d[respDataLen] != 0) { lastError_ = d[respDataLen + 1]; return Result::Status; }
            if (val) *val = static_cast<uint32_t>(rx_[4]) | (rx_[5] << 8) | (rx_[6] << 16) |
                            (static_cast<uint32_t>(rx_[7]) << 24);
            if (respData) std::memcpy(respData, d, respDataLen);
            return Result::Ok;
        }
        return Result::Timeout;
    }

    EspTransport& t_;
    uint8_t blk_[16 + BLOCK] = {};
    uint8_t rx_[64] = {};
    uint8_t lastError_ = 0;
    uint8_t lastOp_ = 0;
};

} // namespace espflash
