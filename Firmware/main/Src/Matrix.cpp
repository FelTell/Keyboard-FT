#include "Matrix.hpp"

#include "RtosUtils.hpp"
#include <array>
#include <esp_log.h>

#include <driver/gpio.h>
#include <esp_bit_defs.h>

namespace matrix {

static bool Init();
static void Handler();

static rtos::Task task("MatrixTask", 4096, 24, Init, Handler);

static const std::array<gpio_num_t, 6> rows = {
    GPIO_NUM_14,
    GPIO_NUM_2,
    GPIO_NUM_7,
    GPIO_NUM_6,
    GPIO_NUM_5,
    GPIO_NUM_4,
};
static const std::array<gpio_num_t, 15> columns = {GPIO_NUM_37,
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
    for (gpio_num_t column : columns) {
        gpio_set_level(column, true);
        for (gpio_num_t row : rows) {
            if (gpio_get_level(row)) {
                ESP_LOGI("Key is pressed", "column: %d, row: %d", column, row);
            }
        }
        gpio_set_level(column, false);
    }
    rtos::Delay(5);
}

bool SetupTask() {
    if (!task.Setup()) {
        return false;
    }
    // if (!requests.Setup()) {
    //     return false;
    // }
    return true;
}

} // namespace matrix
