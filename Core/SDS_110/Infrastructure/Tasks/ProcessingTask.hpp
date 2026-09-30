/*
 * ProcessingTask.hpp  (Infrastructure/Tasks)
 * RTOS-Task um Processing_Module_120: DETECT -> processFrame(), READ -> streamFrame().
 * Ersetzt SDS/Tasks/SRPTask.
 *
 * Takt (Befund 28), waitForWork():
 *  - Simulation: fester Hop-Takt über HopClock (32 ms, osDelayUntil auf absolute Ticks);
 *    je Durchlauf alle fälligen Hops erzeugen und verarbeiten (höchstens SIM_MAX_CATCH_UP,
 *    der Rest wird übersprungen und gezählt).
 *  - Hardware: Warten auf den fertigen Hop (Thread-Flag aus dem DMA-Interrupt, 116-Hook);
 *    die SAI gibt den Takt vor. Timeout HW_TIMEOUT_MS -> Meldung "116: keine Hops".
 *  - Überlast (Durchlauf länger als ein Hop): keine Hops nachholen (fällige überspringen und
 *    zählen) und danach mindestens ein Drittel der Laufzeit schlafen (MIN_IDLE_MS … MAX_IDLE_MS).
 *    So bleiben LCD/USB/Logger (niedrigere Priorität) bei Überlast mind. ~25 % CPU; ohne diese
 *    Pause liefen deren SDS_Data-Zugriffe in den Sperr-Timeout (Werte 0, Anzeige flackert).
 * Stats "120": Bearbeitungszeit je Aufwachen (alle Hops dieses Durchlaufs).
 */
#pragma once
#include "TaskBase.hpp"
#include "Processing_Module_120/Processing_Module_120.hpp"
#include "Harness/Signal_Simulator.hpp"
#include "Infrastructure/Utils/HopClock.hpp"

namespace sds110 {

class ProcessingTask : public TaskBase {
public:
    static ProcessingTask& instance() { static ProcessingTask inst; return inst; }
    uint32_t simSkippedHops() const { return clock_.skipped(); }
protected:
    void onStart() override;
    void waitForWork() override;
    void runOnce() override;
private:
    static constexpr uint32_t FLAG_HOP         = 0x1;
    static constexpr uint32_t SIM_MAX_CATCH_UP = 4;     // Hops je Durchlauf (128 ms)
    static constexpr uint32_t HW_TIMEOUT_MS    = 100;   // > 3 Hops ohne Daten
    // Überlast: Pause nach einem zu langen Durchlauf = Laufzeit / IDLE_DIVISOR, begrenzt
    static constexpr uint32_t MIN_IDLE_MS      = 5;
    static constexpr uint32_t MAX_IDLE_MS      = 500;
    static constexpr uint32_t IDLE_DIVISOR     = 3;
    bool overloaded() const;

    ProcessingTask() : TaskBase(16384, 0, osPriorityAboveNormal) {}   // delayMs unbenutzt (waitForWork)
    void updateSource();                 // Simulation <-> Hardware umschalten, Szenario wählen
    void initSimulator(uint32_t cmd);    // Szenario aus dem Wert von USB Typ 3 (SimScenario.hpp)
    void process();                      // alle bereiten Hops/Frames je Modus
    static void onHopReadyISR(void* ctx);

    Processing_Module_120& proc_ = Processing_Module_120::instance();
    Signal_Simulator&      sim_  = Signal_Simulator::instance();
    HopClock clock_;
    bool simRunning_ = false;
    bool simOn_      = true;       // Simulator aktiv (simCmd_ != 0)
    uint32_t simCmd_     = SIM_CMD_DEFAULT;   // zuletzt gelesener Wert von SDS_Data::simulation (Standard 1)
    uint32_t simInitCmd_ = 0;                 // Wert, mit dem der Simulator zuletzt gestartet wurde
    bool hwTimeout_  = false;      // Meldung "keine Hops" nur einmal je Ausfall
    bool simOffIgnored_ = false;   // SDS110_SAI_ENABLED 0: Meldung zu Typ 3 = 0 nur einmal
    uint32_t runStartTick_ = 0, runEndTick_ = 0;
    float    simMs_ = 0;           // Simulator je Hop (ms, geglättet) -> LCD "ms S.."
    SDS_Data& dm_ = SDS_Data::instance();
};

} // namespace sds110
