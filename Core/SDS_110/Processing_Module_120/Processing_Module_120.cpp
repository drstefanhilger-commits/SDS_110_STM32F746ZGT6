/*
 * Processing_Module_120.cpp
 */
#include "Processing_Module_120.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"
#include "Infrastructure/Driver/USBDriver.hpp"
#include "Infrastructure/Utils/DWT.hpp"
#include <cmath>

namespace sds110 {

Spectrum Processing_Module_120::spectra_[NUM_MICS];               // nur Band-Bins (SPECTRUM_BINS)
Feature_Extraction_Module_122 Processing_Module_120::featInst_;   // .bss, internes RAM
Machine_Learning_Module_124   Processing_Module_120::mlInst_;
Correlation_Processing_Module_126 Processing_Module_120::corrInst_;

Processing_Module_120& Processing_Module_120::instance()
{
    // Instanz im internen RAM (STM32F746ZGT6: kein SDRAM); Konstruktor beim ersten Aufruf (SDS110_Init)
    static Processing_Module_120 inst;
    return inst;
}

// SRP-Referenzscan (USB Typ 6) nur in jedem n-ten Frame: reiner Vergleich, 8 Peilungen/s genügen.
// Hier statt in SDS_110_Config.hpp, weil die Config in die Merkmalsversion eingeht.
static constexpr uint32_t SRP_EVERY_N = 4;

bool Processing_Module_120::init(SAI_HandleTypeDef* hsai, I2C_HandleTypeDef* hi2c)
{
    SDS_Data& dm = SDS_Data::instance();
    const bool unitOk = unit_.init(hsai, hi2c);
    if (!unitOk) dm.pushErrorMessage(unit_.sampling().errorCount() ? "116: SAI clock / I2C" : "116 init failed");

    feat_.init();
    const bool mlOk = ml_.init();
    dm.setMlInitError(!mlOk);
    corr_.init(unit_.array());
    corr_.setSrpEvery(SRP_EVERY_N);        // SRP-Referenzscan nur jeden 4. Frame (Vergleich, kein Patentpfad)
    const Vec3 origin{0, 0, 0};
    loc_.init(&origin, 1);                 // NUM_UNITS = 1: Referenzpunkt = Arraymitte
    out_.init();
    return unitOk;
}

bool Processing_Module_120::start() { return unit_.start(); }

// Rechenzeit je Stufe (DWT-Zyklen -> ms), gleitend geglättet (~10 Frames)
static inline void smoothMs(float& acc, uint32_t cycles)
{
    const float ms = DWTTimer::instance().cyclesToUs(cycles) / 1000.0f;
    acc += 0.1f * (ms - acc);
}

bool Processing_Module_120::processFrame()
{
    SDS_Data& dm = SDS_Data::instance();
    DWTTimer& dwt = DWTTimer::instance();
    const uint32_t c0 = dwt.cycles();
    const AnalysisFrame* frame = nullptr;
    if (!unit_.nextFrame(frame)) return false;   // 112: Hop + 118 + Analysefenster
    smoothMs(tPre_, dwt.cycles() - c0);
    if (!frame) return true;                     // Hop verbraucht, Fenster füllt noch
    const uint32_t c1 = dwt.cycles();

    // (b) 122: STFT Referenzkanal + Merkmale, dann übrige Kanäle für 126
    feat_.process(*frame, spectra_[REF_MIC], features_);
    const uint32_t c1a = dwt.cycles();
    for (uint32_t m = 0; m < NUM_MICS; ++m)
        if (m != REF_MIC) feat_.computeSpectrum(*frame, m, spectra_[m]);
    const uint64_t t = frame->time_utc_us;
    unit_.releaseOldest();                       // Spektren fertig: älteren Hop an 114 zurück (frame ungültig)
    const uint32_t c2 = dwt.cycles();
    smoothMs(tFeat_, c2 - c1);
    // Diagnose Rechenlast: 118 / Fenster (112), 122 Referenzkanal (FFT + Merkmale), je weiteres Mikrofon
    smoothMs(tDiag118_, unit_.lastPreCycles());
    smoothMs(tDiagAsm_, unit_.lastAsmCycles());
    smoothMs(tDiagRef_, c1a - c1);
    smoothMs(tDiagSpec_, (c2 - c1a) / (NUM_MICS - 1));

    // (c) 124: Acoustic State s(t)
    if (!ml_.infer(feat_.magnitude(), features_, state_)) { dm.setMlRunError(true); return true; }
    const uint32_t c3 = dwt.cycles();
    smoothMs(tMl_, c3 - c2);
    dm.setMlRunError(false);
    dm.setAcousticState(state_);
    dm.setHbd(ml_.hbd().f0Hz, ml_.hbd().score, ml_.hbd().globalSnrAvgDb, ml_.hbd().consistentBands, ml_.hbd().droneDetected);
    const auto& sh = ml_.shadow();
    dm.setMl({ static_cast<uint8_t>(ml_.mode()), sh.detectAgreement(), sh.bandOverlap(), sh.meanAbsDiff(), sh.frames });

    // Feedback der Tracking Unit (Abschnitt 10), falls vorhanden
    if (out_.pollFeedback(feedback_, osKernelGetTickCount())) corr_.applyFeedback(feedback_);   // USB Id 8

    // (d)(e) 126: Selektion, Gewichtung, quellkonditionierte GCC-PHAT, Peilung
    const uint32_t c4 = dwt.cycles();
    bool srpOn = corr_.srpReference();                 // USB Typ 6; bei Sperr-Timeout alter Wert
    if (dm.tryGetSrpReference(srpOn)) corr_.setSrpReference(srpOn);
    SDS_Data::Air air;                                 // Lufttemperatur (USB Typ 7) -> c
    if (dm.tryGetAir(air)) { corr_.setSpeedOfSound(air.soundSpeed); loc_.setSpeedOfSound(air.soundSpeed); }
    corr_.deriveSelection(state_, selection_);
    const uint32_t c4a = dwt.cycles();
    corr_.estimateBearing(spectra_, selection_, bearing_);
    const uint32_t c4b = dwt.cycles();
    // Vergleich TDOA-LS (Patent) vs. SRP-PHAT (alt), nur in jedem SRP_EVERY_N-ten Frame (126)
    float srpAz = 0.0f, srpPow = 0.0f, srpRatio = 0.0f;
    const bool srpOk = corr_.srpReference() && corr_.srpScan(srpAz, srpPow, srpRatio);
    const uint32_t c5 = dwt.cycles();                  // Zeitfenster SRP ohne die Mutex-Aufrufe
    // Nordabgleich (USB Typ 9) vor der SRP-Anzeige übernehmen: SRP-Referenz und Peilung am LCD
    // im selben (abgeglichenen) System wie "True Azimuth" (ProcessingTask)
    float azOffset = 0.0f;
    if (dm.tryGetAzimuthOffset(azOffset)) loc_.setCalibration(azOffset, 1.0f);
    if (srpOk) { dm.setDebugValue(0, loc_.calibrate(srpAz)); dm.setDebugValue(1, srpRatio); }
    smoothMs(tCorr_, c5 - c4);
    smoothMs(tSel_, c4a - c4);                         // Selektion S(t), Gewichte
    smoothMs(tGcc_, c4b - c4a);                        // 28 Paar-Korrelationen + LS-Peilung
    smoothMs(tSrp_, c5 - c4b);                         // SRP-Referenzscan

    // (f) 128: Kandidatenposition (Einzel-Unit: Peilung + Pegel-Fallback)
    // levelA aus dem Referenzspektrum nach 118; durch die dort angewendete Verstärkung
    // (NS · AGC, in der Frame-Mitte) teilen, sonst misst der Pegel die AGC statt der Quelle
    float levelA = 0.0f;
    for (uint32_t b = 0; b < NUM_BANDS; ++b) levelA += std::exp(features_.band_log_power[b]) * state_.p[b];
    const float gRef = unit_.preprocessor().frameCenterGain(REF_MIC);
    levelA = std::sqrt(levelA) / ((gRef > 1e-6f) ? gRef : 1e-6f);
    loc_.fromBearing(bearing_, levelA, location_);
    dm.setCandidate(location_.azimuth_deg, location_.distance_m, location_.confidence, location_.valid);

    // UnitReport über 140 an das Processing Module (PC); der Candidate Report (g) entsteht dort
    if (location_.valid && dm.getDetected()) {
        out_.buildReport(location_, state_, selection_, levelA, t, report_);
        out_.send(report_);
    }
    dm.pushEvent(SDS_DataEventType::REPORT_UPDATE, 0.0f);
    // Rest: SDS_Data-Aufrufe zwischen den Stufen, 128, 130
    smoothMs(tRest_, (dwt.cycles() - c1) - (c2 - c1) - (c3 - c2) - (c5 - c4));
    dm.setStageTimes(tPre_, tFeat_, tMl_, tCorr_, tRest_);
    dm.setCorrTimes(tSel_, tGcc_, tSrp_);
    dm.setDiagTimes(tDiag118_, tDiagAsm_, tDiagRef_, tDiagSpec_);
    return true;
}

bool Processing_Module_120::streamFrame()
{
    MicFrame* frame = unit_.nextHop();           // READ: Rohdaten-Hops (vor 118) ohne Überlappung
    if (!frame) return false;
    const uint32_t ts = static_cast<uint32_t>(frame->time_utc_us / 1000ULL);
    // 96 x 532 B je Hop (~1,6 MB/s) übersteigt USB-FS: auf Platz im TX-Puffer warten;
    // gelingt das nicht, Rest des Frames verwerfen statt einzelne Pakete zu verlieren.
    // Nicht gestreamte Frames zählt 114 als dropped. Die Hop-Nummer (frame_id) im Kopf zeigt
    // dem PC jede Lücke (Befund 35).
    constexpr uint32_t kWaitMs = 20;
    bool ok = true;
    for (uint32_t m = 0; m < NUM_MICS && ok; ++m)
        for (uint32_t f = 0; f < HOP_SAMPLES / SDS_MSG_BUFFER_SIZE && ok; ++f)
            ok = USBDriver::sendRead(ts, m, f, frame->frame_id, frame, kWaitMs);
    unit_.releaseHop(frame);
    return true;
}

} // namespace sds110
