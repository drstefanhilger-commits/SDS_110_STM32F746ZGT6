/*
 * t_sds_data – Sperr-Timeouts in SDS_Data (Befund 29)
 *
 * Prüft mit Fehlerinjektion im Host-Shim (g_osMutexFailNext):
 *   1. Getter liefern bei Timeout den aktuellen Wert statt 0 (Mode, Unit-ID, Simulation, Azimut,
 *      Task-Statistik, akustischer Zustand) und zählen den Timeout
 *   2. tryGet liefert bei Timeout false und lässt den Ausgabewert unverändert
 *   3. Setter warten nach dem ersten Timeout ein zweites Mal: ein Timeout -> Wert übernommen;
 *      zwei Timeouts -> verworfen, gezählt, errorFlag 999
 * Aufruf: build/test_host/t_sds_data
 */
#include <cstdio>
#include "Infrastructure/Model/SDS_Data.hpp"
using namespace sds110;

static int g_fail = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) g_fail = 1; }

int main()
{
    SDS_Data& dm = SDS_Data::instance();
    dm.setMode(SDS_Mode::READ); dm.setId(4660); dm.setSimulation(1);
    dm.setCandidate(123.0f, 45.0f, 0.8f, true);
    dm.setTaskStats(TaskId::Usb, 1000, 1.5f, 77);
    AcousticState st{}; st.p[5] = 0.9f; dm.setAcousticState(st);

    // 1) Getter bei Timeout
    const uint32_t t0 = dm.lockTimeouts();
    g_osMutexFailNext = 1; const SDS_Mode m = dm.getMode();
    g_osMutexFailNext = 1; const uint16_t id = dm.getId();
    g_osMutexFailNext = 1; const uint32_t sim = dm.getSimulation();
    g_osMutexFailNext = 1; const float az = dm.getAzimuth();
    g_osMutexFailNext = 1; const TaskStats ts = dm.getTaskStats(TaskId::Usb);
    AcousticState out{}; g_osMutexFailNext = 1; dm.getAcousticState(out);
    check(m == SDS_Mode::READ && id == 4660 && sim == 1 && az == 123.0f, "Getter: Wert statt 0 bei Timeout");
    check(ts.loopCounter == 77 && out.p[5] == 0.9f, "Task-Statistik und akustischer Zustand bei Timeout");
    check(dm.lockTimeouts() - t0 == 6, "Timeouts gezählt (6)");

    // 2) tryGet
    uint32_t s2 = 42; g_osMutexFailNext = 1;
    const bool r = dm.tryGetSimulation(s2);
    check(!r && s2 == 42 && dm.lockTimeouts() - t0 == 7, "tryGet: false, Ausgabe unverändert");

    // 3) Setter
    g_osMutexFailNext = 1; dm.setId(1234);
    check(dm.getId() == 1234 && dm.getErrorFlag() != 999, "Setter: ein Timeout -> zweiter Versuch übernimmt den Wert");
    g_osMutexFailNext = 2; dm.setId(999);
    check(dm.getId() == 1234 && dm.getErrorFlag() == 999 && dm.lockTimeouts() - t0 == 8,
          "Setter: zwei Timeouts -> verworfen, gezählt, errorFlag 999");
    g_osMutexFailNext = 0;
    return g_fail;
}
