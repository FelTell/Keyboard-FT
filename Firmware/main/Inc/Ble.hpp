#pragma once

#include "Models.hpp"

namespace ble {

bool Init();

void SendReport(models::KbHidReport report);

} // namespace ble
