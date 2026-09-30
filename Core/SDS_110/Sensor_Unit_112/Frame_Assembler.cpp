/*
 * Frame_Assembler.cpp
 */
#include "Frame_Assembler.hpp"

namespace sds110 {

void Frame_Assembler::dropFront()
{
    if (fill_ == 0) return;
    Microphone_Array_114::instance().release(hops_[0]);
    for (uint32_t k = 0; k + 1 < HOPS_PER_FRAME; ++k) hops_[k] = hops_[k + 1];
    hops_[HOPS_PER_FRAME - 1] = nullptr;
    --fill_;
}

void Frame_Assembler::reset()
{
    while (fill_) dropFront();
    haveLast_ = false;
}

bool Frame_Assembler::push(MicFrame* hop)
{
    if (!hop) return false;
    if (haveLast_ && hop->frame_id != lastHopId_ + 1) while (fill_) dropFront();   // Lücke: neu beginnen
    if (fill_ == HOPS_PER_FRAME) dropFront();                                      // Fenster weiterschieben
    hops_[fill_++] = hop;
    lastHopId_ = hop->frame_id; haveLast_ = true;
    if (fill_ < HOPS_PER_FRAME) return false;
    for (uint32_t k = 0; k < HOPS_PER_FRAME; ++k) frame_.hop[k] = hops_[k];
    frame_.time_utc_us = hops_[0]->time_utc_us;
    frame_.frame_id    = nextFrameId_++;
    return true;
}

void Frame_Assembler::releaseOldest()
{
    if (fill_ == HOPS_PER_FRAME) dropFront();
}

} // namespace sds110
