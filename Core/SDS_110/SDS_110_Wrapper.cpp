/*
 * SDS_110_Wrapper.cpp – C-Einstiegspunkte für main.c (STM32F746ZGT6-Board).
 */
#include "SDS_110_Wrapper.hpp"
#include "Infrastructure/Tasks/USBTask.hpp"
#include "Infrastructure/Tasks/LoggerTask.hpp"
#include "Infrastructure/Tasks/ProcessingTask.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"

// CubeMX, main.c: ADAU7118 über SAI1 Block A (PE4 FS, PE5 SCK, PE6 SD) und I2C2 (PB10/PB11)
extern SAI_HandleTypeDef hsai_BlockA1;
extern I2C_HandleTypeDef hi2c2;

extern "C" {

void SDS110_Init(void)
{
    SDS_Data& dm = SDS_Data::instance();
    // Standard-Unit-ID: 96-Bit-UID des STM32 auf 16 Bit gefaltet (per USB-Kommando Typ 5 überschreibbar)
    const uint32_t uid = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();
    dm.setId(static_cast<uint16_t>((uid ^ (uid >> 16)) & 0xFFFF));

    sds110::Processing_Module_120::instance().init(&hsai_BlockA1, &hi2c2);
}

void SDS110_StartProcessingTask(void) { sds110::ProcessingTask::instance().start(); }
void SDS110_StartUSBTask(void)        { sds110::USBTask::instance().start(); }
void SDS110_StartLoggerTask(void)     { sds110::LoggerTask::instance().start(); }

} // extern "C"
