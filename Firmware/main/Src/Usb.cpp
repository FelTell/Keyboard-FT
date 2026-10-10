#include "Usb.hpp"

#include <algorithm>
#include <cstdlib>

#include <class/hid/hid_device.h>
#include <esp_log.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>

#include "RtosUtils.hpp"

#include "Leds.hpp"

namespace usb {

static void PollConnection();

static rtos::Timer pollConnectionTimer("PollConnectionTimer",
                                       100,
                                       true,
                                       PollConnection);
// TODO (Felipe): This should be a semaphore
static rtos::Event hidReady;

static bool isReady;

// TinyUSB descriptors

static constexpr uint8_t KEYBOARD_REPORT_ID = 1;
static constexpr uint8_t CONSUMER_REPORT_ID = 3;

static constexpr uint32_t TUSB_DESC_TOTAL_LEN =
    TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN;

static constexpr uint8_t reportDescriptor[] = {
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

bool Init() {
    tinyusb_config_t tinyUsbConfig             = TINYUSB_DEFAULT_CONFIG();
    tinyUsbConfig.task.priority                = 24;
    tinyUsbConfig.task.xCoreID                 = 0;
    tinyUsbConfig.descriptor.device            = NULL;
    tinyUsbConfig.descriptor.full_speed_config = configurationDescriptor;
    tinyUsbConfig.descriptor.string            = stringDescriptor;
    tinyUsbConfig.descriptor.string_count =
        sizeof(stringDescriptor) / sizeof(stringDescriptor[0]);

    ESP_ERROR_CHECK(tinyusb_driver_install(&tinyUsbConfig));

    pollConnectionTimer.Start();

    if (hidReady.Setup() == false) {
        return false;
    }
    hidReady.Set(1);

    return true;
}

void SendKeyboardReport(models::KeyboardReport report) {
    hidReady.Wait(1);
    tud_hid_report(KEYBOARD_REPORT_ID, report.data(), report.size());
}

void SendConsumerCode(uint16_t consumerCode) {
    hidReady.Wait(1);
    tud_hid_report(CONSUMER_REPORT_ID, &consumerCode, sizeof(consumerCode));
}

static void PollConnection() {
    const bool tinyUsbReady = tud_ready();
    if (isReady != tinyUsbReady) {
        isReady = tinyUsbReady;
        if (!isReady) {
            isReady = false;
            leds::SendCommand(leds::Commands::NotConnected);
        } else if (leds::GetMode() == leds::Commands::NotConnected) {
            leds::SendCommand(leds::Commands::Usb);
        }
    }
}

} // namespace usb

extern "C" const uint8_t* tud_hid_descriptor_report_cb(
    [[maybe_unused]] uint8_t instance) {
    return usb::reportDescriptor;
}

extern "C" uint16_t tud_hid_get_report_cb([[maybe_unused]] uint8_t instance,
                                          [[maybe_unused]] uint8_t id,
                                          [[maybe_unused]] hid_report_type_t type,
                                          [[maybe_unused]] uint8_t* buf,
                                          [[maybe_unused]] uint16_t realen) {
    return 0;
}

extern "C" void tud_hid_set_report_cb([[maybe_unused]] uint8_t instance,
                                      uint8_t id,
                                      hid_report_type_t type,
                                      const uint8_t* buf,
                                      uint16_t size) {
    if (id != 1 && type != HID_REPORT_TYPE_OUTPUT && size != 1) {
        ESP_LOGI("set report cb", "Unkown output event");
        return;
    }
    bool capsState = buf[0] & 2;
    leds::SendCommand(capsState ? leds::Commands::CapsOnUsb : leds::Commands::Usb);
}

extern "C" void tud_hid_report_complete_cb([[maybe_unused]] uint8_t instance,
                                           [[maybe_unused]] const uint8_t* report,
                                           [[maybe_unused]] uint16_t len) {
    usb::hidReady.Set(1);
}
