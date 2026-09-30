/*
 * Sensor_Unit_112.hpp
 * Akustische Sensoreinheit 112-n = 114 + 116 + 118.
 */
#pragma once
#include "stm32f7xx_hal.h"
#include "Microphone_Array_114.hpp"
#include "Sampling_Circuitry_116.hpp"
#include "Pre_Processor_118.hpp"
#include "Frame_Assembler.hpp"
#include "Infrastructure/Utils/DWT.hpp"

namespace sds110 {

class Sensor_Unit_112 {
public:
    explicit Sensor_Unit_112(uint32_t id) : id_(id) {}

    bool init(SAI_HandleTypeDef* hsai, I2C_HandleTypeDef* hi2c)
    {
        pre_.init();
#if SDS110_SAI_ENABLED
        return sampling_.init(hsai, hi2c);
#else
        (void)hsai; (void)hi2c;                        // SAI-Hardwarefehler: 116 bleibt aus, nur Simulator
        return true;
#endif
    }
    bool start() { return sampling_.start(); }

    /// Nächsten Hop aus 114 holen, mit 118 in place verarbeiten und ans Analysefenster anhängen.
    /// Rückgabe false: kein Hop bereit. Sonst true; frame zeigt auf einen vollständigen
    /// Analyse-Frame (64 ms, 50 % Überlappung) oder ist nullptr, solange das Fenster füllt.
    /// Nach den Spektren des Frames releaseOldest() aufrufen (gibt den älteren Hop an 114 zurück).
    bool nextFrame(const AnalysisFrame*& frame)
    {
        frame = nullptr;
        asm_.releaseOldest();                          // falls 120 es nicht schon getan hat
        MicFrame* h = array_.acquireReadable();
        if (!h) return false;
        const uint32_t c0 = DWTTimer::instance().cycles();
        pre_.process(*h);                              // 118 in place (Blockgleitkomma)
        const uint32_t c1 = DWTTimer::instance().cycles();
        const bool full = asm_.push(h);                // Besitz geht an den Frame_Assembler
        lastPreCycles_ = c1 - c0;                      // Diagnose Rechenlast: 118 und Fenster getrennt
        lastAsmCycles_ = DWTTimer::instance().cycles() - c1;
        if (full) frame = &asm_.frame();
        return true;
    }
    /// Älteren Hop des aktuellen Frames freigeben (Spektren berechnet; frame danach ungültig)
    void releaseOldest() { asm_.releaseOldest(); }

    /// READ-Modus: nächster Hop als Rohdaten, also ohne 118 (Bandpass, NS, AGC), ohne Überlappung
    /// (nullptr = keiner bereit). Befund 35: Aufnahmen für Training und Endabnahme (AP 8) brauchen
    /// die Mikrofonsignale; tools/features/sds_features wendet 118 selbst an. Aufrufer gibt den
    /// Hop mit releaseHop() zurück. Das Analysefenster beginnt danach neu; 118 läuft beim Wechsel
    /// zurück nach DETECT mit dem alten Zustand weiter (einige Frames Einschwingen).
    MicFrame* nextHop()
    {
        asm_.reset();                                  // gehaltene Hops an 114 zurück
        return array_.acquireReadable();
    }
    void releaseHop(MicFrame* h) { array_.release(h); }

    uint32_t id() const { return id_; }
    uint32_t lastPreCycles() const { return lastPreCycles_; }   ///< 118 des letzten Hops (DWT-Zyklen)
    uint32_t lastAsmCycles() const { return lastAsmCycles_; }   ///< Frame_Assembler des letzten Hops
    const Microphone_Array_114& array() const { return array_; }
    const Pre_Processor_118& preprocessor() const { return pre_; }
    const Sampling_Circuitry_116& sampling() const { return sampling_; }
    Sampling_Circuitry_116& sampling() { return sampling_; }

private:
    uint32_t id_;
    Microphone_Array_114&   array_    = Microphone_Array_114::instance();
    Sampling_Circuitry_116& sampling_ = Sampling_Circuitry_116::instance();
    Pre_Processor_118       pre_;
    Frame_Assembler         asm_;   // hält nur Zeiger auf Hops im 114-Pool
    uint32_t lastPreCycles_ = 0, lastAsmCycles_ = 0;
};

} // namespace sds110
