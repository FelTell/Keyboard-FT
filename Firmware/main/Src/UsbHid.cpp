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
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"

#include "esp_hid_gap.h"
#include "esp_hidd.h"

namespace usb_hid {

static constexpr uint8_t REPORT_MAX_KEYS = 6;
static constexpr uint8_t REPORT_SIZE     = 2 + REPORT_MAX_KEYS;

static bool Init();
static void Handler();

static void PrintReport(std::array<uint8_t, REPORT_SIZE>& report);

static rtos::Task task("UsbHidTask", 4096, 24, Init, Handler, 0);
static rtos::Queue<KbHidReport> kbReportsQueue(10);

static constexpr uint8_t KEYBOARD_REPORT_ID = 1;
static constexpr uint8_t CONSUMER_REPORT_ID = 3;

bool SendReport(KbHidReport kbHidReport) {
    return kbReportsQueue.Send(kbHidReport);
}

const uint8_t reportsMap[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_REPORT_ID)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(CONSUMER_REPORT_ID))};

static esp_hid_raw_report_map_t reportsMaps[] = {
    {.data = reportsMap, .len = sizeof(reportsMap)}};

static esp_hid_device_config_t hidConfig = {.vendor_id         = 0x16C0,
                                            .product_id        = 0x05DF,
                                            .version           = 0x0100,
                                            .device_name       = "Keyboard-FT",
                                            .manufacturer_name = "Espressif",
                                            .serial_number     = "1234567890",
                                            .report_maps       = reportsMaps,
                                            .report_maps_len   = 1};

static void HidEventCallback(void* handlerArgs,
                             esp_event_base_t base,
                             int32_t id,
                             void* eventData) {
    esp_hidd_event_t event       = (esp_hidd_event_t)id;
    esp_hidd_event_data_t* param = (esp_hidd_event_data_t*)eventData;
    static const char* TAG       = "HID_DEV_BLE";

    switch (event) {
        case ESP_HIDD_START_EVENT: {
            ESP_LOGI(TAG, "START");
            AdvertisingStart();
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
            auto size = param->output.length;
            auto id   = param->output.report_id;
            if (id != 1 || size != 1) {
                // Unknown message, log and ignore it
                ESP_LOGI(TAG, "Len: %d, Data:", id, size);
                ESP_LOG_BUFFER_HEX(TAG, param->output.data, size);
                break;
            }
            bool capsState = param->output.data[0] & KEYBOARD_LED_CAPSLOCK;
            ESP_LOGI(TAG, "capsState: %d", capsState);
            leds::SendCommand(capsState ? leds::Commands::CapsOnUsb
                                        : leds::Commands::BluetoothConnected);
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
            AdvertisingStart();
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

static local_param_t hidParams = {};

static bool Init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = BleGatInit("Keyboard-FT");
    ESP_ERROR_CHECK(ret);

    if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) !=
        ESP_OK) {
        ESP_LOGE("BleHid", "GATTS register callback failed: %d", ret);
        return false;
    }
    ESP_LOGI("BleHid", "setting ble device");
    ESP_ERROR_CHECK(esp_hidd_dev_init(&hidConfig,
                                      ESP_HID_TRANSPORT_BLE,
                                      HidEventCallback,
                                      &hidParams.hid_dev));

    return true;
}

static void Handler() {
    static uint16_t lastConsumerCode;
    static std::array<uint8_t, REPORT_SIZE> keyCodes = {};

    KbHidReport report;
    kbReportsQueue.Wait(report);

    if (lastConsumerCode != report.consumerCode) {
        lastConsumerCode = report.consumerCode;
        esp_hidd_dev_input_set(hidParams.hid_dev,
                               0,
                               CONSUMER_REPORT_ID,
                               reinterpret_cast<uint8_t*>(&lastConsumerCode),
                               2);
        ESP_LOGI("ConsumerReport: ", "%d", report.consumerCode);
        return;
    }

    keyCodes[0] = report.modifiers;
    memcpy(&keyCodes[2], report.keys.data(), REPORT_MAX_KEYS);

    esp_hidd_dev_input_set(hidParams.hid_dev,
                           0,
                           KEYBOARD_REPORT_ID,
                           keyCodes.data(),
                           REPORT_SIZE);

    PrintReport(keyCodes);

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

} // namespace usb_hid
