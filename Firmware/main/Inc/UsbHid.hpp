#pragma once

#include "Layout.hpp"

namespace usb_hid {

struct KbHidReport {
    std::array<uint8_t, layout::COLUMNS_NUM * layout::ROWS_NUM> keys;
    uint16_t size;
    uint8_t modifiers;
};

bool SendReport(KbHidReport);

bool SetupTask();

} // namespace usb_hid
