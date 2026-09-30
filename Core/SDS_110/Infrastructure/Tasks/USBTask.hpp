/*
 * USBTask.hpp  (Infrastructure/Tasks)
 *
 * Empfangsseite CDC (Kommandos vom PC-Monitor) und nicht-patentrelevante
 * Sendepfade (Logging, Rohdaten im READ-Modus).
 * Der Candidate Report (Patent) wird NICHT hier, sondern in
 * Output_Interface_130 gesendet.
 *
 * Anbindung in usbd_cdc_if.c, CDC_Receive_FS(), USER CODE 6:
 *     extern void USBTask_OnReceive(uint8_t* buf, uint32_t len);
 *     USBTask_OnReceive(Buf, *Len);
 * Ein USB-Paket (max. 64 Byte) kann mehrere Kommandos oder den Teil eines Kommandos
 * enthalten; der CommandAssembler setzt den Bytestrom zusammen (Befund 32). Das Symbol usb_debug_counter (bisher in LCDTask)
 * wird hier definiert.
 *
 * Ereignisgetrieben: waitForWork() blockiert auf rxQueue_, bis ein Kommando
 * kommt (kein Polling, keine Latenz). Stats "USB": Bearbeitungszeit je
 * Aufwachen, Zähler = Anzahl Aufwachvorgänge. Kommt 2 s (IDLE_RESET_MS)
 * kein Kommando, wird die Bearbeitungszeit einmalig auf 0 gesetzt.
 *
 * Migration aus SDS/Tasks/USBTask: SDS_Data-API angepasst
 * (setMode(SDS_Mode), setTaskStats, setErrorBuffer statt getErrorBuffer()+memcpy).
 */
#pragma once
#include <cstring>
#include "FreeRTOS.h"
#include "queue.h"
#include "TaskBase.hpp"
#include "Infrastructure/Driver/USBDriver.hpp"
#include "Infrastructure/Utils/CommandAssembler.hpp"

namespace sds110 {

class USBTask : public TaskBase {
public:
    static USBTask& instance() { static USBTask inst; return inst; }

    /// aus CDC_Receive_FS (ISR-Kontext). Konstruiert NICHT die Instanz:
    /// solange der Task nicht angelegt ist, wird verworfen.
    static void onUsbReceiveISR(const uint8_t* buf, uint32_t len);

    uint32_t rxDropped() const { return rxDropped_; }

protected:
    void waitForWork() override;
    void runOnce() override;

private:
    USBTask();
    static constexpr size_t   MAX_LENGTH    = 64;    // ein USB-FS-Paket
    struct RxChunk { uint64_t rxUs; uint8_t len; uint8_t data[MAX_LENGTH]; };
    static constexpr UBaseType_t RX_QUEUE_LEN = 16;

    void handleChunk(const RxChunk& c);
    void handle(const uint8_t* rx);                // ein vollständiges Kommando ab Magic
    void handleTimeSync(const uint8_t* rx);
    void handleModeChange(const uint8_t* rx);
    void handleSimulation(const uint8_t* rx);
    void handleSetUnitId(const uint8_t* rx);     // Typ 5: Payload u32 = neue Unit-ID
    void handleSrpReference(const uint8_t* rx);  // Typ 6: Payload u32 0 = SRP-Scan aus, sonst ein
    void handleSync(const uint8_t* rx);          // Typ 7: UTC in µs (u64) + Temperatur (i16, 0,01 °C), 24 Byte
    void handleFeedback(const uint8_t* rx);      // Typ 8: Feedback der Tracking-Einheit (ŝ, Vorhersage), 52 Byte
    void handleAzimuthOffset(const uint8_t* rx); // Typ 9: Nordabgleich, Offset i32 in 0,01°
    void handlePosition(const uint8_t* rx);      // Typ 10: Standort lokal (Ost, Nord, Oben), 28 Byte; Antwort Id 6
    void handleError(const uint8_t* rx);
    void handleErrorBytes(const uint8_t* p, uint32_t n);
    static bool hasMagic(const uint8_t* rx);
    /// Längenfeld: Gesamtlänge der Nachricht (SDS_CMD_LENGTH), nicht nur der Nutzdaten
    static uint32_t msgLen(const uint8_t* rx)     { return (rx[5] << 16) | (rx[6] << 8) | rx[7]; }
    static uint32_t payloadU32(const uint8_t* rx)  { return (rx[8] << 24) | (rx[9] << 16) | (rx[10] << 8) | rx[11]; }
    static uint64_t payloadU64(const uint8_t* rx)
    { uint64_t v = 0; for (int i = 8; i < 16; ++i) v = (v << 8) | rx[i]; return v; }
    void resetCounters();

    static constexpr uint32_t IDLE_RESET_MS = 2000;  // danach Zeit-Anzeige = 0
    SDS_Data&     dm_ = SDS_Data::instance();
    QueueHandle_t rxQueue_ = nullptr;
    RxChunk       rx_{};                         // von waitForWork() empfangen
    CommandAssembler asm_;                       // Bytestrom -> Kommandos
    uint64_t      cmdRxUs_ = 0;                  // Empfangszeit des Pakets mit dem aktuellen Kommando
    bool          rxValid_ = false;
    volatile uint32_t rxDropped_ = 0;            // Queue voll / Task nicht bereit

    static USBTask* volatile active_;            // gesetzt, sobald Queue existiert
};

} // namespace sds110

extern "C" {
void USBTask_OnReceive(uint8_t* buf, uint32_t len);
extern volatile uint32_t usb_debug_counter;
}
