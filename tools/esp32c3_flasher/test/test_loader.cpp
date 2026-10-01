/*
 * test_loader.cpp – EspRomLoader gegen ein simuliertes ESP32-C3-ROM (Host, ohne Hardware).
 *
 *   make -C tools/esp32c3_flasher test
 *
 * Das simulierte ROM wertet SLIP-Rahmen wie das echte aus: Prüfsumme der FLASH_DATA-Blöcke,
 * Reihenfolge der Blöcke, SPI_ATTACH vor dem Flash-Zugriff, Baudratenwechsel (Rahmen bei falscher
 * Baudrate gehen verloren), mehrfache SYNC-Antworten, MD5 als 32 Hex-Zeichen + 4 Statusbytes.
 */
#include "../src/EspRomLoader.hpp"

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <string>
#include <vector>

using namespace espflash;

// ---------------------------------------------------------------- MD5 (RFC 1321)
static std::string md5hex(const uint8_t* msg, size_t len)
{
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
                              5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                              6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    std::vector<uint8_t> m(msg, msg + len);
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    const uint64_t bits = static_cast<uint64_t>(len) * 8;
    for (int i = 0; i < 8; ++i) m.push_back(static_cast<uint8_t>(bits >> (8 * i)));
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[16];
        for (int i = 0; i < 16; ++i)
            w[i] = m[off + 4 * i] | (m[off + 4 * i + 1] << 8) | (m[off + 4 * i + 2] << 16) |
                   (static_cast<uint32_t>(m[off + 4 * i + 3]) << 24);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 64; ++i) {
            uint32_t f;
            int g;
            if (i < 16)      { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) % 16; }
            else             { f = c ^ (b | ~d);       g = (7 * i) % 16; }
            const uint32_t t = d;
            d = c;
            c = b;
            const uint32_t x = a + f + K[i] + w[g];
            b = b + ((x << R[i]) | (x >> (32 - R[i])));
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    char out[33];
    for (int i = 0; i < 16; ++i) snprintf(out + 2 * i, 3, "%02x", (h[i / 4] >> (8 * (i % 4))) & 0xFF);
    return std::string(out, 32);
}

// ---------------------------------------------------------------- simuliertes ROM + Transport
class FakeRom : public EspTransport {
public:
    std::vector<uint8_t> flash = std::vector<uint8_t>(4 * 1024 * 1024, 0xA5);
    int  syncIgnore      = 0;      // so viele SYNC-Rahmen überhören (Download-Modus noch nicht aktiv)
    int  corruptDataSeq  = -1;     // diesen FLASH_DATA-Block einmal mit falscher Prüfsumme empfangen
    bool silentCorrupt   = false;  // ein Byte im Flash still verfälschen (MD5 muss es finden)
    int  dataFrames = 0, checksumErrors = 0, syncFrames = 0;
    uint32_t romBaud = 115200, hostBaud = 115200, now = 0;

    // EspTransport
    void write(const uint8_t* d, size_t n) override
    {
        for (size_t i = 0; i < n; ++i) feed(d[i]);
    }
    int readByte(uint32_t timeoutMs) override
    {
        if (rx_.empty()) { now += timeoutMs; return -1; }
        const int b = rx_.front();
        rx_.pop_front();
        return b;
    }
    void flushInput() override { rx_.clear(); }
    void setBaud(uint32_t b) override { hostBaud = b; }
    void delayMs(uint32_t ms) override { now += ms; }
    uint32_t millis() override { return now; }

private:
    std::deque<uint8_t> rx_;
    std::vector<uint8_t> frame_;
    bool inFrame_ = false, esc_ = false, attached_ = false;
    uint32_t beginOff_ = 0, beginBlocks_ = 0, beginBlockSize_ = 0, nextSeq_ = 0;

    static uint32_t get32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

    void feed(uint8_t b)
    {
        if (b == 0xC0) {
            if (inFrame_ && !frame_.empty()) handle(frame_);
            frame_.clear();
            inFrame_ = true;
            esc_ = false;
            return;
        }
        if (!inFrame_) return;
        if (esc_) { b = (b == 0xDC) ? 0xC0 : 0xDB; esc_ = false; }
        else if (b == 0xDB) { esc_ = true; return; }
        frame_.push_back(b);
    }

    void reply(uint8_t op, uint32_t val, const std::vector<uint8_t>& data, uint8_t status, uint8_t err)
    {
        std::vector<uint8_t> p = {0x01, op, 0, 0, static_cast<uint8_t>(val), static_cast<uint8_t>(val >> 8),
                                  static_cast<uint8_t>(val >> 16), static_cast<uint8_t>(val >> 24)};
        p.insert(p.end(), data.begin(), data.end());
        p.push_back(status);
        p.push_back(err);
        p.push_back(0);
        p.push_back(0);
        const uint16_t len = static_cast<uint16_t>(p.size() - 8);
        p[2] = static_cast<uint8_t>(len);
        p[3] = static_cast<uint8_t>(len >> 8);
        rx_.push_back(0xC0);
        for (uint8_t b : p) {
            if (b == 0xC0)      { rx_.push_back(0xDB); rx_.push_back(0xDC); }
            else if (b == 0xDB) { rx_.push_back(0xDB); rx_.push_back(0xDD); }
            else                rx_.push_back(b);
        }
        rx_.push_back(0xC0);
    }

    void handle(std::vector<uint8_t> f)
    {
        if (hostBaud != romBaud) return;                      // falsche Baudrate: nur Müll angekommen
        if (f.size() < 8 || f[0] != 0x00) return;
        const uint8_t op = f[1];
        const uint16_t len = f[2] | (f[3] << 8);
        const uint32_t chk = get32(&f[4]);
        if (f.size() != 8u + len) { reply(op, 0, {}, 1, 0xC1); return; }
        const uint8_t* d = f.data() + 8;
        switch (op) {
        case EspRomLoader::OP_SYNC:
            ++syncFrames;
            if (syncIgnore > 0) { --syncIgnore; return; }
            for (int i = 0; i < 8; ++i) reply(op, 0x12345678, {}, 0, 0);
            return;
        case EspRomLoader::OP_SPI_ATTACH:
            attached_ = (len == 8);
            reply(op, 0, {}, attached_ ? 0 : 1, attached_ ? 0 : 0xC1);
            return;
        case EspRomLoader::OP_SPI_SET_PARAMS:
            reply(op, 0, {}, (len == 24 && get32(d + 4) == flash.size()) ? 0 : 1, 0);
            return;
        case EspRomLoader::OP_CHANGE_BAUD:
            reply(op, 0, {}, 0, 0);                            // Antwort noch mit alter Baudrate
            romBaud = get32(d);
            return;
        case EspRomLoader::OP_FLASH_BEGIN: {
            if (!attached_ || len != 20) { reply(op, 0, {}, 1, 0xC1); return; }
            const uint32_t erase = get32(d), off = get32(d + 12);
            beginBlocks_ = get32(d + 4);
            beginBlockSize_ = get32(d + 8);
            beginOff_ = off;
            nextSeq_ = 0;
            const uint32_t from = off & ~0xFFFu, to = (off + erase + 0xFFF) & ~0xFFFu;
            for (uint32_t a = from; a < to; ++a) flash[a] = 0xFF;
            now += erase / 1000;                               // Löschzeit
            reply(op, 0, {}, 0, 0);
            return;
        }
        case EspRomLoader::OP_FLASH_DATA: {
            ++dataFrames;
            const uint32_t n = get32(d), seq = get32(d + 4);
            std::vector<uint8_t> blk(d + 16, d + 16 + n);
            if (static_cast<int>(seq) == corruptDataSeq) { blk[3] ^= 0x40; corruptDataSeq = -1; }
            uint8_t c = 0xEF;
            for (uint8_t b : blk) c ^= b;
            if (n != beginBlockSize_ || len != 16 + n) { reply(op, 0, {}, 1, 0xC1); return; }
            if (c != chk) { ++checksumErrors; reply(op, 0, {}, 1, 0x07); return; }
            if (seq != nextSeq_ || seq >= beginBlocks_) { reply(op, 0, {}, 1, 0xC2); return; }
            for (uint32_t i = 0; i < n; ++i) flash[beginOff_ + seq * n + i] &= blk[i];   // NOR: nur 1 -> 0
            if (silentCorrupt && seq == 1) { flash[beginOff_ + seq * n + 5] ^= 0x01; silentCorrupt = false; }
            ++nextSeq_;
            reply(op, 0, {}, 0, 0);
            return;
        }
        case EspRomLoader::OP_SPI_FLASH_MD5: {
            const std::string h = md5hex(&flash[get32(d)], get32(d + 4));
            reply(op, 0, std::vector<uint8_t>(h.begin(), h.end()), 0, 0);
            return;
        }
        case EspRomLoader::OP_FLASH_END:
            reply(op, 0, {}, 0, 0);
            return;
        default:
            reply(op, 0, {}, 1, 0x05);                         // unbekannter Befehl
        }
    }
};

// ---------------------------------------------------------------- Tests
static int failures = 0;
#define CHECK(cond, msg)                                                   \
    do {                                                                   \
        if (cond) std::printf("ok   %s\n", msg);                           \
        else { std::printf("FEHL %s (Zeile %d)\n", msg, __LINE__); ++failures; } \
    } while (0)

struct Image { uint32_t off; std::vector<uint8_t> data; std::string md5; };

static std::vector<Image> makeImages()
{
    std::mt19937 rng(42);
    auto rnd = [&](size_t n) {
        std::vector<uint8_t> v(n);
        for (auto& b : v) b = static_cast<uint8_t>(rng());
        for (size_t i = 0; i < n; i += 97) v[i] = (i & 1) ? 0xC0 : 0xDB;   // SLIP-Sonderzeichen erzwingen
        return v;
    };
    std::vector<Image> im = {{0x0, rnd(20 * 1024 + 17), ""},       // Bootloader
                             {0x8000, rnd(3072), ""},             // Partitionstabelle
                             {0xD000, std::vector<uint8_t>(8192, 0xFF), ""},   // otadata
                             {0x10000, rnd(790353), ""}};         // Anwendung
    for (auto& i : im) i.md5 = md5hex(i.data.data(), i.data.size());
    return im;
}

static Result flashAll(FakeRom& rom, EspRomLoader& l, const std::vector<Image>& im, uint32_t baud)
{
    Result r = l.sync(10);
    if (r != Result::Ok) return r;
    if ((r = l.spiAttach()) != Result::Ok) return r;
    if ((r = l.setFlashParams(4 * 1024 * 1024)) != Result::Ok) return r;
    if (baud && (r = l.changeBaud(baud)) != Result::Ok) return r;
    for (const auto& i : im) {
        uint32_t last = 0;
        r = l.writeImage(i.off, i.data.data(), static_cast<uint32_t>(i.data.size()), i.md5.c_str(),
                         [&](uint32_t done, uint32_t) { last = done; });
        if (r != Result::Ok) return r;
        if (last != i.data.size()) return Result::ShortResponse;
    }
    (void)rom;
    return l.flashEnd(true);
}

int main()
{
    std::printf("MD5-Selbsttest: %s\n", md5hex(reinterpret_cast<const uint8_t*>("abc"), 3).c_str());
    CHECK(md5hex(reinterpret_cast<const uint8_t*>("abc"), 3) == "900150983cd24fb0d6963f7d28e17f72", "MD5 (RFC 1321, \"abc\")");
    const auto images = makeImages();

    {   // normaler Ablauf mit Baudwechsel, ROM hört die ersten SYNC nicht
        FakeRom rom;
        rom.syncIgnore = 3;
        EspRomLoader l(rom);
        const Result r = flashAll(rom, l, images, 460800);
        CHECK(r == Result::Ok, "alle Images geschrieben und per MD5 bestätigt");
        bool same = true;
        for (const auto& i : images)
            same &= std::equal(i.data.begin(), i.data.end(), rom.flash.begin() + i.off);
        CHECK(same, "Flash-Inhalt gleich den Images");
        CHECK(rom.flash[0x8000 + 3072] == 0xFF && rom.flash[0x8FFF] == 0xFF, "Rest des 4-KB-Sektors gelöscht");
        CHECK(rom.flash[0x9000] == 0xA5 && rom.flash[0xF000] == 0xA5, "außerhalb der Images unverändert");
        CHECK(rom.romBaud == 460800 && rom.hostBaud == 460800, "Baudrate auf beiden Seiten 460800");
        CHECK(rom.syncFrames == 4, "SYNC nach 3 überhörten Versuchen");
        std::printf("     FLASH_DATA-Rahmen %d, simulierte Zeit %.1f s\n", rom.dataFrames, rom.now / 1000.0);
    }
    {   // Übertragungsfehler in einem Block: ROM meldet Prüfsummenfehler, Loader wiederholt
        FakeRom rom;
        rom.corruptDataSeq = 5;
        EspRomLoader l(rom);
        CHECK(flashAll(rom, l, images, 0) == Result::Ok, "Block mit Prüfsummenfehler wiederholt");
        CHECK(rom.checksumErrors == 1, "genau ein Prüfsummenfehler gemeldet");
    }
    {   // Flash still verfälscht: MD5-Vergleich muss es finden
        FakeRom rom;
        rom.silentCorrupt = true;
        EspRomLoader l(rom);
        CHECK(flashAll(rom, l, images, 0) == Result::Md5Mismatch, "verfälschter Flash: MD5 weicht ab");
    }
    {   // kein Download-Modus (SW3 nicht gedrückt): SYNC scheitert
        FakeRom rom;
        rom.syncIgnore = 1000;
        EspRomLoader l(rom);
        CHECK(l.sync(10) == Result::Timeout, "ohne Download-Modus: SYNC-Timeout");
    }
    {   // FLASH_BEGIN ohne SPI_ATTACH: ROM-Fehler wird gemeldet
        FakeRom rom;
        EspRomLoader l(rom);
        l.sync(3);
        CHECK(l.flashBegin(4096, 0) == Result::Status && l.lastError() == 0xC1, "ROM-Fehlercode durchgereicht");
    }

    std::printf(failures ? "=== %d Fehler\n" : "=== alle Prüfungen bestanden\n", failures);
    return failures ? 1 : 0;
}
