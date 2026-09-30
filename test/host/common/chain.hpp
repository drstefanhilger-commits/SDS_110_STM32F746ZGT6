// Host-Tests: Simulator -> 114 (Hop) -> 118 -> Frame_Assembler, wie Sensor_Unit_112::nextFrame()
#pragma once
#include "Harness/Signal_Simulator.hpp"
#include "Sensor_Unit_112/Pre_Processor_118.hpp"
#include "Sensor_Unit_112/Frame_Assembler.hpp"
namespace sds110 {
static Frame_Assembler g_fa;
inline const AnalysisFrame& nextAnalysisFrame(Signal_Simulator& sim, Microphone_Array_114& arr, Pre_Processor_118& pre)
{
    g_fa.releaseOldest();                          // wie 120 nach den Spektren des letzten Frames
    for (;;) {
        sim.generateHop(0);
        MicFrame* h = arr.acquireReadable();
        pre.process(*h);                           // wie am Board: 118 in place (Blockgleitkomma)
        if (g_fa.push(h)) return g_fa.frame();     // Besitz an den Frame_Assembler
    }
}
}
