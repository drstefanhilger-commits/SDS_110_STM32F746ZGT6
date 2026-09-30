/*
 * Frame_Assembler.hpp
 *
 * Analyse-Frames mit Überlappung (Patent, Abschnitt 2: 64 ms, 50 %):
 * Gleitendes Fenster über die letzten FRAME_SAMPLES / HOP_SAMPLES Hops aus 114, nach 118.
 * Jeder neue Hop schiebt das Fenster um HOP_SAMPLES weiter; ab dem zweiten Hop liegt
 * je Hop ein vollständiger Frame vor (Takt 32 ms).
 *
 * Fehlt ein Hop (frame_id nicht fortlaufend, z. B. verworfen in 114 oder Moduswechsel),
 * beginnt das Fenster neu – ein Frame enthält nie zeitlich getrennte Stücke.
 *
 * Speicher (STM32F746ZGT6, kein SDRAM): keine eigenen Slots mehr. Der Assembler übernimmt die
 * von 118 in place verarbeiteten Hop-Puffer aus 114 (MicFrame, Blockgleitkomma) und gibt sie an
 * 114 zurück, sobald sie nicht mehr gebraucht werden: beim Weiterschieben des Fensters, bei einer
 * Lücke, bei reset() und über releaseOldest(), sobald alle Spektren eines Frames berechnet sind
 * (Processing_Module_120). So belegt das Analysefenster höchstens zwei Puffer des 114-Pools.
 */
#pragma once
#include <cstdint>
#include "SDS_110_Config.hpp"
#include "Microphone_Array_114.hpp"

namespace sds110 {

/// Analyse-Frame als Sicht auf zwei Hops (älterer, neuerer; je HOP_SAMPLES Samples je Kanal).
/// 122 dekodiert die beiden Teile je Kanal in seinen FFT-Puffer (decode()).
struct AnalysisFrame {
    static constexpr uint32_t PARTS = FRAME_SAMPLES / HOP_SAMPLES;
    uint32_t        frame_id;                 // fortlaufend je vollständigem Frame
    uint64_t        time_utc_us;              // Zeit des ersten Samples im Frame
    const MicFrame* hop[PARTS];               // hop[0] älterer Hop, hop[1] neuerer
    float sample(uint32_t ch, uint32_t i) const { return hop[i / HOP_SAMPLES]->sample(ch, i % HOP_SAMPLES); }
    /// Kanal ch komplett (FRAME_SAMPLES Werte) nach dst
    void decode(uint32_t ch, float* dst) const
    { for (uint32_t p = 0; p < PARTS; ++p) hop[p]->decode(ch, dst + p * HOP_SAMPLES); }
};

class Frame_Assembler {
public:
    static constexpr uint32_t HOPS_PER_FRAME = AnalysisFrame::PARTS;
    static_assert(HOPS_PER_FRAME * HOP_SAMPLES == FRAME_SAMPLES, "Frame = ganze Zahl von Hops");

    /// Hops freigeben, Fenster beginnt neu
    void reset();

    /// Von 118 verarbeiteten Hop übernehmen (Besitz geht an den Assembler, Rückgabe an 114
    /// erfolgt hier). true, wenn frame() einen vollständigen Frame enthält.
    bool push(MicFrame* hop);

    /// Ältesten Hop des vollständigen Frames an 114 zurückgeben (frame() danach ungültig);
    /// der neuere bleibt für den nächsten Frame. Ohne vollständigen Frame wirkungslos.
    void releaseOldest();

    const AnalysisFrame& frame() const { return frame_; }
    uint32_t held() const { return fill_; }

private:
    MicFrame* hops_[HOPS_PER_FRAME] = {};     // ältester Hop zuerst
    AnalysisFrame frame_{};
    uint32_t fill_ = 0;                       // Hops im Fenster (0 .. HOPS_PER_FRAME)
    uint32_t lastHopId_ = 0;
    bool     haveLast_ = false;
    uint32_t nextFrameId_ = 0;
    void dropFront();
};

} // namespace sds110
