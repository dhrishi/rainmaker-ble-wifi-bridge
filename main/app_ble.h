/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once
#include <sdkconfig.h>
#include <esp_err.h>

#define MAX_DEV CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#define SCAN_DURATION_MS (30 * 1000)
#define RESCAN_DURATION_MS (5 * 1000)

typedef esp_err_t (*add_func_t)(void);
typedef struct ble_dev *ble_dev_handle_t;

typedef struct {
    /* Name seen in BLE advertisement data */
    const char *adv_name;
    /* 16-bit BLE Characteristic UUID of the parameter to be controlled */
    uint16_t chr_uuid;
    /* 16-bit BLE Service UUID of the characteristic */
    uint16_t svc_uuid;
    /* Function to add device and its parameters to RainMaker */
    add_func_t add;
} ble_cfg_t;

/**
 * Start BLE framework
 *
 * This API will start BLE central role and return after scanning for 30 seconds.
 * The interval can be changed by configuring SCAN_DURATION_MS. During this time,
 * it will find all the registered BLE devices of which are discoverable and add them
 * to the RainMaker framework (maximum upto MAX_DEV)
 *
 * @note This API should be called after esp_rmaker_init() but before esp_rmaker_start()
 */
void app_ble_start(void);

/**
 * Create and add RainMaker a device and its parameters for the corresponding BLE device
 *
 * This API will add a RainMaker device and its parameters as per the functionality
 * exposed by the BLE device.
 *
 * @param[in] cfg BLE configuration of type ble_cfg_t
 *
 * @return BLE device handle if the device is added successfully.
 * @return NULL in case of failures.
 */
ble_dev_handle_t app_ble_add_dev(ble_cfg_t *cfg);

/**
 * Update the BLE device parameter
 *
 * This API will update (write) the parameter (characteristic) value over BLE
 *
 * @param[in] dev BLE device handle returned from app_ble_add_dev()
 * @param[in] data Data to be written
 * @param[in] len Length of the data
 *
 * @return ESP_OK if successful.
 * @return error in case of failures.
 *
 * @note The call to this API should be preceded by mapping the data received from
 * RainMaker cloud to the format accepted by the BLE device.
 */
esp_err_t app_ble_update_dev(ble_dev_handle_t dev, uint8_t *data, int len);
