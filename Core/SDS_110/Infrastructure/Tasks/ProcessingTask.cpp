/*
 * ProcessingTask.cpp  (Infrastructure/Tasks)
 */
#include "ProcessingTask.hpp"
#include "stm32f7xx_hal.h"
#include "Infrastructure/Utils/TimeBase.hpp"
#include "Infrastructure/Utils/Azimuth.hpp"

namespace sds110 {

void ProcessingTask::onStart()
{
    initSimulator(simCmd_);
    clock_.start(osKernelGetTickCount(), osKernelGetTickFreq());
    proc_.unit().sampling().setHopReadyHook(&ProcessingTask::onHopReadyISR, this);
}

void ProcessingTask::initSimulator(uint32_t cmd)
{
    SimParams sp;                       // Grundwerte: SIM_* in SDS_110_Config.hpp
    SimScenario s;
    sp.scenario = simScenarioFromCommand(cmd, s) ? s : static_cast<SimScenario>(SIM_SCENARIO_ID);
    sp.f0_hz    = SIM_F0_HZ;
    sp.snr_db   = SIM_SNR_DB;
    sim_.init(sp);
    simInitCmd_ = cmd;
}

void ProcessingTask::onHopReadyISR(void* ctx)
{
    osThreadFlagsSet(static_cast<ProcessingTask*>(ctx)->handle(), FLAG_HOP);
}

bool ProcessingTask::overloaded() const
{
    return runEndTick_ - runStartTick_ >= HOP_SAMPLES * osKernelGetTickFreq() / SAMPLE_RATE_HZ;
}

void ProcessingTask::waitForWork()
{
    if (overloaded()) {
        uint32_t idle = (runEndTick_ - runStartTick_) / IDLE_DIVISOR;
        idle = idle < MIN_IDLE_MS ? MIN_IDLE_MS : (idle > MAX_IDLE_MS ? MAX_IDLE_MS : idle);
        osDelay(idle);
    }
    if (simOn_) {
        osDelayUntil(clock_.nextTick());         // bereits fällig: kehrt sofort zurück
        return;
    }
    const uint32_t r = osThreadFlagsWait(FLAG_HOP, osFlagsWaitAny, HW_TIMEOUT_MS);
    const bool timeout = (r & osFlagsError) != 0;
    if (timeout && proc_.unit().sampling().running() && !hwTimeout_) dm_.pushErrorMessage("116: keine Hops");
    hwTimeout_ = timeout;
}

void ProcessingTask::updateSource()
{
    // Simulation (USB-Kommando Typ 3, Standard 1): Generator statt SAI/DMA, Wert wählt das Szenario
    // (SimScenario.hpp). Sperr-Timeout: letzten Wert behalten, nicht auf Hardware umschalten (Befund 29)
    uint32_t simCmd = simCmd_;
    if (dm_.tryGetSimulation(simCmd)) simCmd_ = simCmd;
    simOn_ = simCmd_ != SIM_CMD_OFF;
    if (simOn_ && simCmd_ != simInitCmd_) initSimulator(simCmd_);   // anderes Szenario: neu beginnen
    if (simOn_ && !simRunning_) {
        proc_.unit().sampling().stop();
        clock_.start(osKernelGetTickCount(), osKernelGetTickFreq());   // Takt neu ab jetzt
        simRunning_ = true;
    }
    if (!simOn_ && simRunning_) {
        simRunning_ = false;
        hwTimeout_  = false;
        osThreadFlagsClear(FLAG_HOP);
        if (!proc_.unit().sampling().dmaReady()) dm_.pushErrorMessage("116: SAI ohne DMA");
        else if (!proc_.start())                 dm_.pushErrorMessage("116 start failed");
    }
}

void ProcessingTask::process()
{
    switch (dm_.getMode()) {
        case SDS_Mode::DETECT:
        case SDS_Mode::CALIBRATE:          // Nordabgleich am PC braucht Peilungen (UnitReport)
            while (proc_.processFrame()) {}
            break;
        case SDS_Mode::READ:
            while (proc_.streamFrame()) {}
            break;
        default:
            break;
    }
}

void ProcessingTask::runOnce()
{
    runStartTick_ = osKernelGetTickCount();
    updateSource();
    if (simOn_) {
        // alle fälligen Hops, je Hop sofort verarbeiten (114 hat nur NUM_MIC_FRAMES Puffer)
        // Überlast: nicht nachholen – jeder weitere Hop verlängert den Durchlauf nur
        SDS_Data::Air air;                           // simulierte Luft wie in 126 (USB Typ 7)
        if (dm_.tryGetAir(air)) sim_.setSpeedOfSound(air.soundSpeed);
        LocalPosition pos;                           // Standort (USB Typ 10): FlyBy-Bahn um den Ursprung
        if (dm_.tryGetPosition(pos)) sim_.setUnitPosition(pos.eastM(), pos.northM(), pos.upM());
        const uint32_t n = clock_.due(osKernelGetTickCount(), overloaded() ? 1 : SIM_MAX_CATCH_UP);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t c0 = DWTTimer::instance().cycles();
            sim_.generateHop(TimeBase::nowUs());     // gleiche Zeitbasis wie 116
            simMs_ += 0.1f * (DWTTimer::instance().cyclesToUs(DWTTimer::instance().cycles() - c0) / 1000.0f - simMs_);
            process();
        }
        dm_.setSimTime(simMs_);
        // LCD: "True Azimuth". Der Simulator setzt die Quelle im Array-System (Mikrofon 0 = 0°);
        // 128 addiert den Nordabgleich (USB Typ 9) auf die Peilung. Ohne den Offset hier wäre
        // "Dif Azimuth" genau der Offset (z. B. 136° nach einem Abgleich mit DroneSweep).
        dm_.setDebugValue(2, Azimuth::wrap360(sim_.trueAzimuth() + dm_.getAzimuthOffset()));
        dm_.setDebugValue(3, sim_.trueDistance());    // LCD: "True Distance"
        dm_.setDebugValue(4, sim_.sourceActive() ? 1.0f : 0.0f);   // LCD: FlyBy-Pause
    } else {
        process();
    }
    reportStats(TaskId::Proc120);
    runEndTick_ = osKernelGetTickCount();
}

} // namespace sds110
