#include "UsbHid.hpp"

#include <algorithm>
#include <cstdlib>

#include <class/hid/hid_device.h>
#include <esp_log.h>
#include <tinyusb.h>
#include <tinyusb_default_config.h>

#include "RtosUtils.hpp"

#include "Leds.hpp"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_bt_defs.h"
#if CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#endif
#include "esp_bt_device.h"
#include "esp_bt_main.h"

#include "esp_hid_gap.h"
#include "esp_hidd.h"

#define HIDD_BLE_MODE 0x01

namespace usb_hid {

static constexpr uint8_t REPORT_MAX_KEYS = 6;
static constexpr uint8_t REPORT_SIZE     = 2 + REPORT_MAX_KEYS;

static bool Init();
static void Handler();

static void PollConnection();
static void PrintReport(std::array<uint8_t, REPORT_SIZE>& report);

static rtos::Task task("UsbHidTask", 4096, 24, Init, Handler, 0);
static rtos::Timer pollConnectionTimer("PollConnectionTimer",
                                       100,
                                       true,
                                       PollConnection);
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

const unsigned char keyboardReportMap[] = {
    // 7 bytes input (modifiers, resrvd, keys*5), 1 byte output
    0x05,
    0x01, // Usage Page (Generic Desktop Ctrls)
    0x09,
    0x06, // Usage (Keyboard)
    0xA1,
    0x01, // Collection (Application)
    0x85,
    0x01, //   Report ID (1)
    0x05,
    0x07, //   Usage Page (Kbrd/Keypad)
    0x19,
    0xE0, //   Usage Minimum (0xE0)
    0x29,
    0xE7, //   Usage Maximum (0xE7)
    0x15,
    0x00, //   Logical Minimum (0)
    0x25,
    0x01, //   Logical Maximum (1)
    0x75,
    0x01, //   Report Size (1)
    0x95,
    0x08, //   Report Count (8)
    0x81,
    0x02, //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
          //   Position)
    0x95,
    0x01, //   Report Count (1)
    0x75,
    0x08, //   Report Size (8)
    0x81,
    0x03, //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null
          //   Position)
    0x95,
    0x05, //   Report Count (5)
    0x75,
    0x01, //   Report Size (1)
    0x05,
    0x08, //   Usage Page (LEDs)
    0x19,
    0x01, //   Usage Minimum (Num Lock)
    0x29,
    0x05, //   Usage Maximum (Kana)
    0x91,
    0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null
          //   Position,Non-volatile)
    0x95,
    0x01, //   Report Count (1)
    0x75,
    0x03, //   Report Size (3)
    0x91,
    0x03, //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null
          //   Position,Non-volatile)
    0x95,
    0x05, //   Report Count (5)
    0x75,
    0x08, //   Report Size (8)
    0x15,
    0x00, //   Logical Minimum (0)
    0x25,
    0x65, //   Logical Maximum (101)
    0x05,
    0x07, //   Usage Page (Kbrd/Keypad)
    0x19,
    0x00, //   Usage Minimum (0x00)
    0x29,
    0x65, //   Usage Maximum (0x65)
    0x81,
    0x00, //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null
          //   Position)
    0xC0, // End Collection

    // 65 bytes
};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {.data = keyboardReportMap, .len = sizeof(keyboardReportMap)},

};

static esp_hid_device_config_t ble_hid_config = {.vendor_id   = 0x16C0,
                                                 .product_id  = 0x05DF,
                                                 .version     = 0x0100,
                                                 .device_name = "Keyboard-FT",
                                                 .manufacturer_name =
                                                     "Espressif",
                                                 .serial_number = "1234567890",
                                                 .report_maps = ble_report_maps,
                                                 .report_maps_len = 1};

static void ble_hidd_event_callback(void* handler_args,
                                    esp_event_base_t base,
                                    int32_t id,
                                    void* event_data) {
    esp_hidd_event_t event       = (esp_hidd_event_t)id;
    esp_hidd_event_data_t* param = (esp_hidd_event_data_t*)event_data;
    static const char* TAG       = "HID_DEV_BLE";

    switch (event) {
        case ESP_HIDD_START_EVENT: {
            ESP_LOGI(TAG, "START");
            esp_hid_ble_gap_adv_start();
            break;
        }
        case ESP_HIDD_CONNECT_EVENT: {
            ESP_LOGI(TAG, "CONNECT");
            break;
        }
        case ESP_HIDD_PROTOCOL_MODE_EVENT: {
            ESP_LOGI(TAG,
                     "PROTOCOL MODE[%u]: %s",
                     param->protocol_mode.map_index,
                     param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
            break;
        }
        case ESP_HIDD_CONTROL_EVENT: {
            ESP_LOGI(TAG,
                     "CONTROL[%u]: %sSUSPEND",
                     param->control.map_index,
                     param->control.control ? "EXIT_" : "");
            if (param->control.control) {
                // exit suspend
                // ble_hid_task_start_up();
            } else {
                // suspend
                // ble_hid_task_shut_down();
            }
            break;
        }
        case ESP_HIDD_OUTPUT_EVENT: {
            ESP_LOGI(TAG,
                     "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:",
                     param->output.map_index,
                     esp_hid_usage_str(param->output.usage),
                     param->output.report_id,
                     param->output.length);
            ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
            break;
        }
        case ESP_HIDD_FEATURE_EVENT: {
            ESP_LOGI(TAG,
                     "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:",
                     param->feature.map_index,
                     esp_hid_usage_str(param->feature.usage),
                     param->feature.report_id,
                     param->feature.length);
            ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
            break;
        }
        case ESP_HIDD_DISCONNECT_EVENT: {
            ESP_LOGI(TAG,
                     "DISCONNECT: %s",
                     esp_hid_disconnect_reason_str(
                         esp_hidd_dev_transport_get(param->disconnect.dev),
                         param->disconnect.reason));
            // ble_hid_task_shut_down();
            esp_hid_ble_gap_adv_start();
            break;
        }
        case ESP_HIDD_STOP_EVENT: {
            ESP_LOGI(TAG, "STOP");
            break;
        }
        default:
            break;
    }
    return;
}


typedef struct {
    TaskHandle_t task_hdl;
    esp_hidd_dev_t* hid_dev;
    uint8_t protocol_mode;
    uint8_t* buffer;
} local_param_t;

static local_param_t s_ble_hid_param = {};

static bool Init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("BleHid", "setting hid gap, mode:%d", HID_DEV_MODE);
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK(ret);
    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_GENERIC, "Keyboard-FT");
    ESP_ERROR_CHECK(ret);

    if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) !=
        ESP_OK) {
        ESP_LOGE("BleHid", "GATTS register callback failed: %d", ret);
        return false;
    }
    ESP_LOGI("BleHid", "setting ble device");
    ESP_ERROR_CHECK(esp_hidd_dev_init(&ble_hid_config,
                                      ESP_HID_TRANSPORT_BLE,
                                      ble_hidd_event_callback,
                                      &s_ble_hid_param.hid_dev));

    pollConnectionTimer.Start();

    return true;
}

static void Handler() {
    static uint16_t lastConsumerCode;
    static std::array<uint8_t, REPORT_SIZE> keyCodes = {};
    rtos::Delay(1000);
    return;

    KbHidReport report;
    if (!kbReportsQueue.Wait(report, 1000)) {
        tud_hid_report(KEYBOARD_REPORT_ID, keyCodes.data(), REPORT_SIZE);
        return;
    }

    if (lastConsumerCode != report.consumerCode) {
        lastConsumerCode = report.consumerCode;
        tud_hid_report(CONSUMER_REPORT_ID, &lastConsumerCode, 2);
        ESP_LOGI("ConsumerReport: ", "%d", report.consumerCode);
        return;
    }

    keyCodes[0] = report.modifiers;
    memcpy(&keyCodes[2], report.keys.data(), REPORT_MAX_KEYS);

    tud_hid_report(KEYBOARD_REPORT_ID, keyCodes.data(), REPORT_SIZE);

    PrintReport(keyCodes);

    leds::ResetTimeout();
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
    leds::SendCommand(capsState ? leds::Commands::CapsOnUsb
                                : leds::Commands::Usb);
}
