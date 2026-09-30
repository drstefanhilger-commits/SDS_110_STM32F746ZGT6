// Host-Messungen: Stufe von 124 über die Umgebung wählen (AP7, Vergleich HBD <-> ML)
//   ML124_MODE=0 HBD, 1 Schatten (s(t) wie HBD), 2 ML; ohne Angabe: ML124_MODE aus ML124_Config.hpp
#pragma once
#include <cstdio>
#include <cstdlib>
#include "Processing_Module_120/Machine_Learning_Module_124/Machine_Learning_Module_124.hpp"
namespace sds110 {
inline void mlModeFromEnv(Machine_Learning_Module_124& ml)
{
    if (const char* e = std::getenv("ML124_MODE")) ml.setMode(static_cast<Ml124Mode>(std::atoi(e)));
    static const char* const names[] = { "HBD", "Schatten", "ML" };
    std::printf("124 Stufe: %s\n", names[static_cast<int>(ml.mode()) % 3]);
}
}
