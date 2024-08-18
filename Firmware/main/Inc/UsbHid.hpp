#pragma once

#include "Layout.hpp"

#include "Models/HidModel.hpp"

namespace usb_hid {

bool SendReport(model::hid::Report);

bool SetupTask();

} // namespace usb_hid
