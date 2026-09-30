/*
 * Correlation_Processing_Module_126.hpp
 *
 * Korrelationsmodul 126 (Patent, Abschnitte 4, 5, 10; FIG. 2 unten, FIG. 3, FIG. 8):
 *  (d)(i)  Komponentenselektion S(t) = { k : p_b(k)(t) > θ_sel }
 *  (d)(ii) Gewichtung w(t,k) = g(p_b(t)) = p^γ
 *  (e)     Quellkonditionierte GCC-PHAT: R_ij(k) = w(k) · X_i X_j* / |X_i X_j*| nur auf S(t),
 *          IFFT -> Kreuzkorrelation, Peak im Fenster ±τ_max, Peak-Ratio-Test -> TDOA τ_ij
 *          Schnellpfad: bei höchstens directMaxBins() selektierten Bins wird die Korrelation
 *          nur für die nötigen Lags (±31 bei 20 °C) direkt aus den Bins berechnet (identisch zur IFFT bis auf
 *          Rundung, t_gcc_direct) – statt 28 inverser FFTs über N_FFT Werte je Frame.
 *  Feedback (Abschnitt 10): ŝ senkt θ_sel für Referenzbänder, x̂ verengt das Suchfenster.
 *
 *  Inter-Unit (N ≥ 3): crossCorrelate() auf den Referenzkanal-Spektren zweier Units.
 *  Intra-Unit (Peilung, "no beamforming is required"): estimateBearing() korreliert die
 *  8 Mikrofone einer Unit paarweise und löst die Fernfeld-Richtung u per gewichteter LS.
 *
 * Migration aus SDS/Algorithm/SRPPhat + SDS_SRPBuffers:
 *  - computePHATIFFT -> crossCorrelate (mit Selektion/Gewichtung, CMSIS statt kiss_fft)
 *  - stepAzimuthScan (SRP-Gitter über 360 Hypothesen) -> estimateBearing (TDOA-LS, kein Scan)
 *  - calibrateAzimuth/filterAzimuth -> entfallen (Kalibrierung in 128, Glättung ist Tracking 150)
 */
#pragma once
#include "arm_math.h"
#include "Infrastructure/Utils/DspScratch.hpp"
#include "SDS_110_Config.hpp"
#include "Data_Interface_140/Candidate_Report_140.hpp"
#include "Sensor_Unit_112/Microphone_Array_114.hpp"
#include "Processing_Module_120/Feature_Extraction_Module_122/Feature_Extraction_Module_122.hpp"

namespace sds110 {

struct ComponentSelection {
    bool     selected[SPECTRUM_BINS] = {};   // nur Band-Bins (< SPECTRUM_BINS) können selektiert sein
    float    weight  [SPECTRUM_BINS] = {};
    uint32_t num_bands = 0;      // selektierte Bänder
    uint32_t num_bins  = 0;      // selektierte Bins
};

struct TdoaMeasurement {
    uint32_t i = 0, j = 0;       // Unit- oder Mikrofonindizes
    float    tdoa_s = 0.0f;
    float    peak = 0.0f;
    float    peak_ratio = 0.0f;
    bool     valid = false;
};

struct Bearing {
    float    azimuth_deg = 0.0f;
    float    residual = 0.0f;    // gewichtetes LS-Residuum (s)
    uint8_t  valid_pairs = 0;
    float    mean_peak = 0.0f;   // mittlere GCC-PHAT-Peakhöhe (Pegelmaß für 128-Fallback)
    bool     valid = false;
};

class Correlation_Processing_Module_126 {
public:
    void init(const Microphone_Array_114& array);

    /// Abschnitt 4: S(t), w(t,k) aus s(t)
    void deriveSelection(const AcousticState& state, ComponentSelection& sel) const;

    /// Abschnitt 5: quellkonditionierte GCC-PHAT eines Paars, Suchfenster ±maxDelay_s
    bool crossCorrelate(const Spectrum& Xi, const Spectrum& Xj, const ComponentSelection& sel,
                        float maxDelay_s, TdoaMeasurement& out);

    /// Intra-Unit-Peilung aus allen Mikrofonpaaren einer Unit (TDOA-Least-Squares, Patentpfad)
    bool estimateBearing(const Spectrum* micSpectra, const ComponentSelection& sel, Bearing& out);

    /// Referenz: SRP-PHAT-Scan über die in estimateBearing() gespeicherten Paarkorrelationen
    /// (klassisches Verfahren aus SDS/Algorithm/SRPPhat, hier ohne eigene FFTs). Nur Vergleich.
    /// false, wenn der Referenzscan abgeschaltet ist (setSrpReference(false)).
    bool srpScan(float& azimuth_deg, float& peakPower, float& peakRatio) const;
    /// Referenzscan zur Laufzeit ein/aus (Standard ein; 120 setzt ihn aus SDS_Data, USB Typ 6).
    /// Aus: estimateBearing() sichert die Paarkorrelationen nicht, srpScan() liefert false.
    void setSrpReference(bool on) { if (on != srpOn_) srpCount_ = 0; srpOn_ = on; srpValid_ = false; }
    bool srpReference() const { return SRP_REFERENCE_ENABLED && srpOn_; }
    /// Referenzscan nur in jedem n-ten estimateBearing() (Standard 1 = jeder Frame; 120 nutzt 4).
    /// Nur in diesen Frames werden die Paarkorrelationen gesichert und das Lag-Fenster erweitert.
    void setSrpEvery(uint32_t n) { srpEvery_ = (n == 0) ? 1 : n; srpCount_ = 0; }
    /// Raster des Scans: grob alle SRP_COARSE_STEP Grad, dann ±SRP_FINE_HALF Grad im 1°-Raster
    static constexpr uint32_t SRP_COARSE_STEP = 5, SRP_FINE_HALF = 4;

    /// Abschnitt 10: Feedback der Tracking Unit
    void applyFeedback(const TrackingFeedback& fb);
    void clearFeedback();

    const TdoaMeasurement* lastPairTdoa() const { return pairTdoa_; }

    /// Befund 26: Mehrdeutigkeit bei hohem f0 (Paarkorrelation mit mehreren fast gleich hohen
    /// Spitzen; eine vertauschte Spitze liegt ≥ eine Periode der höchsten genutzten Frequenz daneben,
    /// ≥ 12 Samples). Nach der LS-Lösung wird das größte Paar-Residuum in Samples geprüft:
    ///  (1) robuste LS: über OUTLIER_SAMPLES das Paar mit dem größten Residuum verwerfen und neu
    ///      lösen (mindestens BEARING_MIN_PAIRS_FRACTION der Paare bleiben); Rauschen bleibt fast
    ///      immer darunter und kostet keine Paare
    ///  (2) bleibt ein Residuum darüber oder sind zu wenige Paare eindeutig: Richtung aus dem SRP-Scan
    ///      der gesicherten Paarkorrelationen; Paare mit mehr als CONSIST_MAX_SAMPLES Abweichung
    ///      nehmen die Spitze ±REPICK_HALF Samples um die vorhergesagte Verzögerung; Paare über
    ///      OUTLIER_SAMPLES verwerfen; gilt, wenn danach alle Residuen ≤ OUTLIER_SAMPLES
    ///  (3) sonst ungültig – keine falsche Peilung melden
    /// Die Peilung bleibt TDOA-LS (Patentpfad), SRP löst nur die Mehrdeutigkeit auf.
    static constexpr float CONSIST_MAX_SAMPLES = 4.0f;
    static constexpr float OUTLIER_SAMPLES = 8.0f;
    static constexpr int   REPICK_HALF = 5;          // < halbe Periode bei 4 kHz (12 Samples)
    bool     lastWasGuided() const { return guided_; }          ///< letzte Peilung per SRP geführt
    uint32_t lastRejectedPairs() const { return rejected_; }    ///< danach verworfene Paare

    /// Schnellpfad bis zu so vielen selektierten Bins (darüber IFFT); 0 = immer IFFT (Tests)
    static constexpr uint32_t DIRECT_MAX_BINS = 128;
    void     setDirectMaxBins(uint32_t n) { directMaxBins_ = n < DIRECT_MAX_BINS ? n : DIRECT_MAX_BINS; }
    uint32_t directMaxBins() const { return directMaxBins_; }
    float    maxIntraDelay() const { return maxIntraDelay_s_; }   ///< Intra-Unit-Fenster (s)
    /// Schallgeschwindigkeit (m/s, aus der Lufttemperatur): Paarverzögerungen, Lag-Fenster
    /// und Peilung neu; Standard SPEED_OF_SOUND. Ein Aufruf mit unverändertem c kostet nichts.
    void     setSpeedOfSound(float c);
    float    speedOfSound() const { return c_; }
    int      lastWindowHalf() const { return winHalf_; }             ///< Lags des Schnellpfads
    bool     lastWasDirect() const { return direct_; }

private:
    // Lags, die der Schnellpfad berechnet (winHalf_, je Frame): Peak-Suche braucht ±(maxLag+1)
    // (Intra-Unit bei 200 mm: 30 bei 20 °C, 34 bei −40 °C), der SRP-Scan ±SRP_MAX_LAG, wenn er
    // läuft. Puffer bis WIN_MAX; größere maxLag (zwischen Einheiten) -> IFFT
    static constexpr int WIN_MAX = 40;
    static_assert(WIN_MAX >= static_cast<int>(SRP_MAX_LAG), "Schnellpfad muss das SRP-Fenster abdecken");
    void  updateGeometry();                    // pairDx_/Dy_, maxIntraDelay_s_ aus micPos_ und c_

    /// Bins und Drehzeiger für den Schnellpfad vorbereiten (einmal je Frame); false -> IFFT
    bool  prepareBins(const ComponentSelection& sel, int maxLag);
    bool  correlatePair(const Spectrum& X, const Spectrum& Y, const ComponentSelection& sel,
                        int maxLag, TdoaMeasurement& out);
    float lagValue(int lag) const
    { return direct_ ? win_[lag + WIN_MAX] : corr_[(lag + static_cast<int>(N_FFT)) % static_cast<int>(N_FFT)]; }
    static int maxLagFor(float maxDelay_s);

    uint32_t directMaxBins_ = DIRECT_MAX_BINS;
    bool     srpOn_ = true;
    bool     srpValid_ = false;          // pairCorr_ stammt aus dem letzten estimateBearing()
    bool     srpFrame_ = false;          // dieser Frame sichert pairCorr_ (jeder srpEvery_-te)
    uint32_t srpEvery_ = 1, srpCount_ = 0;
    bool     direct_ = false;
    uint32_t nBins_ = 0;
    uint16_t binK_[DIRECT_MAX_BINS];
    float    binW_[DIRECT_MAX_BINS], binCos_[DIRECT_MAX_BINS], binSin_[DIRECT_MAX_BINS];
    int      winHalf_ = 0;
    static float win_[2 * WIN_MAX + 1];   // Korrelation für Lag -winHalf_ … +winHalf_ (Mitte WIN_MAX)

    float thetaSel_[NUM_BANDS];
    float weightBoost_[NUM_BANDS];
    TrackingFeedback feedback_{};

    Vec3  micPos_[NUM_MICS];
    float maxIntraDelay_s_ = 0.0f;   // größter Mikrofonabstand / c · 1,1
    float dmax_ = 0.0f;              // größter Mikrofonabstand (m)
    float c_ = SPEED_OF_SOUND;       // Schallgeschwindigkeit (m/s)
    arm_rfft_fast_instance_f32 ifft_;
    // Arbeitspuffer der 28 Paar-Korrelationen je Frame (je 16 kB, bei jedem Paar komplett
    // geschrieben/gelesen): im gemeinsamen DSP-Arbeitsspeicher (DspScratch.hpp)
    float* const spec_ = dspScratch();           // gepacktes Spektrum für die IFFT
    float* const corr_ = dspScratch() + N_FFT;   // Kreuzkorrelation (zeitlich)
    TdoaMeasurement pairTdoa_[NUM_MIC_PAIRS];
    // Fenster ±SRP_MAX_LAG jeder Paarkorrelation für srpScan()
    float pairCorr_[NUM_MIC_PAIRS][2 * SRP_MAX_LAG + 1];
    float pairDx_[NUM_MIC_PAIRS], pairDy_[NUM_MIC_PAIRS];   // (p_j - p_i)/c * fs
    float srpCos_[SRP_AZ_STEPS], srpSin_[SRP_AZ_STEPS];     // Richtungen des SRP-Rasters (init)
    float srpAt(uint32_t step) const;                       // SRP-Leistung einer Rasterrichtung
    /// SRP-Scan über pairCorr_: Rasterschritt des Maximums (+ Parabel-Versatz), Maximum, zweitbester
    /// Punkt des Grobrasters; false, wenn kein positives Maximum
    bool  scanSrp(int& bestStep, float& delta, float& best, float& second) const;
    // Befund 26: LS-Lösung aus pairTdoa_, größtes Residuum, Spitzen um eine Richtung neu wählen
    bool  solveLs(float& ux, float& uy, uint32_t& valid, float& peakSum) const;
    float maxResidualSamples(float ux, float uy) const;
    void  repickPairs(float ux, float uy, int maxLag);
    bool  guided_ = false;
    uint32_t rejected_ = 0;
};

} // namespace sds110
