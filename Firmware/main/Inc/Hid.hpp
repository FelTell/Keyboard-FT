#pragma once

#include <array>
#include <cstdint>

#include "Models.hpp"

namespace hid {

bool SendReport(models::KbHidReport);

void SetUsbMode(bool isPressed);

void SetBleMode(bool isPressed);

bool SetupTask();

} // namespace hid
