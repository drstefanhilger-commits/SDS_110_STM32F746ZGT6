/*
 * ML124_Config.hpp – Umschalter HBD / ML für Modul 124
 *
 * Eigene Datei statt SDS_110_Config.hpp: SDS_110_Config.hpp geht in die Merkmalsversion ein
 * (tools/features/Makefile), ein Umschalten würde sonst ein Neutraining erzwingen.
 *
 * Stufen zum schrittweisen Einschalten (ML_Test docs/Trainingskonzept_ML124.md, Abschnitt 8):
 *   Hbd    s(t) aus dem HBD wie bisher, MLP wird nicht gerechnet
 *   Shadow s(t) aus dem HBD; MLP läuft mit, Abweichung HBD ↔ ML wird gezählt (LCD, shadow())
 *   Ml     s(t) aus dem MLP; HBD läuft weiter für Gate (Haltezeit) und Diagnose
 * In allen Stufen folgt die Glättung über STATE_SMOOTH_FRAMES, und das HBD-Gate bleibt
 * wirksam (Konzept Abschnitt 6: erst nach der Bewertung in AP7 ggf. entfernen).
 */
#pragma once
#include <cstdint>

namespace sds110 {

enum class Ml124Mode : uint8_t { Hbd = 0, Shadow = 1, Ml = 2 };

constexpr Ml124Mode ML124_MODE = Ml124Mode::Shadow;   // Standard nach dem Start

} // namespace sds110
