#include "Hid.hpp"

#include <esp_log.h>

#include "RtosUtils.hpp"

#include "Leds.hpp"

#include "Ble.hpp"
#include "Usb.hpp"
#include "esp_hidd.h"

namespace hid {

static constexpr uint8_t REPORT_MAX_KEYS = 6;
static constexpr uint8_t REPORT_SIZE     = 2 + REPORT_MAX_KEYS;

static bool Init();
static void Handler();

static void PrintReport(std::array<uint8_t, REPORT_SIZE>& report);

static rtos::Task task("UsbHidTask", 4096, 24, Init, Handler, 0);
static rtos::Queue<models::KbHidReport> kbReportsQueue(10);

static constexpr uint8_t KEYBOARD_REPORT_ID = 1;
static constexpr uint8_t CONSUMER_REPORT_ID = 3;

enum class Mode {
    Usb,
    Ble
};

static Mode currentMode = Mode::Usb;

bool SendReport(models::KbHidReport kbHidReport) {
    return kbReportsQueue.Send(kbHidReport);
}

void SetUsbMode(bool isPressed) {
    static bool commandDone;

    if (!isPressed) {
        commandDone = false;
        return;
    }
    if (commandDone) {
        return;
    }
    commandDone = true;

    leds::SendCommand(leds::Commands::Usb);
    currentMode = Mode::Usb;
}

void SetBleMode(bool isPressed) {
    static bool commandDone;

    if (!isPressed) {
        commandDone = false;
        return;
    }
    if (commandDone) {
        return;
    }
    commandDone = true;

    leds::SendCommand(leds::Commands::BluetoothSearching);
    currentMode = Mode::Ble;
}

static bool Init() {
    usb::Init();
    ble::Init();

    return true;
}

static void Handler() {
    static uint16_t lastConsumerCode;
    static std::array<uint8_t, REPORT_SIZE> keyCodes = {};

    models::KbHidReport report;
    kbReportsQueue.Wait(report);

    if (currentMode == Mode::Ble) {
        ble::SendReport(report);
    } else if (currentMode == Mode::Usb) {
        usb::SendReport(report);
    }

    leds::ResetTimeout();
}

static void PrintReport(std::array<uint8_t, REPORT_SIZE>& report) {
    uint16_t textIndex         = 0;
    std::array<char, 100> text = {""};

    for (uint16_t i = 0; i < REPORT_SIZE; ++i) {
        textIndex += snprintf(&text[textIndex],
                              sizeof(text) - textIndex,
                              "%d ",
                              report[i]);
    }
    ESP_LOGI("Report: ", "%s", text.data());
}

bool SetupTask() {
    if (!kbReportsQueue.Setup()) {
        return false;
    }
    if (!task.Setup()) {
        return false;
    }
    return true;
}

} // namespace hid
