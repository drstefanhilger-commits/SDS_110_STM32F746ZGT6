# Migration Infrastructure/Model

| Neu | Aus SDS | Änderung |
|---|---|---|
| SDS_Params.hpp | Model/SDS_Params.hpp | nur noch Simulation, UI, Version; Rest -> SDS_110_Config.hpp |
| SDS_Structs.hpp | Model/SDS_Structs.hpp | nur noch Events, Modus, USB-Wire-Format, ErrorMessage |
| SDS_Data.{hpp,cpp} | Model/SDS_Data.{hpp,cpp} | ohne Mel/AI; AcousticState-Spiegel; TaskStats generisch |
| Model.hpp | Model/Model.hpp | Sammel-Header nur für Infrastruktur |
| entfällt | Model/SDS_Structs.cpp (g_systemState) | ersetzt durch SDS_Data |

## API-Änderungen für LCDTask / USBTask / LoggerTask
| Alt | Neu |
|---|---|
| `setAzimuth/setDistance/setConfidence/setDetected` | `setCandidate(az, dist, confidence, valid)` (nur 120 schreibt; Konfidenz aus `candidateConfidence()` in 128, seit Befund 18) |
| `getAiDrone/Human/Wind/Background`, `getDroneDetected` | `getAcousticState(out)` (64 Bänder), `getSelectedBands()`, `getDetected()` |
| `setAiInitError/RunError` | `setMlInitError/RunError` |
| `setSrpTaskFreeStack/SrpLoopTime/SrpLoopCounter` | `setTaskStats(TaskId::Proc120, stack, time, counter)` |
| `setLcd*/setMic*/setUsb*` | `setTaskStats(TaskId::Lcd / Mic / Usb, …)` |
| `setDebugValue/1/2/3` | `setDebugValue(0..3, v)` |
| `getErrorBuffer()` (Pointer, nicht thread-sicher) | `getErrorBuffer(dst, maxLen)` (Kopie) |
| `setMode(uint32_t)` | `setMode(SDS_Mode)` |
| `SDS_ModeEventType` | `SDS_Mode` |
| `SDS_DataEventType::SRP_UPDATE / MICBLOCK_UPDATE` | `REPORT_UPDATE / MICFRAME_UPDATE` |

## Include-Pfade
Model.hpp erwartet `Core/SDS_110` als Include-Root (wie die Modul-Header):
`Data_Interface_140/...`, `Infrastructure/Utils/Logger.hpp`.
