/*
 * SDS_Data.hpp  (Infrastructure)
 *
 * Synchronisiertes Status-/Debug-Modell für LCD-, Logger- und USB-Task.
 * Enthält KEINE Signalverarbeitung mehr: 120 schreibt Ergebnisse hinein,
 * die Anzeige-/Transport-Tasks lesen sie. Mutex-geschützt (CMSIS-RTOS2).
 *
 * Migration aus SDS/Model/SDS_Data – entfernt:
 *   computeMelFeatures(), FFTProcessor/MelFilterbank/MelSpectrogram, fftMag/melOut
 *       -> Feature_Extraction_Module_122
 *   setAiDrone/Human/Wind/Background, droneDetected  (4-Klassen-Modell)
 *       -> ersetzt durch AcousticState-Spiegel (64 Bandwahrscheinlichkeiten, HBD-ML)
 *   srp-, lcd-, mic-, usb-Task-Statistiken -> generisches TaskStats[TaskId]
 * Beibehalten: Error-Queue, Event-Queue, Mode/Simulation, Debug-Werte, Error-Buffer.
 */
#pragma once
#include <cstdint>
#include <cstring>
#include "cmsis_os2.h"
#include "SDS_Params.hpp"
#include "SDS_Structs.hpp"
#include "Data_Interface_140/Candidate_Report_140.hpp"
#include "Infrastructure/Utils/UtcClock.hpp"
#include "Infrastructure/Utils/LocalPosition.hpp"
#include "Infrastructure/Utils/SoundSpeed.hpp"

enum class TaskId : uint8_t { Proc120 = 0, Lcd, Mic, Usb, Logger, Count };

struct TaskStats {
    uint32_t freeStack  = 0;
    float    loopTime   = 0.0f;
    uint32_t loopCounter = 0;
};

class SDS_Data {
public:
    static SDS_Data& instance();

    // --- Ergebnis der Patent-Kette (von 120 geschrieben) -------------------
    void setAcousticState(const sds110::AcousticState& s);
    void getAcousticState(sds110::AcousticState& out) const;
    /// Kandidat aus 128: Azimut, Distanz, Konfidenz 0..1 (candidateConfidence)
    void setCandidate(float azimuthDeg, float distanceM, float confidence, bool valid);

    float    getAzimuth()  const { return getValue(azimuthDeg); }
    float    getDistance() const { return getValue(distance); }
    float    getConfidence() const { return getValue(confidence); }
    bool     getDetected() const { return getValue(detected); }
    uint32_t getSelectedBands() const { return getValue(selectedBands); }
    uint32_t getReportCount() const { return getValue(reportCount); }

    // --- HBD (124) Diagnose --------------------------------------------------
    struct HbdStatus { float f0Hz = 0, score = 0, snrDb = 0; uint8_t consistent = 0; bool detected = false; };
    void setHbd(float f0, float score, float snr, uint8_t consistent, bool det)
    { HbdStatus h{f0, score, snr, consistent, det}; setValue(hbd_, h); }
    HbdStatus getHbd() const { return getValue(hbd_); }

    // --- HBD-ML Status ------------------------------------------------------
    void setMlInitError(bool v) { setValue(mlInitError, v); }
    bool getMlInitError() const { return getValue(mlInitError); }
    void setMlRunError(bool v)  { setValue(mlRunError, v); }
    bool getMlRunError() const  { return getValue(mlRunError); }
    /// Stufe von 124 (0 HBD, 1 Schatten, 2 ML) und Abgleich HBD ↔ ML in der Stufe Schatten
    struct MlStatus { uint8_t mode = 0; float detectAgree = 1, bandOverlap = 1, meanAbsDiff = 0; uint32_t frames = 0; };
    void setMl(const MlStatus& v) { setValue(ml_, v); }
    MlStatus getMl() const { return getValue(ml_); }

    // --- Rechenzeit je Stufe (ms, geglättet) – LCD-Zeile "ms S.. P.. F.. M.. K.. R.." ----
    struct StageTimes { float sim = 0, pre = 0, feat = 0, ml = 0, corr = 0, rest = 0;
                        float corrSel = 0, corrGcc = 0, corrSrp = 0;      // corr = Sel + Gcc + Srp
                        // Diagnose: P = pre118 + asm (+ Hop holen), F = featRef + 7 · spec
                        float pre118 = 0, asm_ = 0, featRef = 0, spec = 0; };
    void setStageTimes(float pre, float feat, float ml, float corr, float rest)
    { if (!lockWrite()) return; st_.pre = pre; st_.feat = feat; st_.ml = ml; st_.corr = corr; st_.rest = rest; unlock(); }
    void setCorrTimes(float sel, float gcc, float srp)
    { if (!lockWrite()) return; st_.corrSel = sel; st_.corrGcc = gcc; st_.corrSrp = srp; unlock(); }
    void setDiagTimes(float pre118, float asmMs, float featRef, float spec)
    { if (!lockWrite()) return; st_.pre118 = pre118; st_.asm_ = asmMs; st_.featRef = featRef; st_.spec = spec; unlock(); }
    void setSimTime(float ms) { if (!lockWrite()) return; st_.sim = ms; unlock(); }
    StageTimes getStageTimes() const { return getValue(st_); }

    // --- System Status ------------------------------------------------------
    /// Unit-ID: Standard aus STM32-UID (siehe SDS110_Init), per USB-Kommando Typ 5 überschreibbar
    void setId(uint16_t v) { setValue(id, v); }
    uint16_t getId() const { return getValue(id); }
    void setMode(SDS_Mode v) { setValue(mode, v); }
    SDS_Mode getMode() const { return getValue(mode); }
    void setSimulation(uint32_t v) { setValue(simulation, v); }
    uint32_t getSimulation() const { return getValue(simulation); }
    /// wie getSimulation(), aber false bei Sperr-Timeout (out bleibt dann unverändert).
    /// getSimulation() liefert dann 0 = Hardware -> SAI-Start (Befund 29).
    bool tryGetSimulation(uint32_t& out) const { return tryGetValue(simulation, out); }
    /// SRP-Referenzscan in 126 (USB-Kommando Typ 6), Standard aus
    void setSrpReference(bool v) { setValue(srpReference, v); }
    bool getSrpReference() const { return getValue(srpReference); }
    bool tryGetSrpReference(bool& out) const { return tryGetValue(srpReference, out); }
    /// Standort lokal (Ost, Nord, Oben; USB Id 10 vom PC), Grundwert Ursprung [0, 0, 0]
    void setPosition(const sds110::LocalPosition& v) { setValue(position_, v); }
    sds110::LocalPosition getPosition() const { return getValue(position_); }
    bool tryGetPosition(sds110::LocalPosition& out) const { return tryGetValue(position_, out); }
    /// Nordabgleich (USB Id 9): Offset in Grad auf die Peilung in 128, Standard 0
    void setAzimuthOffset(float deg) { setValue(azimuthOffsetDeg_, deg); }
    float getAzimuthOffset() const { return getValue(azimuthOffsetDeg_); }
    bool tryGetAzimuthOffset(float& out) const { return tryGetValue(azimuthOffsetDeg_, out); }
    /// UTC-Versatz der Laufzeit (USB-Kommando Typ 7; später GNSS-PPS), siehe UtcClock
    void setUtcOffset(const sds110::UtcOffset& v) { setValue(utcOffset_, v); }
    sds110::UtcOffset getUtcOffset() const { return getValue(utcOffset_); }
    /// Lufttemperatur vom PC-Monitor (USB Typ 7) und daraus die Schallgeschwindigkeit
    struct Air { bool valid = false; float tempC = 0; float soundSpeed = sds110::SPEED_OF_SOUND; };
    void setAirTemperature(float tC)
    { Air a; a.valid = true; a.tempC = tC; a.soundSpeed = sds110::SoundSpeed::fromTemperature(tC); setValue(air_, a); }
    Air getAir() const { return getValue(air_); }
    bool tryGetAir(Air& out) const { return tryGetValue(air_, out); }
    /// Feedback der Tracking-Einheit (USB Id 8): Postfach mit Zähler und Empfangszeit (ms)
    struct FeedbackBox { sds110::TrackingFeedback fb{}; bool positionValid = false; uint32_t seq = 0; uint32_t tickMs = 0; };
    void setFeedback(const sds110::TrackingFeedback& fb, bool positionValid, uint32_t tickMs)
    {
        if (!lockWrite()) return;
        feedback_.fb = fb; feedback_.positionValid = positionValid; feedback_.tickMs = tickMs; ++feedback_.seq;
        unlock();
    }
    bool tryGetFeedback(FeedbackBox& out) const { return tryGetValue(feedback_, out); }
    void setSyncTimeDifference(uint32_t v) { setValue(syncTimeDifference, v); }
    uint32_t getSyncTimeDifference() const { return getValue(syncTimeDifference); }

    // --- Task-Statistiken ---------------------------------------------------
    void setTaskStats(TaskId t, uint32_t freeStack, float loopTime, uint32_t loopCounter);
    TaskStats getTaskStats(TaskId t) const;

    // --- Debug --------------------------------------------------------------
    void setDebugTime(float v)   { setValue(debugTime, v); }
    float getDebugTime() const   { return getValue(debugTime); }
    /// 0 SRP-Azimut, 1 SRP-Ratio, 2 True Azimuth, 3 True Distance, 4 Quelle aktiv (Simulator, 1/0)
    static constexpr uint32_t NUM_DEBUG_VALUES = 5;
    void setDebugValue(uint32_t i, float v);
    float getDebugValue(uint32_t i) const;

    // --- Error --------------------------------------------------------------
    void pushErrorMessage(const char* msg);
    bool popErrorMessage(SDS_ErrorMessage& out);
    void setErrorBuffer(const uint8_t* src, uint32_t len);
    uint32_t getErrorBuffer(uint8_t* dst, uint32_t maxLen) const;
    void setErrorFlag(uint32_t v)  { setValue(errorFlag, v); }
    uint32_t getErrorFlag() const  { return getValue(errorFlag); }
    void setErrorCount(uint32_t v) { setValue(errorCount, v); }
    uint32_t getErrorCount() const { return getValue(errorCount); }
    void setUsbErrorCount(uint32_t v) { setValue(usbErrorCount, v); }
    uint32_t getUsbErrorCount() const { return getValue(usbErrorCount); }

    // --- Event-Queue --------------------------------------------------------
    osMessageQueueId_t eventQueue() const { return eventQueue_; }
    void pushEvent(SDS_DataEventType type, float processTime);

private:
    SDS_Data();

    // Befund 29: Sperr-Timeouts
    //   Lesen (getX):     bei Timeout den aktuellen Wert ohne Sperre lesen statt 0 zu liefern.
    //                     Werte bis 4 Byte liest der Cortex-M7 atomar; größere Strukturen lesen
    //                     nur Anzeige und USB (höchstens eine gemischte Anzeige, kein falscher 0-Wert).
    //   tryGetX:          false bei Timeout (Aufrufer behält seinen letzten Wert, z. B. 116-Start)
    //   Schreiben (setX): nach dem ersten Timeout einmal länger warten, erst dann verwerfen
    // Timeouts entstehen vor allem bei CPU-Überlast (am Board: springende Anzeigewerte); der Mutex
    // vererbt die Priorität (FreeRTOS-Mutexe immer, osMutexPrioInherit im Konstruktor dokumentiert es).
    static constexpr uint32_t LOCK_MS = 2, WRITE_RETRY_MS = 10;
    bool lock(uint32_t timeout = LOCK_MS) const { return osMutexAcquire(mutex_, timeout) == osOK; }
    void unlock() const { osMutexRelease(mutex_); }
    bool lockRead() const { if (lock()) return true; ++lockTimeouts_; return false; }
    bool lockWrite() { if (lock() || lock(WRITE_RETRY_MS)) return true; ++lockTimeouts_; errorFlag = 999; return false; }

    template<typename T> void setValue(T& target, const T& value)
    {
        if (!lockWrite()) return;
        target = value;
        unlock();
    }
    template<typename T> bool tryGetValue(const T& target, T& out) const
    {
        if (!lockRead()) return false;
        out = target;
        unlock();
        return true;
    }
    template<typename T> T getValue(const T& target) const
    {
        const bool locked = lockRead();
        T v = target;
        if (locked) unlock();
        return v;
    }

public:
    /// Zahl der Sperr-Timeouts seit dem Start (Diagnose, LCD)
    uint32_t lockTimeouts() const { return lockTimeouts_; }

private:
    mutable volatile uint32_t lockTimeouts_ = 0;

    mutable osMutexId_t mutex_;
    osMessageQueueId_t  eventQueue_;
    osMessageQueueId_t  errorMsgQueue_;

    // Ergebnis 120
    sds110::AcousticState acousticState{};
    float    azimuthDeg = 0, distance = 0, confidence = 0;
    bool     detected = false;
    uint32_t selectedBands = 0;
    uint32_t reportCount = 0;
    bool     mlInitError = false, mlRunError = false;
    HbdStatus hbd_{};
    MlStatus  ml_{};
    StageTimes st_{};

    // System
    uint16_t id = 0;
    SDS_Mode mode = SDS_Mode::DETECT;
    uint32_t simulation = 1;
    // SRP-Referenzscan (USB Typ 6), Standard aus (ein: Scan jeden 4. Frame, 126 SRP_EVERY_N in 120); die Peilung
    // (TDOA-LS) hängt nicht davon ab. Hier statt in SDS_110_Config.hpp, weil die Config in die
    // Merkmalsversion eingeht (jede Änderung dort erzwingt einen Neuexport des Modells).
    bool     srpReference = false;
    uint32_t syncTimeDifference = 0;
    sds110::LocalPosition position_{};   // Standort (USB Id 10), Ursprung bis gesetzt
    float    azimuthOffsetDeg_ = 0.0f;   // Nordabgleich (USB Id 9), ebenfalls nicht in der Config
    sds110::UtcOffset utcOffset_{};
    Air      air_{};
    FeedbackBox feedback_{};

    TaskStats tasks_[static_cast<uint8_t>(TaskId::Count)];

    // Debug
    float debugTime = 0;
    float debugValue[NUM_DEBUG_VALUES] = {};

    // Error
    uint32_t errorFlag = 0, errorLen = 0, errorCount = 0, usbErrorCount = 0;
    uint8_t  errorBuffer_[64] = {};
};
