#pragma once

#include "Models.hpp"

namespace usb {

bool Init();

void SendReport(models::KbHidReport report);

} // namespace usb
