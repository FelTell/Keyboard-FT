#pragma once

#include <array>
#include <cstdint>

#include "Models.hpp"

namespace hid {

struct KbHidReport {
    std::array<uint8_t, models::COLUMNS_NUM * models::ROWS_NUM> keys;
    uint16_t consumerCode;
    uint16_t size;
    uint8_t modifiers;
};

bool SendReport(KbHidReport);

void SetUsbMode(bool isPressed);

void SetBleMode(bool isPressed);

bool SetupTask();

} // namespace hid
