/*
 * SDS_Structs.hpp  (Infrastructure)
 *
 * Verbleibende gemeinsame Typen: RTOS-Events, Betriebsmodi, USB-Wire-Format,
 * Fehlermeldungen. Keine DSP-Typen mehr.
 *
 * Migration aus SDS/Model/SDS_Structs.hpp – entfernt / verschoben:
 *   Vec3                  -> Microphone_Array_114.hpp
 *   SRPFrame, DistanceResult -> SRP-spezifisch, ersetzt durch TdoaMeasurement (126) / CandidateLocation (128)
 *   TrackState            -> Tracking Unit 150, nicht Teil von SDS 110 (Claim 10)
 *   SystemState, DetectionState, SDSEvent -> ersetzt durch SDS_Data-Felder
 *   SDS_MicPayload        -> auf FRAME_SAMPLES umgestellt (unten)
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include "SDS_110_Config.hpp"

// ======================================================
//  RTOS Events
// ======================================================
enum class SDS_DataEventType : uint8_t {
    REPORT_UPDATE,      // neuer Candidate Report (ehem. SRP_UPDATE)
    DEBUG_UPDATE,
    MICFRAME_UPDATE     // neuer Frame aus 114 (ehem. MICBLOCK_UPDATE)
};

struct SDS_DataEvent {
    SDS_DataEventType type;
    float             processTime;
};

// ======================================================
//  Betriebsmodus (USB-steuerbar)
// ======================================================
enum class SDS_Mode : uint32_t {
    DETECT    = 1,
    CALIBRATE = 2,
    READ      = 3,
    ERROR     = 99
};

// ======================================================
//  USB Wire-Format (unverändert aus SDS, PC-Monitor-kompatibel)
// ======================================================
// Kommandos PC -> Board: [Magic 4][Id 1][Länge 3, big-endian][Wert 4][CRC 4].
// Die Länge ist die GESAMTLÄNGE der Nachricht (16 Byte), wie bei den Nachrichten
// Board -> PC (len_id = sizeof). Das Format vor 07.09.2026 (ohne Magic) zählte
// nur 12 Byte; die Vorlagen unten trugen noch diesen alten Wert.
constexpr uint32_t SDS_CMD_LENGTH = 16;

#pragma pack(push, 1)
struct SDS_UnixTimeSync {
    uint8_t magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t id       =  0x01;
    uint8_t size[3]  = {0x00, 0x00, SDS_CMD_LENGTH};
    uint8_t time[4]  = {0x00, 0x00, 0x00, 0x00};
    uint8_t crc[4]   = {0x00, 0x00, 0x00, 0x00};   // ToDo activate crc
};

struct SDS_ModeChange {
    uint8_t magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t id       =  0x02;
    uint8_t size[3]  = {0x00, 0x00, SDS_CMD_LENGTH};
    uint8_t mode[4]  = {0x00, 0x00, 0x00, 0x01};   // SDS_Mode
    uint8_t crc[4]   = {0x00, 0x00, 0x00, 0x00};
};

struct SDS_ModeSimulation {
    uint8_t magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t id       =  0x03;
    uint8_t size[3]  = {0x00, 0x00, SDS_CMD_LENGTH};
    uint8_t sim[4]   = {0x00, 0x00, 0x00, 0x01};   // 0 = Real, 1 = Simulated
    uint8_t crc[4]   = {0x00, 0x00, 0x00, 0x00};
};

struct SDS_SetUnitId {
    uint8_t magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t id       =  0x05;
    uint8_t size[3]  = {0x00, 0x00, SDS_CMD_LENGTH};
    uint8_t unit[4]  = {0x00, 0x00, 0x00, 0x01};   // Unit-ID (u32, nur untere 16 Bit genutzt)
    uint8_t crc[4]   = {0x00, 0x00, 0x00, 0x00};
};
struct SDS_SrpReference {
    uint8_t magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t id       =  0x06;
    uint8_t size[3]  = {0x00, 0x00, SDS_CMD_LENGTH};
    uint8_t on[4]    = {0x00, 0x00, 0x00, 0x00};   // SRP-Referenzscan: 0 = aus, 1 = ein
    uint8_t crc[4]   = {0x00, 0x00, 0x00, 0x00};
};
/// Sync vom PC-Monitor (doc/ICD_SDS_PC_Monitor.md): UTC in µs seit 01.01.1970 und Luft-
/// temperatur in 0,01 °C, big-endian wie die übrigen Kommandowerte. UTC 0 = keine Zeit (nur
/// Temperatur), Temperatur 0x8000 = unbekannt. Die Firmware setzt Versatz = UTC − Laufzeit beim
/// Empfang (USB-Interrupt, ~1 ms genau) und c aus der Temperatur. Länge 24 Byte.
constexpr uint32_t SDS_SYNC_CMD_LENGTH = 24;
struct SDS_Sync {
    uint8_t magic[4]  = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t id        =  0x07;
    uint8_t size[3]   = {0x00, 0x00, SDS_SYNC_CMD_LENGTH};
    uint8_t utc_us[8] = {};                        // µs seit Epoche, MSB zuerst; 0 = keine Zeit
    uint8_t temp[2]   = {0x80, 0x00};              // int16, 0,01 °C, MSB zuerst; 0x8000 = unbekannt
    uint8_t reserved[2] = {};                      // 0
    uint8_t crc[4]    = {0x00, 0x00, 0x00, 0x00};
};
static_assert(sizeof(SDS_Sync) == SDS_SYNC_CMD_LENGTH, "Sync-Kommando muss 24 Byte lang sein");
static_assert(sizeof(SDS_UnixTimeSync)   == SDS_CMD_LENGTH, "Kommando muss 16 Byte lang sein");
static_assert(sizeof(SDS_ModeChange)     == SDS_CMD_LENGTH, "Kommando muss 16 Byte lang sein");
static_assert(sizeof(SDS_ModeSimulation) == SDS_CMD_LENGTH, "Kommando muss 16 Byte lang sein");
static_assert(sizeof(SDS_SetUnitId)      == SDS_CMD_LENGTH, "Kommando muss 16 Byte lang sein");
static_assert(sizeof(SDS_SrpReference)   == SDS_CMD_LENGTH, "Kommando muss 16 Byte lang sein");

struct SDS_MsgDetect {
    uint32_t magic     = 0xDEADBEEF;
    uint32_t len_id    = ((sizeof(SDS_MsgDetect) & 0x00FFFFFF) | ((uint32_t)0x01 << 24));
    uint32_t timestamp = 0;
    uint32_t mic;              // = Unit-ID (SDS_110)
    float    azi;
    float    distance;
    float    conf;
    uint32_t crc32;
};
static_assert(sizeof(SDS_MsgDetect) == 32, "SDS_MsgDetect must be 32 bytes (wire format)");

#define SDS_MSG_BUFFER_SIZE 128
struct SDS_MsgRead {
    uint32_t magic     = 0xDEADBEEF;
    uint32_t len_id    = ((sizeof(SDS_MsgRead) & 0x00FFFFFF) | ((uint32_t)0x02 << 24));
    uint32_t timestamp = 0;
    uint8_t  micNr     = 0;      // Mikrofon 0…7
    uint8_t  blockNr   = 0;      // Block 0…11 im Hop (je 128 Samples)
    uint16_t hopNr     = 0;      // Hop-Nummer (frame_id aus 114, mod 2^16): Lücken erkennbar (Befund 35)
    uint32_t data[SDS_MSG_BUFFER_SIZE];
    uint32_t crc32;
};
static_assert(sizeof(SDS_MsgRead) == 532, "SDS_MsgRead must be 532 bytes (wire format)");
static_assert(offsetof(SDS_MsgRead, micNr) == 12 && offsetof(SDS_MsgRead, blockNr) == 13 &&
              offsetof(SDS_MsgRead, hopNr) == 14 && offsetof(SDS_MsgRead, data) == 16,
              "SDS_MsgRead: Kopf wie ICD 5.3 (mic u8, block u8, hop u16)");

union MessageData {
    uint8_t  b[128];
    uint16_t h[64];
    uint32_t w[32];
};

struct Message {
    uint32_t magic     = 0xDEADBEEF;
    uint32_t len_id    = ((sizeof(Message) & 0x00FFFFFF) | ((uint32_t)0x03 << 24));
    uint32_t timestamp = 0;
    MessageData data;
    uint32_t crc32     = 0;
};
#pragma pack(pop)

// Payload für Rohdaten-Streaming (READ-Modus); Größe folgt jetzt Config
struct SDS_MicPayload {
    float micData[sds110::NUM_MICS][sds110::FRAME_SAMPLES];
};

// ======================================================
//  Fehlermeldung (Queue)
// ======================================================
struct SDS_ErrorMessage {
    char text[32];
};
