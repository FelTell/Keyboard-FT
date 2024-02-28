#include "StatusLed.hpp"

#include "RtosUtils.hpp"
#include "led_strip.h"
#include <esp_log.h>

#include <driver/gpio.h>

namespace leds {
static const char* taskName = "LedsTask";

static constexpr auto LED_MAX        = 16;
static constexpr auto STATUS_LED_PIN = GPIO_NUM_48;
static constexpr auto CAPS_LED_PIN   = GPIO_NUM_39;

static bool Init();
static void Handler();

static void DelayAndWaitNewCommand(const TickType_t ticksToDelay);

static void ShowCapsOnUsb();
static void ShowCapsOnBle();
static void ShowUsb();
static void ShowBluetoothSearching();
static void ShowBluetoothConnected();
static void ShowStartupDance();
static void ShowError();
static void SetCapsKey(bool);

static led_strip_handle_t ledHandle;

static rtos::Task task(taskName, 4096, 24, Init, Handler);
static rtos::Queue<Modes> requests(1);

static Modes currentMode;

bool SetMode(Modes mode) {
    return requests.Send(mode);
}

static bool Init() {
    const led_strip_config_t stripConfig = {
        .strip_gpio_num   = STATUS_LED_PIN,
        .max_leds         = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model        = LED_MODEL_WS2812,
        .flags            = {.invert_out = false},
    };
    // Using default settings
    const led_strip_rmt_config_t rmtConfig = {};

    led_strip_new_rmt_device(&stripConfig, &rmtConfig, &ledHandle);
    led_strip_clear(ledHandle);

    const gpio_config_t config = {
        .pin_bit_mask = BIT64(CAPS_LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);

    SetCapsKey(false);

    currentMode = Modes::Rainbow;

    return true;
}

static void Handler() {
    switch (currentMode) {
        case Modes::CapsOnUsb:
            ShowCapsOnUsb();
            break;
        case Modes::CapsOnBle:
            ShowCapsOnBle();
            break;
        case Modes::Usb:
            ShowUsb();
            break;
        case Modes::BluetoothSearching:
            ShowBluetoothSearching();
            break;
        case Modes::BluetoothConnected:
            ShowBluetoothConnected();
            break;
        case Modes::Rainbow:
            ShowStartupDance();
            break;
        case Modes::Error:
            ShowError();
            break;
    }
}

static void DelayAndWaitNewCommand(const TickType_t ticksToDelay) {
    auto newCommand = requests.Wait(ticksToDelay);
    if (newCommand) {
        currentMode = newCommand.value();
    }
}

static void ShowCapsOnUsb() {
    led_strip_set_pixel(ledHandle, 0, LED_MAX, LED_MAX, 0);
    led_strip_refresh(ledHandle);

    SetCapsKey(true);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowCapsOnBle() {
    led_strip_set_pixel(ledHandle, 0, LED_MAX, 0, LED_MAX);
    led_strip_refresh(ledHandle);

    SetCapsKey(true);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowUsb() {
    led_strip_set_pixel(ledHandle, 0, 0, LED_MAX, 0);
    led_strip_refresh(ledHandle);

    SetCapsKey(false);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowBluetoothSearching() {
    static bool state;
    if (state) {
        led_strip_set_pixel(ledHandle, 0, 0, 0, LED_MAX);
        led_strip_refresh(ledHandle);
    } else {
        led_strip_set_pixel(ledHandle, 0, LED_MAX, 0, 0);
        led_strip_refresh(ledHandle);
    }
    state = !state;

    SetCapsKey(false);

    DelayAndWaitNewCommand(250);
}

static void ShowBluetoothConnected() {
    led_strip_set_pixel(ledHandle, 0, 0, 0, LED_MAX);

    SetCapsKey(false);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowStartupDance() {
    static enum class State {
        Start,
        Red,
        Green,
        Blue
    } state = State::Start;
    static uint8_t level;

    if (state == State::Start) {
        led_strip_set_pixel(ledHandle, 0, level, 0, 0);
        led_strip_refresh(ledHandle);
        level++;
        if (level >= LED_MAX) {
            state = State::Red;
            level = 0;
        }
    } else if (state == State::Red) {
        led_strip_set_pixel(ledHandle, 0, LED_MAX - level, level, 0);
        led_strip_refresh(ledHandle);
        level++;
        if (level >= LED_MAX) {
            state = State::Green;
            level = 0;
        }
    } else if (state == State::Green) {
        led_strip_set_pixel(ledHandle, 0, 0, LED_MAX - level, level);
        led_strip_refresh(ledHandle);
        level++;
        if (level >= LED_MAX) {
            state = State::Blue;
            level = 0;
        }
    } else if (state == State::Blue) {
        led_strip_set_pixel(ledHandle, 0, level, 0, LED_MAX - level);
        led_strip_refresh(ledHandle);
        level++;
        if (level >= LED_MAX) {
            state = State::Red;
            level = 0;
        }
    }
    DelayAndWaitNewCommand(10);
}

static void ShowError() {
    led_strip_set_pixel(ledHandle, 0, 0, LED_MAX, 0);
    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void SetCapsKey(bool state) {
    gpio_set_level(CAPS_LED_PIN, !state);
}

bool SetupTask() {
    if (!task.Setup()) {
        return false;
    }
    if (!requests.Setup()) {
        return false;
    }
    return true;
}

} // namespace leds
