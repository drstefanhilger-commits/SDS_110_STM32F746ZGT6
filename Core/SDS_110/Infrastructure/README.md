Nicht patentrelevante Infrastruktur (aus SDS übernommen und angepasst, Stand 25.09.2026):
- Tasks/  : TaskBase, ProcessingTask (Aufrufer von 120), LCDTask, USBTask, LoggerTask
- Timer/  : HardwareTimer, TaskTimerBase (TIM-getaktete Tasks: LCD, Logger)
- Driver/ : USBDriver (TX-Ringpuffer), SDRAMSelfTest, LCDDriver, Font8x12, SDRAMDriver, MPUDriver, PrintfDriver
- Utils/  : Logger, DWT, TimeBase (µs-Zeitbasis), crc32
- Model/  : SDS_Params, SDS_Structs, SDS_Data (Status/Debug für LCD & Logger)
syscalls.c liegt in Core/Src (CubeIDE-Version).
