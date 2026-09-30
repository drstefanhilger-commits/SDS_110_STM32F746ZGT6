/*
 * CommandAssembler.hpp  (Infrastructure/Utils)
 *
 * Kommandos PC -> SDS aus dem USB-Bytestrom lösen (doc/ICD_SDS_PC_Monitor.md 2, 4; Befund 32).
 * Der Host darf aufeinanderfolgende Schreibvorgänge zu einem USB-Paket zusammenfassen (Windows
 * usbser.sys tut das, sobald Kommandos dicht folgen, z. B. Feedback Id 8 mit bis zu 31/s) oder ein
 * Kommando auf zwei Pakete verteilen. Bisher wurde je Paket nur das erste Kommando ausgewertet.
 *
 *   - Synchronisation Byte für Byte auf das Magic DE AD BE EF
 *   - Gesamtlänge aus Byte 5–7 (big-endian), gültig MIN_LEN … MAX_LEN; sonst 1 Byte verwerfen
 *   - vollständiges Kommando -> Command; Reste bleiben für das nächste Paket
 *   - ein Rest, der älter als PARTIAL_TIMEOUT_US ist, wird vor neuen Daten verworfen: sonst würde
 *     ein abgebrochenes Kommando mit dem Anfang des nächsten zusammengesetzt (CRC noch ungeprüft,
 *     Befund 12)
 * Reine Logik ohne Hardware (Host-Test t_usb_commands).
 */
#pragma once
#include <cstdint>
#include <cstring>

namespace sds110 {

class CommandAssembler {
public:
    static constexpr uint32_t MIN_LEN = 16;
    static constexpr uint32_t MAX_LEN = 56;                  // größtes Kommando (Id 8: 52 Byte)
    static constexpr uint32_t BUF_LEN = 192;                 // drei volle USB-Pakete
    static constexpr uint64_t PARTIAL_TIMEOUT_US = 20000;    // 20 ms
    static constexpr uint32_t ERR_SHOW = 16;                 // Bytes je Fehlermeldung (LCD)

    enum class Result { None, Command, Error };

    /// Paket anhängen; rxUs = Empfangszeit (Laufzeit µs) aus dem USB-Interrupt
    void push(const uint8_t* data, uint32_t n, uint64_t rxUs)
    {
        if (n_ > 0 && rxUs - lastRxUs_ > PARTIAL_TIMEOUT_US) { staleDropped_ += n_; stale_ = n_; n_ = 0; }
        if (n > BUF_LEN) { data += n - BUF_LEN; overflowDropped_ += n - BUF_LEN; n = BUF_LEN; }
        if (n_ + n > BUF_LEN) { const uint32_t d = n_ + n - BUF_LEN; drop(d); overflowDropped_ += d; }
        std::memcpy(buf_ + n_, data, n);
        n_ += n;
        lastRxUs_ = rxUs;
    }

    /**
     * Nächstes Ergebnis:
     *   Command: out/len = vollständiges Kommando ab Magic, gültig bis zum nächsten Aufruf
     *   Error:   out/len = verworfene Bytes (höchstens ERR_SHOW): Müll vor dem Magic, falsche
     *            Länge oder ein veralteter Rest
     *   None:    zu wenig Daten
     */
    Result next(const uint8_t*& out, uint32_t& len)
    {
        if (stale_ > 0) {                                    // veralteter Rest aus push()
            len = 0; stale_ = 0; out = out_;
            return Result::Error;
        }
        // Magic suchen
        uint32_t i = 0;
        while (i + 4 <= n_ && !isMagic(buf_ + i)) ++i;
        if (i + 4 > n_) {                                    // kein Magic: bis auf 3 Byte verwerfen
            const uint32_t keep = (n_ < 3) ? n_ : 3;
            i = n_ - keep;
            while (i < n_ && buf_[i] != MAGIC0) ++i;         // Rest behalten, falls er ein Magic beginnt
        }
        if (i > 0) return dropAsError(i, out, len);
        if (n_ < 8) return Result::None;
        const uint32_t l = (static_cast<uint32_t>(buf_[5]) << 16) | (buf_[6] << 8) | buf_[7];
        if (l < MIN_LEN || l > MAX_LEN) return dropAsError(1, out, len);   // scheinbares Magic: 1 Byte weiter
        if (n_ < l) return Result::None;
        std::memcpy(out_, buf_, l);
        drop(l);
        out = out_; len = l;
        return Result::Command;
    }

    uint32_t pending() const { return n_; }
    uint32_t staleDropped() const { return staleDropped_; }
    uint32_t overflowDropped() const { return overflowDropped_; }

private:
    static constexpr uint8_t MAGIC0 = 0xDE;
    static bool isMagic(const uint8_t* p) { return p[0] == 0xDE && p[1] == 0xAD && p[2] == 0xBE && p[3] == 0xEF; }

    void drop(uint32_t k)
    {
        if (k >= n_) { n_ = 0; return; }
        std::memmove(buf_, buf_ + k, n_ - k);
        n_ -= k;
    }
    Result dropAsError(uint32_t k, const uint8_t*& out, uint32_t& len)
    {
        const uint32_t show = (k < ERR_SHOW) ? k : ERR_SHOW;
        std::memcpy(out_, buf_, show);
        drop(k);
        out = out_; len = show;
        return Result::Error;
    }

    uint8_t  buf_[BUF_LEN] = {};
    uint8_t  out_[MAX_LEN] = {};
    uint32_t n_ = 0, stale_ = 0;
    uint64_t lastRxUs_ = 0;
    uint32_t staleDropped_ = 0, overflowDropped_ = 0;
};

} // namespace sds110
