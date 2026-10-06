#pragma once

#include "Models.hpp"

namespace ble {

bool Init();
bool DeInit();

void SendReport(models::KbHidReport report);

} // namespace ble
