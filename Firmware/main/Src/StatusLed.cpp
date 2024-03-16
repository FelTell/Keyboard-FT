#include "StatusLed.hpp"
#include <array>

#include "RtosUtils.hpp"
#include "led_strip.h"
#include <esp_log.h>

#include <driver/gpio.h>

namespace leds {
static const char* taskName = "LedsTask";

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
static void ShowRainbow();
static void ShowError();
static void SetCapsKey(bool);
static void DecreaseIncreaseBrightness(bool isIncrease);

static led_strip_handle_t rgbHandle;

static rtos::Task task(taskName, 4096, 24, Init, Handler);
static rtos::Queue<Commands> requests(1);

static Commands currentMode;

static uint8_t dimmLevel;
static constexpr int8_t DEFAULT_INDEX = 2;
static uint8_t brightness             = (1 << (DEFAULT_INDEX + 1)) - 1;

bool SendCommand(Commands mode) {
    return requests.Send(mode);
}

void IncreaseBrightness(bool isPressed) {
    static bool commandDone;

    if (!isPressed) {
        commandDone = false;
        return;
    }
    if (commandDone) {
        return;
    }

    SendCommand(Commands::IncreaseBrightness);
    commandDone = true;
}

void DecreaseBrightness(bool isPressed) {
    static bool commandDone;

    if (!isPressed) {
        commandDone = false;
        return;
    }
    if (commandDone) {
        return;
    }

    SendCommand(Commands::DecreaseBrightness);
    commandDone = true;
}

// Since this is a simple variable that only changes in one place there is no
// need for synchronization
Commands GetMode() {
    return currentMode;
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

    led_strip_new_rmt_device(&stripConfig, &rmtConfig, &rgbHandle);
    led_strip_clear(rgbHandle);

    const gpio_config_t config = {
        .pin_bit_mask = BIT64(CAPS_LED_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&config);

    SetCapsKey(false);

    currentMode = Commands::NotConnected;

    return true;
}

static void Handler() {
    switch (currentMode) {
        case Commands::CapsOnUsb:
            ShowCapsOnUsb();
            break;
        case Commands::CapsOnBle:
            ShowCapsOnBle();
            break;
        case Commands::Usb:
            ShowUsb();
            break;
        case Commands::BluetoothSearching:
            ShowBluetoothSearching();
            break;
        case Commands::BluetoothConnected:
            ShowBluetoothConnected();
            break;
        case Commands::NotConnected:
            ShowRainbow();
            break;
        case Commands::Error:
            ShowError();
            break;
        default:
            DelayAndWaitNewCommand(0xFFFFFFFF);
            break;
    }
}

static void DelayAndWaitNewCommand(const TickType_t ticksToDelay) {
    auto newCommand = requests.Wait(ticksToDelay);
    if (newCommand) {
        if (*newCommand == Commands::DecreaseBrightness ||
            *newCommand == Commands::IncreaseBrightness) {
            DecreaseIncreaseBrightness(*newCommand ==
                                       Commands::IncreaseBrightness);
        } else {
            currentMode = newCommand.value();
        }
    }
}

static void ShowCapsOnUsb() {
    led_strip_set_pixel(rgbHandle, 0, brightness, brightness, 0);
    led_strip_refresh(rgbHandle);

    SetCapsKey(true);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowCapsOnBle() {
    led_strip_set_pixel(rgbHandle, 0, brightness, 0, brightness);
    led_strip_refresh(rgbHandle);

    SetCapsKey(true);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowUsb() {
    led_strip_set_pixel(rgbHandle, 0, 0, brightness, 0);
    led_strip_refresh(rgbHandle);

    SetCapsKey(false);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowBluetoothSearching() {
    static bool state;
    if (state) {
        led_strip_set_pixel(rgbHandle, 0, 0, 0, brightness);
        led_strip_refresh(rgbHandle);
    } else {
        led_strip_set_pixel(rgbHandle, 0, brightness, 0, 0);
        led_strip_refresh(rgbHandle);
    }
    state = !state;

    SetCapsKey(false);

    DelayAndWaitNewCommand(250);
}

static void ShowBluetoothConnected() {
    led_strip_set_pixel(rgbHandle, 0, 0, 0, brightness);

    SetCapsKey(false);

    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void ShowRainbow() {
    static enum class State {
        Start,
        Red,
        Green,
        Blue
    } state = State::Start;

    if (state == State::Start) {
        led_strip_set_pixel(rgbHandle, 0, dimmLevel, 0, 0);
        led_strip_refresh(rgbHandle);
        dimmLevel++;
        if (dimmLevel >= brightness) {
            state     = State::Red;
            dimmLevel = 0;
        }
    } else if (state == State::Red) {
        led_strip_set_pixel(rgbHandle, 0, brightness - dimmLevel, dimmLevel, 0);
        led_strip_refresh(rgbHandle);
        dimmLevel++;
        if (dimmLevel >= brightness) {
            state     = State::Green;
            dimmLevel = 0;
        }
    } else if (state == State::Green) {
        led_strip_set_pixel(rgbHandle, 0, 0, brightness - dimmLevel, dimmLevel);
        led_strip_refresh(rgbHandle);
        dimmLevel++;
        if (dimmLevel >= brightness) {
            state     = State::Blue;
            dimmLevel = 0;
        }
    } else if (state == State::Blue) {
        led_strip_set_pixel(rgbHandle, 0, dimmLevel, 0, brightness - dimmLevel);
        led_strip_refresh(rgbHandle);
        dimmLevel++;
        if (dimmLevel >= brightness) {
            state     = State::Red;
            dimmLevel = 0;
        }
    }
    DelayAndWaitNewCommand(160 / brightness);
}

static void ShowError() {
    led_strip_set_pixel(rgbHandle, 0, 0, brightness, 0);
    DelayAndWaitNewCommand(0xFFFFFFFF);
}

static void SetCapsKey(bool state) {
    gpio_set_level(CAPS_LED_PIN, !state);
}

static void DecreaseIncreaseBrightness(bool isIncrease) {
    static constexpr int8_t MAX_INDEX     = 7;
    static constexpr int8_t DEFAULT_INDEX = 2;
    static int8_t currentIndex            = DEFAULT_INDEX;

    if (isIncrease && currentIndex < MAX_INDEX) {
        currentIndex++;
    } else if (!isIncrease && currentIndex > 0) {
        currentIndex--;
    }

    brightness = (1 << (currentIndex + 1)) - 1;
    dimmLevel  = 0;

    ESP_LOGI("Led Brightness", "Set to %d", brightness);
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
