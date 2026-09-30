/*
 * Output_Interface_130.cpp
 */
#include "Output_Interface_130.hpp"
#include "Infrastructure/Driver/USBDriver.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"
#include <cstring>

namespace sds110 {

bool Output_Interface_130::init() { sent_ = 0; return true; }

void Output_Interface_130::buildReport(const CandidateLocation& loc, const AcousticState& s,
                                       const ComponentSelection& sel, float level, uint64_t t, UnitReport& r) const
{
    r = UnitReport{};
    SDS_Data& dm = SDS_Data::instance();
    r.unit_id          = dm.getId();
    const UtcOffset utc = dm.getUtcOffset();      // Sperr-Timeout: Laufzeit, Quelle Uptime
    r.time_utc_us      = UtcClock::toReport(t, utc);
    r.time_source      = utc.source;
    r.bearing_deg      = loc.azimuth_deg;
    r.bearing_residual = loc.ls_residual;
    r.confidence       = loc.confidence;
    r.valid_pairs      = loc.accepted_pairs;
    r.level            = level;
    r.num_selected     = 0;
    for (uint32_t b = 0; b < NUM_BANDS; ++b) {
        uint32_t k0, k1; Feature_Extraction_Module_122::bandBins(b, k0, k1);
        if (k0 < SPECTRUM_BINS && sel.selected[k0]) {
            r.band_index[r.num_selected] = static_cast<uint8_t>(b);
            r.band_prob [r.num_selected] = s.p[b];
            ++r.num_selected;
        }
    }
}

bool Output_Interface_130::send(const UnitReport& r)
{
    const uint32_t ts = static_cast<uint32_t>(r.time_utc_us / 1000ULL);   // Kopf: ms (32 bit, läuft über)
    const float distFallback = (SINGLE_UNIT_LEVEL_DISTANCE && r.level > 0.0f) ? LEVEL_DIST_K_REF / (r.level + LEVEL_DIST_EPS) : 0.0f;

    // 1) Legacy-Frame für den bestehenden PC-Monitor
    bool ok = USBDriver::sendDetection(ts, r.unit_id, r.bearing_deg, distFallback, r.confidence);

    // 2) UnitReport (id 5, little-endian):
    //    [unit u16][time_us u64][src u8][bearing f32][residual f32][pairs u8][nsel u8][level f32]
    //    [idx u8 x nsel][p u8 x nsel]      src: 0 Laufzeit, 1 UTC vom PC, 2 GNSS-PPS
    MessageData d{};
    uint32_t o = 0;
    std::memcpy(&d.b[o], &r.unit_id, 2);          o += 2;
    std::memcpy(&d.b[o], &r.time_utc_us, 8);      o += 8;
    d.b[o++] = static_cast<uint8_t>(r.time_source);
    std::memcpy(&d.b[o], &r.bearing_deg, 4);      o += 4;
    std::memcpy(&d.b[o], &r.bearing_residual, 4); o += 4;
    // Nutzlast 128 Byte: UNIT_REPORT_HEAD Byte Kopf + 2 Byte je Band -> höchstens 51 Bänder. Im
    // Feld nsel steht die tatsächlich gesendete Anzahl, sonst liest der PC über das Ende hinaus.
    constexpr uint32_t kMaxBands = (sizeof(MessageData) - UNIT_REPORT_HEAD) / 2;   // 51
    const uint32_t n = (r.num_selected > kMaxBands) ? kMaxBands : r.num_selected;
    d.b[o++] = r.valid_pairs;
    d.b[o++] = static_cast<uint8_t>(n);
    std::memcpy(&d.b[o], &r.level, 4);            o += 4;
    for (uint32_t i = 0; i < n; ++i) d.b[o++] = r.band_index[i];
    for (uint32_t i = 0; i < n; ++i) d.b[o++] = static_cast<uint8_t>(r.band_prob[i] * 255.0f);
    ok = USBDriver::sendMessage(UNIT_REPORT_ID, ts, d) && ok;

    if (ok) ++sent_;
    return ok;
}

bool Output_Interface_130::pollFeedback(TrackingFeedback& fb, uint32_t nowMs)
{
    SDS_Data::FeedbackBox box;
    if (!SDS_Data::instance().tryGetFeedback(box)) return false;     // Sperr-Timeout: nächster Frame
    if (box.seq != fbSeq_) {                                        // neues Feedback
        fbSeq_ = box.seq;
        fb = box.fb;
        fbActive_ = fb.valid;
        return true;
    }
    if (fbActive_ && nowMs - box.tickMs > FEEDBACK_TIMEOUT_MS) {    // abgelaufen -> zurücksetzen
        fbActive_ = false;
        fb = TrackingFeedback{};
        return true;
    }
    return false;
}

} // namespace sds110
