
#include "Bluetooth/BluetoothController.hpp"
#include "esp_bt.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <array>

#include "RtosUtils.hpp"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gatts_api.h"
#include "esp_hidd_prf_api.hpp"
#include "hid_dev.hpp"

namespace bluetooth::controller {

static bool Init();
static void Handler();
static void HidEventCallback(esp_hidd_cb_event_t event,
                             esp_hidd_cb_param_t* param);

static constexpr const char* TAG         = "BluetoothTask";
static constexpr const char* DEVICE_NAME = "Keyboard-FT";

static rtos::Task task(TAG, 4096, 24, Init, Handler);
static rtos::Queue<model::hid::Report> reportsQueue(10);
static rtos::Event events;
static constexpr uint32_t IS_CONNECTED_FLAG = 1 << 0;

static uint16_t connectionId = 0;

bool SetupTask() {
    if (!task.Setup()) {
        return false;
    }
    if (!reportsQueue.Setup()) {
        return false;
    }
    if (!events.Setup()) {
        return false;
    }
    return true;
}

bool SendReport(model::hid::Report report) {
    return reportsQueue.Send(report);
}

static uint8_t uuid[] = {0xfb,
                         0x34,
                         0x9b,
                         0x5f,
                         0x80,
                         0x00,
                         0x00,
                         0x80,
                         0x00,
                         0x10,
                         0x00,
                         0x00,
                         0x12,
                         0x18,
                         0x00,
                         0x00};

static esp_ble_adv_data_t hiddAdvData = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x03c0,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(uuid),
    .p_service_uuid      = uuid,
    .flag                = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min   = 0x20,
    .adv_int_max   = 0x30,
    .adv_type      = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map       = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void HidEventCallback(esp_hidd_cb_event_t event,
                             esp_hidd_cb_param_t* param) {
    switch (event) {
        case ESP_HIDD_EVENT_REG_FINISH: {
            if (param->init_finish.state == ESP_HIDD_INIT_OK) {
                esp_ble_gap_set_device_name(DEVICE_NAME);
                esp_ble_gap_config_adv_data(&hiddAdvData);
            }
            break;
        }
        case ESP_BAT_EVENT_REG: {
            break;
        }
        case ESP_HIDD_EVENT_DEINIT_FINISH:
            break;
        case ESP_HIDD_EVENT_BLE_CONNECT: {
            ESP_LOGI(TAG, "ESP_HIDD_EVENT_BLE_CONNECT");
            connectionId = param->connect.conn_id;
            break;
        }
        case ESP_HIDD_EVENT_BLE_DISCONNECT: {
            events.Clear(IS_CONNECTED_FLAG);
            ESP_LOGI(TAG, "ESP_HIDD_EVENT_BLE_DISCONNECT");
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        }
        case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT: {
            ESP_LOGI(TAG,
                     "%s, ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT",
                     __func__);
            ESP_LOG_BUFFER_HEX(TAG,
                               param->vendor_write.data,
                               param->vendor_write.length);
            break;
        }
        case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT: {
            ESP_LOGI(TAG, "ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT");
            ESP_LOG_BUFFER_HEX(TAG,
                               param->led_write.data,
                               param->led_write.length);
            break;
        }
        default:
            break;
    }
    return;
}

static void BleEventHandler(esp_gap_ble_cb_event_t event,
                            esp_ble_gap_cb_param_t* param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&hidd_adv_params);
            break;
        case ESP_GAP_BLE_SEC_REQ_EVT:
            for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
                ESP_LOGD(TAG, "%x:", param->ble_security.ble_req.bd_addr[i]);
            }
            esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
            break;
        case ESP_GAP_BLE_AUTH_CMPL_EVT:
            events.Set(IS_CONNECTED_FLAG);
            esp_bd_addr_t bd_addr;
            memcpy(bd_addr,
                   param->ble_security.auth_cmpl.bd_addr,
                   sizeof(esp_bd_addr_t));
            ESP_LOGI(TAG,
                     "remote BD_ADDR: %08x%04x",
                     (bd_addr[0] << 24) + (bd_addr[1] << 16) +
                         (bd_addr[2] << 8) + bd_addr[3],
                     (bd_addr[4] << 8) + bd_addr[5]);
            ESP_LOGI(TAG,
                     "address type = %d",
                     param->ble_security.auth_cmpl.addr_type);
            ESP_LOGI(TAG,
                     "pair status = %s",
                     param->ble_security.auth_cmpl.success ? "success"
                                                           : "fail");
            if (!param->ble_security.auth_cmpl.success) {
                ESP_LOGE(TAG,
                         "fail reason = 0x%x",
                         param->ble_security.auth_cmpl.fail_reason);
            }
            break;
        default:
            break;
    }
}

static constexpr uint8_t REPORT_MAX_KEYS = 6;
static constexpr uint8_t REPORT_SIZE     = 2 + REPORT_MAX_KEYS;

void Handler() {
    static uint16_t lastConsumerCode;
    static model::hid::Report report;

    if (!reportsQueue.Wait(report, 1000)) {
        esp_hidd_send_keyboard_value(connectionId,
                                     report.modifiers,
                                     report.keys.data(),
                                     6);

        return;
    }

    if (lastConsumerCode != report.consumerCode) {
        lastConsumerCode = report.consumerCode;
        esp_hidd_send_consumer_value(connectionId, lastConsumerCode, true);
        ESP_LOGI("ConsumerReport: ", "%d", report.consumerCode);
        return;
    }

    esp_hidd_send_keyboard_value(connectionId,
                                 report.modifiers,
                                 report.keys.data(),
                                 6);
}

bool Init() {
    esp_err_t ret;

    // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret                               = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed\n", __func__);
        return false;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed\n", __func__);
        return false;
    }

    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluedroid failed\n", __func__);
        return false;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluedroid failed\n", __func__);
        return false;
    }

    if ((ret = esp_hidd_profile_init()) != ESP_OK) {
        ESP_LOGE(TAG, "%s init bluedroid failed\n", __func__);
    }

    /// register the callback function to the gap module
    esp_ble_gap_register_callback(BleEventHandler);
    esp_hidd_register_callbacks(HidEventCallback);

    /* set the security iocap & auth_req & key size & init key response key
     * parameters to the stack*/
    esp_ble_auth_req_t auth_req =
        ESP_LE_AUTH_BOND; // bonding with peer device after authentication
    esp_ble_io_cap_t iocap =
        ESP_IO_CAP_NONE;   // set the IO capability to No output No input
    uint8_t key_size = 16; // the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key  = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,
                                   &auth_req,
                                   sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,
                                   &iocap,
                                   sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,
                                   &key_size,
                                   sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which
    types of key of the master should distribute to you, and the response key
    means which key you can distribute to the Master; If your BLE device act as
    a master, the response key means you hope which types of key of the slave
    should distribute to you, and the init key means which key you can
    distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,
                                   &init_key,
                                   sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,
                                   &rsp_key,
                                   sizeof(uint8_t));

    return true;
}

} // namespace bluetooth::controller
