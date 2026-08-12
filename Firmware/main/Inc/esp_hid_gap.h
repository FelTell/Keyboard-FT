#pragma once

#include "esp_err.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_hid_common.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t BleGatInit(const char* device_name);
esp_err_t AdvertisingStart(void);

#ifdef __cplusplus
}
#endif
