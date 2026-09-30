/*
 * USBDriver.hpp  (Infrastructure/Driver)
 *
 * Sendeseite CDC: Wire-Format aus SDS_Structs.hpp + CRC32 -> TX-Ringpuffer -> CDC.
 * STM32F746ZGT6-Board: Transport über USART1/CP2102N statt USB-CDC (SDS110_LINK_UART,
 * SDS_110_Board.h); Nachrichtenformat unverändert.
 * Wird von LoggerTask (Logging), Processing_Module_120 (READ-Streaming) und von
 * Output_Interface_130 (UnitReport) genutzt.
 *
 * Sendepfad (thread-sicher, keine eigene Task):
 *   send*() baut die Nachricht auf dem Stack und kopiert sie vollständig in den
 *   Ringpuffer (ganz oder gar nicht, im kritischen Abschnitt). Ist der Endpunkt frei,
 *   wird sofort ein Block von bis zu TX_CHUNK Byte in den statischen txBuf_ kopiert
 *   und übertragen. CDC_TransmitCplt_FS (OTG-FS-ISR) lädt den nächsten Block nach.
 *   Damit bleibt der übertragene Puffer bis zum Ende des Transfers gültig, und
 *   BUSY führt nicht mehr zum Verlust von Nachrichten.
 *   Rückgabe false: Ringpuffer voll (nach waitMs) oder USB nicht konfiguriert.
 *   waitMs > 0 nur aus Task-Kontext (wartet mit osDelay(1)).
 *
 * Migration aus SDS/Driver/USBDriver.{hpp,cpp}:
 *  - USB_SendRead: liest jetzt float-Frames aus Microphone_Array_114 (kein raw[] mehr);
 *    Werte werden auf int32 (24-bit) zurückskaliert, Wire-Format bleibt 532 Byte.
 *  - USB_SendRead_Test entfällt; SDSUSBMicSender (Prototyp) entfällt.
 */
#pragma once
#include <cstdint>
#include "Infrastructure/Model/SDS_Structs.hpp"
#include "Infrastructure/Utils/LocalPosition.hpp"
#include "Sensor_Unit_112/Microphone_Array_114.hpp"

namespace sds110 {

class USBDriver {
public:
    /// 32 Byte, id 1 – Azimut/Distanz/Konfidenz (Legacy-Format des PC-Monitors)
    static bool sendDetection(uint32_t timestamp, uint32_t micId, float azimuth, float distance, float confidence,
                              uint32_t waitMs = 0);
    /// 532 Byte, id 2 – 128 Rohsamples (vor 118) eines Mikrofons ab blockNr*128, mit Hop-Nummer
    static bool sendRead(uint32_t timestamp, uint32_t micNr, uint32_t blockNr, uint32_t hopNr, const MicFrame* frame,
                         uint32_t waitMs = 0);
    /// 48 Byte, id 3 – 32 Byte Logtext
    static bool sendLogging(uint32_t timestamp, const uint8_t* src, int len, uint32_t waitMs = 0);
    /// 144 Byte, id 6 – Standort der Einheit (ICD 5.5), Rahmen wie sendMessage
    static bool sendPosition(const LocalPosition& pos, uint16_t unit, uint32_t waitMs = 0);
    static constexpr uint32_t POSITION_MSG_ID = 6;
    /// generische Nachricht (128 Byte Payload)
    static bool sendMessage(uint32_t id, uint32_t timestamp, const MessageData& data, uint32_t waitMs = 0);

    /// aus CDC_TransmitCplt_FS bzw. HAL_UART_TxCpltCallback (ISR-Kontext): nächsten Block übertragen
    static void onTransmitComplete();
    /// Transport starten: USART1 (Baudrate, IRQ, Empfang) bei SDS110_LINK_UART, sonst nichts (USB-CDC)
    static void startLink();

    /// Diagnose: verworfene Nachrichten (Puffer voll / USB nicht konfiguriert)
    static uint32_t txDropped() { return txDropped_; }

    static constexpr uint32_t TX_RING_SIZE = 8192;   // Zweierpotenz
    static constexpr uint32_t TX_CHUNK     = 2048;   // max. Bytes je CDC-Transfer

private:
    static bool transmit(const void* buf, uint32_t len, uint32_t waitMs);
    static bool enqueue(const uint8_t* buf, uint32_t len);   // im kritischen Abschnitt
    static void kick();                                      // ISR oder kritischer Abschnitt
    static bool linkReady();

    static uint8_t  ring_[TX_RING_SIZE];
    static uint8_t  txBuf_[TX_CHUNK];
    static volatile uint32_t head_;       // Schreibindex (frei laufend)
    static volatile uint32_t tail_;       // Leseindex (frei laufend)
    static volatile uint32_t txDropped_;
};

} // namespace sds110

extern "C" void USBDriver_OnTransmitComplete(void);
