#pragma once

#include <array>
#include <cstdint>

namespace models {

static constexpr uint8_t ROWS_NUM    = 6;
static constexpr uint8_t COLUMNS_NUM = 15;

static constexpr uint8_t REPORT_MAX_KEYS = 6;
static constexpr uint8_t REPORT_SIZE     = 2 + REPORT_MAX_KEYS;

using KeyboardReport = std::array<uint8_t, REPORT_SIZE>;

} // namespace models
