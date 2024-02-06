#include "StatusLed.hpp"

#include "RtosUtils.hpp"
#include "led_strip.h"
#include <esp_log.h>

#include <driver/gpio.h>

namespace status_led {
const char* taskName = "StatusLedTask";

static constexpr auto LED_MAX        = 16;
static constexpr auto STATUS_LED_PIN = GPIO_NUM_48;

bool Init();
void Handler();

static led_strip_handle_t ledHandle;

bool Init() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = STATUS_LED_PIN,
        .max_leds       = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10'000'000,
    };
    led_strip_new_rmt_device(&strip_config, &rmt_config, &ledHandle);
    led_strip_clear(ledHandle);

    return true;
}

void Handler() {
    static bool state;
    if (state) {
        led_strip_set_pixel(ledHandle, 0, LED_MAX, LED_MAX, LED_MAX);
        led_strip_refresh(ledHandle);
    } else {
        led_strip_clear(ledHandle);
    }
    state = !state;
    rtos::Delay(1000);
}

bool SetupTask() {
    if (xTaskCreate(
            [](void* arg) {
                while (Init() == false) {
                    ESP_LOGE(taskName, "Init failed");
                    rtos::Delay(100);
                }
                ESP_LOGI(taskName, "Init Successful");
                while (1) {
                    Handler();
                }
            },
            taskName,
            4096,
            NULL,
            configMAX_PRIORITIES - 1,
            NULL) != pdPASS) {
        return false;
    }
    return true;
}

} // namespace status_led
