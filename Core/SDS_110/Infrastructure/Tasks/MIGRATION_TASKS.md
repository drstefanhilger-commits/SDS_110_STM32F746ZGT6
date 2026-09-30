# Migration Infrastructure/Tasks + Driver + Utils

| Neu | Aus SDS | Änderung |
|---|---|---|
| Tasks/TaskBase.{hpp,cpp} | Tasks/TaskBase | Namespace; `reportStats(TaskId)` schreibt Monitoring in SDS_Data |
| Tasks/USBTask.{hpp,cpp} | Tasks/USBTask + `usb_debug_counter` aus LCDTask.cpp | neue SDS_Data-API; RX-Queue wird leer gelesen statt 1 Msg/Loop |
| Driver/USBDriver.{hpp,cpp} | Driver/USBDriver + SDSUSBMicSender | Klasse `USBDriver` (statisch); sendRead liest MicFrame (float) |
| Utils/DWT.hpp, Utils/crc32.hpp | Utils/ | Namespace |

Stand 25.09.2026: Alles migriert (siehe „Stand 2“), `Core/SDS` ist entfernt. `syscalls.c` liegt in
`Core/Src` (CubeIDE-Version). main.c ruft die neue API aus `SDS_110_Wrapper.hpp`
(`SDS110_Init`, `SDS110_Start*Task`); die alten Namen `SDS_Init` / `SDS_Start*` gibt es nicht mehr.

Spätere Änderungen: USBDriver sendet über einen TX-Ringpuffer (Befund 11); `TaskBase::start()` liefert
`bool` und hält bei fehlgeschlagener Task-Anlage an, Heap 64 kB (Befund 13); Logger threadsicher (Befund 19);
`USBTask::msgLen()` statt `payloadLen()` (Befund 12); neu `Utils/TimeBase` (Befund 16) und
`Driver/SDRAMSelfTest` (Befund 14).

## Stand 2 (LCD/Logger)
| Neu | Aus SDS | Änderung |
|---|---|---|
| Tasks/LCDTask.{hpp,cpp} | Tasks/LCDTask | neue SDS_Data-API; READ-Modus zeigt 64-Band-Spektrum s(t) statt 4 AI-Klassen |
| Tasks/LoggerTask.{hpp,cpp} | Tasks/LoggerTask | USBDriver::sendMessage |
| Utils/Logger.{hpp,cpp} | Utils/Logger | Include-Pfad |
| Driver/LCDDriver.hpp, Font8x12.*, SDRAMDriver.h, PrintfDriver.h, MPUDriver.h | Driver/ | unverändert kopiert |

main.c (Stand 25.09.2026): `SDS110_Init()`, `SDS110_StartDisplayTask()`, `SDS110_StartUSBTask()`,
`SDS110_StartLoggerTask()` und seit 27.09.2026 auch `SDS110_StartProcessingTask()` aktiv (Blocker 1; Simulationsbetrieb, Hardware-Pfad siehe Blocker 2–4).

~~Hinweis 122: Kopie unter Sensor_Unit_112/ löschen~~ – *erledigt*, 122 liegt nur noch unter
Processing_Module_120/Feature_Extraction_Module_122/.
