#pragma once

#include "Models.hpp"

namespace usb {

bool Init();

void SendKeyboardReport(models::KeyboardReport report);
void SendConsumerCode(uint16_t consumerCode);

} // namespace usb
