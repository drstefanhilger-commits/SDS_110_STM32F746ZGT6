// Stubs für Output_Interface_130 im Host-Test: fängt die gesendete Nachricht ab
#include "Infrastructure/Driver/USBDriver.hpp"
#include "Infrastructure/Model/SDS_Data.hpp"
namespace sds110 {
MessageData g_lastMsg{}; uint32_t g_lastId = 0;
bool USBDriver::sendDetection(uint32_t, uint32_t, float, float, float, uint32_t) { return true; }
bool USBDriver::sendMessage(uint32_t id, uint32_t, const MessageData& d, uint32_t) { g_lastId = id; g_lastMsg = d; return true; }
}
SDS_Data& SDS_Data::instance() { static SDS_Data d; return d; }
SDS_Data::SDS_Data() {}
