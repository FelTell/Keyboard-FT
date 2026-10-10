#include "Hid.hpp"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <tinyusb_default_config.h>

#include <esp_hidd.h>

#include "Leds.hpp"
#include "RtosUtils.hpp"

namespace ble {

static constexpr const char* deviceName = "Keyboard-FT";

static constexpr uint8_t KEYBOARD_ID = 1;
static constexpr uint8_t CONSUMER_ID = 3;

static constexpr uint8_t reportsMap[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(KEYBOARD_ID)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(CONSUMER_ID))};
static esp_hid_raw_report_map_t reportsMaps[] = {
    {.data = reportsMap, .len = sizeof(reportsMap)}};

static esp_hidd_dev_t* hidDev = {};

static bool isInitialized = false;

static esp_err_t AdvertisingStart(void);
static void EventCallback(void* args, esp_event_base_t base, int32_t id, void* data);

void DeInit() {
    esp_hidd_dev_deinit(hidDev);
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    isInitialized = false;
}

bool Init() {
    if (isInitialized) {
        return true;
    }

    esp_bt_controller_config_t btConfig    = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bluedroid_config_t bluedroidConfig = {};
    esp_ble_io_cap_t iocap                 = ESP_IO_CAP_NONE;
    // clang-format off
    uint8_t serviceUuid[] = {0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x12,0x18,0x00,0x00};
    // clang-format on
    esp_ble_adv_data_t advertisingData = {};
    advertisingData.include_name       = true;
    advertisingData.include_txpower    = true;
    advertisingData.min_interval       = 0x0006;
    advertisingData.max_interval       = 0x0010;
    advertisingData.appearance         = ESP_HID_APPEARANCE_KEYBOARD;
    advertisingData.service_uuid_len   = sizeof(serviceUuid);
    advertisingData.p_service_uuid     = serviceUuid;
    advertisingData.flag               = 0x6;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = esp_bt_controller_init(&btConfig);
    ESP_ERROR_CHECK(ret);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_init_with_cfg(&bluedroidConfig);
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_enable();
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_set_device_name(deviceName);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_config_adv_data(&advertisingData);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
    ESP_ERROR_CHECK(ret);

    static esp_hid_device_config_t hidConfig = {.vendor_id         = 0x16C0,
                                                .product_id        = 0x05DF,
                                                .version           = 0x0100,
                                                .device_name       = deviceName,
                                                .manufacturer_name = "Espressif",
                                                .serial_number     = "1234567890",
                                                .report_maps       = reportsMaps,
                                                .report_maps_len   = 1};
    ret = esp_hidd_dev_init(&hidConfig, ESP_HID_TRANSPORT_BLE, EventCallback, &hidDev);
    ESP_ERROR_CHECK(ret);

    isInitialized = true;
    return true;
}

void SendKeyboardReport(models::KeyboardReport report) {
    esp_hidd_dev_input_set(hidDev, 0, KEYBOARD_ID, report.data(), report.size());
}

void SendConsumerCode(uint16_t consumerCode) {
    esp_hidd_dev_input_set(hidDev,
                           0,
                           CONSUMER_ID,
                           reinterpret_cast<uint8_t*>(&consumerCode),
                           sizeof(consumerCode));
}

static void EventCallback(void* args, esp_event_base_t base, int32_t id, void* data) {
    esp_hidd_event_t event       = (esp_hidd_event_t)id;
    esp_hidd_event_data_t* param = (esp_hidd_event_data_t*)data;
    static const char* TAG       = "HID_DEV_BLE";

    switch (event) {
        case ESP_HIDD_START_EVENT: {
            ESP_LOGI(TAG, "START");
            AdvertisingStart();
        } break;
        case ESP_HIDD_CONNECT_EVENT: {
            ESP_LOGI(TAG, "CONNECT");
        } break;
        case ESP_HIDD_OUTPUT_EVENT: {
            if (param->output.report_id != 1 || param->output.length != 1) {
                ESP_LOGI(TAG, "Unkown output event");
                break;
            }
            const bool capsState = param->output.data[0] & 2;
            ESP_LOGI(TAG, "capsState: %d", capsState);
            leds::SendCommand(capsState ? leds::Commands::CapsOnUsb
                                        : leds::Commands::BluetoothConnected);
        } break;
        case ESP_HIDD_DISCONNECT_EVENT: {
            ESP_LOGI(TAG,
                     "DISCONNECT: %s",
                     esp_hid_disconnect_reason_str(
                         esp_hidd_dev_transport_get(param->disconnect.dev),
                         param->disconnect.reason));
            AdvertisingStart();
        } break;
        default: {
            ESP_LOGI(TAG, "Unknown event: %d", event);
        } break;
    }
}

static esp_err_t AdvertisingStart(void) {
    static esp_ble_adv_params_t hidd_adv_params = {};
    hidd_adv_params.adv_int_min                 = 0x20;
    hidd_adv_params.adv_int_max                 = 0x30;
    hidd_adv_params.adv_type                    = ADV_TYPE_IND;
    hidd_adv_params.own_addr_type               = BLE_ADDR_TYPE_PUBLIC;
    hidd_adv_params.channel_map                 = ADV_CHNL_ALL;
    hidd_adv_params.adv_filter_policy           = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    return esp_ble_gap_start_advertising(&hidd_adv_params);
}

} // namespace ble
