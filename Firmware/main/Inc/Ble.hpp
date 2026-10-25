#pragma once

#include "Models.hpp"

namespace ble {

bool Init();
void DeInit();

void SendKeyboardReport(models::KeyboardReport report);
void SendConsumerCode(uint16_t consumerCode);

} // namespace ble
