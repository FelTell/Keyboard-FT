#include "Matrix.hpp"

#include <array>

#include <class/hid/hid_device.h>
#include <driver/gpio.h>
#include <esp_bit_defs.h>

#include <esp_log.h>

#include "RtosUtils.hpp"

#include "Layout.hpp"
#include "UsbHid.hpp"

namespace matrix {

static bool Init();
static void Handler();
static void HandleKey(Key& key);
static usb_hid::KbHidReport GetKeyReport();

static rtos::Task task("MatrixTask", 4096, 24, Init, Handler);

static const std::array<gpio_num_t, layout::ROWS_NUM> rows = {
    GPIO_NUM_14,
    GPIO_NUM_2,
    GPIO_NUM_7,
    GPIO_NUM_6,
    GPIO_NUM_5,
    GPIO_NUM_4,
};
static const std::array<gpio_num_t, layout::COLUMNS_NUM> columns = {
    GPIO_NUM_37,
    GPIO_NUM_36,
    GPIO_NUM_35,
    GPIO_NUM_21,
    GPIO_NUM_1,
    GPIO_NUM_15,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_18,
    GPIO_NUM_8,
    GPIO_NUM_9,
    GPIO_NUM_10,
    GPIO_NUM_11,
    GPIO_NUM_12,
    GPIO_NUM_13};

static bool Init() {
    gpio_config_t config;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type  = GPIO_INTR_DISABLE;

    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.mode         = GPIO_MODE_INPUT;
    for (gpio_num_t gpioNum : rows) {
        config.pin_bit_mask = BIT64(gpioNum);
        gpio_config(&config);
    }

    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.mode         = GPIO_MODE_OUTPUT;
    for (gpio_num_t gpioNum : columns) {
        config.pin_bit_mask = BIT64(gpioNum);
        gpio_config(&config);
    }
    return true;
}

static void Handler() {
    for (uint8_t column = 0; column < layout::COLUMNS_NUM; ++column) {
        gpio_set_level(columns[column], true);
        rtos::Delay(1);
        for (uint8_t row = 0; row < layout::ROWS_NUM; ++row) {
            Key& key   = layout::keys[column][row];
            bool state = gpio_get_level(rows[row]);
            if (state != key.GetState()) {
                key.SetState(state);
                HandleKey(key);
                ESP_LOGI(key.GetText(),
                         "has been %s. ID = %d. Row = %d, Column = %d. GPIO = "
                         "%d and %d.",
                         key.GetState() ? "pressed" : "released",
                         key.GetCode(),
                         row,
                         column,
                         rows[row],
                         columns[column]);
            }
        }
        gpio_set_level(columns[column], false);
    }
}

static void HandleKey(Key& key) {
    const auto report = GetKeyReport();

    usb_hid::SendReport(report);
}

static usb_hid::KbHidReport GetKeyReport() {
    usb_hid::KbHidReport report = {};

    for (uint8_t column = 0; column < layout::COLUMNS_NUM; ++column) {
        for (uint8_t row = 0; row < layout::ROWS_NUM; ++row) {
            const auto key = layout::keys[column][row];

            if (key.GetState()) {
                if (key.GetCode()) {
                    report.keys[report.size++] = key.GetCode();
                } else {
                    report.modifiers = report.modifiers | key.GetModifier();
                }
            }
        }
    }

    return report;
}

bool SetupTask() {
    if (!task.Setup()) {
        return false;
    }
    return true;
}

} // namespace matrix
