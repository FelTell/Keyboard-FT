#include "esp_hid_gap.h"

#include <esp_bt_main.h>

esp_err_t AdvertisingStart(void) {
    static esp_ble_adv_params_t hidd_adv_params = {
        .adv_int_min       = 0x20,
        .adv_int_max       = 0x30,
        .adv_type          = ADV_TYPE_IND,
        .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
        .channel_map       = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    return esp_ble_gap_start_advertising(&hidd_adv_params);
}

esp_err_t BleGatInit(const char* device_name) {
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg    = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bluedroid_config_t bluedroid_cfg = {};
    // clang-format off
    const uint8_t hidd_service_uuid128[] = {0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x12,0x18,0x00,0x00};
    // clang-format on
    esp_ble_adv_data_t ble_adv_data = {
        .set_scan_rsp        = false,
        .include_name        = true,
        .include_txpower     = true,
        .min_interval        = 0x0006,
        .max_interval        = 0x0010,
        .appearance          = ESP_HID_APPEARANCE_KEYBOARD,
        .manufacturer_len    = 0,
        .p_manufacturer_data = NULL,
        .service_data_len    = 0,
        .p_service_data      = NULL,
        .service_uuid_len    = sizeof(hidd_service_uuid128),
        .p_service_uuid      = (uint8_t*)hidd_service_uuid128,
        .flag                = 0x6,
    };
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;

    ret = esp_bt_controller_init(&bt_cfg);
    ESP_ERROR_CHECK(ret);

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    ESP_ERROR_CHECK(ret);

    ret = esp_bluedroid_enable();
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, 1);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_set_device_name(device_name);
    ESP_ERROR_CHECK(ret);

    ret = esp_ble_gap_config_adv_data(&ble_adv_data);
    ESP_ERROR_CHECK(ret);

    return ret;
}
