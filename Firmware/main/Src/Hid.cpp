#include "Hid.hpp"

#include <esp_log.h>

#include "RtosUtils.hpp"

#include "Leds.hpp"

#include "Ble.hpp"
#include "Usb.hpp"
#include "esp_hidd.h"

namespace hid {

static bool Init();
static void Handler();

static void PrintReport(std::array<uint8_t, models::REPORT_SIZE>& report);

static rtos::Task task("UsbHidTask", 4096, 24, Init, Handler, 0);
static rtos::Queue<KbHidReport> kbReportsQueue(10);

static constexpr uint8_t KEYBOARD_REPORT_ID = 1;
static constexpr uint8_t CONSUMER_REPORT_ID = 3;

enum class Mode {
    Usb,
    Ble
};

static Mode currentMode = Mode::Usb;

bool SendReport(KbHidReport kbHidReport) {
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

    ble::DeInit();
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

    ble::Init();
    leds::SendCommand(leds::Commands::BluetoothConnected);
    currentMode = Mode::Ble;
}

static bool Init() {
    usb::Init();

    return true;
}

static void Handler() {
    static uint16_t lastConsumerCode;
    static models::KeyboardReport keyCodes = {};

    KbHidReport report;
    kbReportsQueue.Wait(report);

    if (lastConsumerCode != report.consumerCode) {
        lastConsumerCode = report.consumerCode;
        if (currentMode == Mode::Ble) {
            ble::SendConsumerCode(lastConsumerCode);
        } else if (currentMode == Mode::Usb) {
            usb::SendConsumerCode(lastConsumerCode);
        }

        return;
    }

    keyCodes[0] = report.modifiers;
    memcpy(&keyCodes[2], report.keys.data(), models::REPORT_MAX_KEYS);

    if (currentMode == Mode::Ble) {
        ble::SendKeyboardReport(keyCodes);
    } else if (currentMode == Mode::Usb) {
        usb::SendKeyboardReport(keyCodes);
    }

    PrintReport(keyCodes);
    leds::ResetTimeout();
}

static void PrintReport(models::KeyboardReport& report) {
    uint16_t textIndex         = 0;
    std::array<char, 100> text = {""};

    for (uint16_t i = 0; i < models::REPORT_SIZE; ++i) {
        textIndex +=
            snprintf(&text[textIndex], sizeof(text) - textIndex, "%d ", report[i]);
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
