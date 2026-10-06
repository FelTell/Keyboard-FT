#pragma once

#include <array>
#include <cstdint>

namespace models {

static constexpr uint8_t ROWS_NUM    = 6;
static constexpr uint8_t COLUMNS_NUM = 15;

struct KbHidReport {
    std::array<uint8_t, COLUMNS_NUM * ROWS_NUM> keys;
    uint16_t consumerCode;
    uint16_t size;
    uint8_t modifiers;
};

} // namespace models
