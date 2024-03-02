#include "UsbHid.hpp"

#include <algorithm>
#include <cstdlib>

#include <class/hid/hid_device.h>
#include <esp_log.h>
#include <tinyusb.h>

#include "RtosUtils.hpp"

#include "StatusLed.hpp"

namespace usb_hid {

static constexpr uint8_t REPORT_MAX_KEYS = 6;

static bool Init();
static void Handler();

static void HandleConnectedStatus();
static void PrintReport(usb_hid::KbHidReport report);

static rtos::Task task("UsbHidTask", 4096, 24, Init, Handler);
static rtos::Queue<KbHidReport> kbReportsQueue(10);

static bool isReady;

// TinyUSB descriptors

static constexpr uint8_t KEYBOARD_REPORT_ID = 1;
static constexpr uint8_t CONSUMER_REPORT_ID = 3;

static constexpr uint32_t TUSB_DESC_TOTAL_LEN =
    TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN;

static const uint8_t reportDescriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_REPORT_ID)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(CONSUMER_REPORT_ID))};

static const char* stringDescriptor[5] = {
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "FelTell",            // 1: Manufacturer
    "Keyboard-FT",        // 2: Product
    "0000001",            // 3: Serial,
    "Keyboard-FT V1.0",   // 4: HID
};

static const uint8_t configurationDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1,
                          1,
                          0,
                          TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
                          100),

    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(reportDescriptor), 0x81, 16, 10),
};

bool SendReport(KbHidReport kbHidReport) {
    return kbReportsQueue.Send(kbHidReport);
}

static bool Init() {
    const tinyusb_config_t tinyUsbConfig = {
        .device_descriptor = NULL,
        .string_descriptor = stringDescriptor,
        .string_descriptor_count =
            sizeof(stringDescriptor) / sizeof(stringDescriptor[0]),
        .external_phy             = false,
        .configuration_descriptor = configurationDescriptor,
        .self_powered             = false,
        .vbus_monitor_io          = 0,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tinyUsbConfig));

    return true;
}

static void Handler() {
    static uint16_t consumerCode;
    static std::array<uint8_t, REPORT_MAX_KEYS> keyCodes = {};
    static uint8_t modifiers;
    bool consumerUpdated = false;

    auto report = kbReportsQueue.Wait(10);

    HandleConnectedStatus();

    if (report) {
        if (consumerCode != report->consumerCode) {
            consumerUpdated = true;
        }

        consumerCode = report->consumerCode;
        modifiers    = report->modifiers;

        for (uint16_t i = 0; i < REPORT_MAX_KEYS; ++i) {
            keyCodes[i] = report->keys[i];
        }

        PrintReport(*report);
    }

    if (consumerUpdated) {
        tud_hid_report(CONSUMER_REPORT_ID, &consumerCode, 2);
    } else {
        tud_hid_keyboard_report(KEYBOARD_REPORT_ID, modifiers, keyCodes.data());
    }
}

static void HandleConnectedStatus() {
    const bool tinyUsbReady = tud_ready();
    if (isReady != tinyUsbReady) {
        isReady = tinyUsbReady;
        if (!isReady) {
            isReady = false;
            leds::SetMode(leds::Modes::NotConnected);
        } else if (leds::GetMode() == leds::Modes::NotConnected) {
            leds::SetMode(leds::Modes::Usb);
        }
    }
}

static void PrintReport(usb_hid::KbHidReport report) {
    static bool isConsumerPressed;

    uint16_t textIndex         = 0;
    std::array<char, 100> text = {""};

    if (report.consumerCode) {
        ESP_LOGI("ConsumerReport: ", "%d pressed", report.consumerCode);
        isConsumerPressed = true;
    } else if (isConsumerPressed) {
        ESP_LOGI("ConsumerReport: ", "released");
        isConsumerPressed = false;
    }

    for (uint16_t i = 0; i < REPORT_MAX_KEYS; ++i) {
        textIndex += snprintf(&text[textIndex],
                              sizeof(text) - textIndex,
                              "%d ,",
                              report.keys[i]);
    }
    ESP_LOGI("Report: ", "%s modifiers: %d", text.data(), report.modifiers);
}

bool SetupTask() {
    if (!task.Setup()) {
        return false;
    }
    if (!kbReportsQueue.Setup()) {
        return false;
    }
    return true;
}

} // namespace usb_hid

// TinyUSB HID callbacks

extern "C" const uint8_t* tud_hid_descriptor_report_cb(
    [[maybe_unused]] uint8_t instance) {
    return usb_hid::reportDescriptor;
}

extern "C" uint16_t tud_hid_get_report_cb(
    [[maybe_unused]] uint8_t instance,
    [[maybe_unused]] uint8_t id,
    [[maybe_unused]] hid_report_type_t type,
    [[maybe_unused]] uint8_t* buf,
    [[maybe_unused]] uint16_t realen) {
    ESP_LOGI("get report cb",
             "id: %d, type: %d, realen: %d, buf: %s",
             id,
             type,
             realen,
             buf);
    return 0;
}

extern "C" void tud_hid_set_report_cb([[maybe_unused]] uint8_t instance,
                                      uint8_t id,
                                      hid_report_type_t type,
                                      const uint8_t* buf,
                                      uint16_t size) {
    if (id != 1 && type != HID_REPORT_TYPE_OUTPUT && size != 1) {
        // Unknown message, log and ignore it
        uint16_t index = 0;
        std::array<char, 100> text;
        for (uint16_t i = 0; i < size; ++i) {
            index +=
                snprintf(&text[index], sizeof(text) - index, "%x ,", buf[i]);
        }
        ESP_LOGI("set report cb",
                 "id: %d, type: %d, size: %d, buf: %s",
                 id,
                 type,
                 size,
                 buf);
        return;
    }
    bool capsState = buf[0] & KEYBOARD_LED_CAPSLOCK;
    leds::SetMode(capsState ? leds::Modes::CapsOnUsb : leds::Modes::Usb);
}
