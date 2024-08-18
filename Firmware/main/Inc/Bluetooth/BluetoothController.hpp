#pragma once

#include "Models/HidModel.hpp"

namespace bluetooth::controller {

bool SendReport(model::hid::Report);

bool SetupTask();

} // namespace bluetooth::controller
