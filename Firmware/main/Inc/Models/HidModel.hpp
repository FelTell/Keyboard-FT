

#pragma once

#include "Layout.hpp"
#include <array>
#include <cstdint>

namespace model::hid {

struct Report {
    std::array<uint8_t, layout::COLUMNS_NUM * layout::ROWS_NUM> keys;
    uint16_t consumerCode;
    uint16_t size;
    uint8_t modifiers;
};

} // namespace model::hid
