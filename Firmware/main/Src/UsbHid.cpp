#include "UsbHid.hpp"

#include <algorithm>
#include <cstdlib>

#include <class/hid/hid_device.h>
#include <esp_log.h>
#include <tinyusb.h>

#include "RtosUtils.hpp"

#include "StatusLed.hpp"

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD))};

namespace usb_hid {

static bool Init();
static void Handler();

static rtos::Task task("UsbHidTask", 4096, 24, Init, Handler);
static rtos::Queue<KbHidReport> kbReportsQueue(10);

/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/**
 * @brief String descriptor
 */
const char* hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04}, // 0: is supported language is
                          // English (0x0409)
    "FelTell",            // 1: Manufacturer
    "Keyboard-FT",        // 2: Product
    "0000001",            // 3: Serials, should use chip ID
    "Keyboard-FT V1.0",   // 4: HID
};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1
 * configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index,
    // total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1,
                          1,
                          0,
                          TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
                          100),

    // Interface number, string index, boot protocol, report
    // descriptor len, EP In address, size & polling
    // interval
    TUD_HID_DESCRIPTOR(0,
                       4,
                       false,
                       sizeof(hid_report_descriptor),
                       0x81,
                       16,
                       10),
};

bool SendReport(KbHidReport kbHidReport) {
    return kbReportsQueue.Send(kbHidReport);
}

static bool Init() {
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count =
            sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy             = false,
        .configuration_descriptor = hid_configuration_descriptor,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    return true;
}

static void Handler() {
    auto report = kbReportsQueue.Wait(0xFFFFFFFF);
    if (!report) {
        return;
    }
    if (!tud_mounted()) {
        return;
    }
    if (!report->size && !report->modifiers) {
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, nullptr);
        return;
    }
    std::array<uint8_t, 6> keycodes = {};
    uint16_t size = std::min(static_cast<uint16_t>(6), report->size);

    uint16_t textIndex         = 0;
    std::array<char, 100> text = {"\0"};

    for (uint16_t i = 0; i < size; ++i) {
        keycodes[i] = report->keys[i];
        textIndex += snprintf(&text[textIndex],
                              sizeof(text) - textIndex,
                              "%d ,",
                              keycodes[i]);
    }
    ESP_LOGI("Report: ", "%s modifiers: %d", text.data(), report->modifiers);
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD,
                            report->modifiers,
                            keycodes.data());
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

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR reauest
// Application return pointer to descriptor, whose contents
// must exist long enough for transfer to complete
extern "C" const uint8_t* tud_hid_descriptor_report_cb(uint8_t instance) {
    // We use only one interface and one HID report
    // descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control reauest
// Application must fill buffer report's content and return
// its length. Return zero will cause the stack to STALL
// reauest
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance,
                                          uint8_t report_id,
                                          hid_report_type_t report_type,
                                          uint8_t* buffer,
                                          uint16_t realen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)realen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
extern "C" void tud_hid_set_report_cb(uint8_t instance,
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
    status_led::SetMode(capsState ? status_led::Modes::CapsOnUsb
                                  : status_led::Modes::Usb);
}
