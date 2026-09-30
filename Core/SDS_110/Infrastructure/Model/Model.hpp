/*
 * Model.hpp  (Infrastructure)
 *
 * Sammel-Header für die Statusdaten der Infrastruktur-Tasks (LCD, Logger, USB).
 * Für die Patent-Verarbeitungskette ist Processing_Module_120.hpp der Sammel-Header.
 *
 * Migration aus SDS/Model/Model.hpp – nicht mehr enthalten:
 *   SDS_MicrophoneBuffer.hpp -> Sensor_Unit_112/Microphone_Array_114.hpp
 *   SDS_SRPBuffers.hpp       -> Correlation_Processing_Module_126
 */
#pragma once

#include "SDS_110_Config.hpp"
#include "SDS_Params.hpp"
#include "SDS_Structs.hpp"
#include "SDS_Data.hpp"
#include "Data_Interface_140/Candidate_Report_140.hpp"
#include "Infrastructure/Utils/Logger.hpp"
