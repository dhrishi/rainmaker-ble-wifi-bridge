/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <string.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>

#include "app_ble.h"
#include "sample_accessory.h"

static const char *TAG = "sample_accessory";
static ble_dev_handle_t s_dev;

static esp_err_t sample_accessory_update_dev(void)
{
    int rc = ESP_FAIL;
    uint8_t value[REQD_DATA_SIZE];

    /* Generate the sequence of bytes in the format required by the BLE accessory and write over BLE */
    rc = app_ble_update_dev(s_dev, value, sizeof(value));
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update the accessory state");
    }
    return rc;
}

static esp_err_t sample_accessory_cb(const char *dev_name, const char *name, esp_rmaker_param_val_t val, void *priv_data)
{
    esp_err_t ret;
    if (strcmp(name, "PARAM1_NAME") == 0) {
        /* You can pass the required parameters to sample_accessory_update_dev */
        sample_accessory_update_dev();
    } else if (strcmp(name, "PARAM2_NAME") == 0) {
        /* You can pass the required parameters to sample_accessory_update_dev */
        sample_accessory_update_dev();
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    if (ret == ESP_OK) {
        esp_rmaker_update_param(dev_name, name, val);
    }
    return ESP_OK;
}

esp_err_t sample_accessory_add_dev(void)
{
    /* Create a device and add its relevant parameters using ESP RainMaker APIs.
     * Refer API Reference documentation here: https://docs.espressif.com/projects/esp-rainmaker/en/latest/c-api-reference/rainmaker_standard_types.html */

    esp_rmaker_create_device("Sample Accessory Name", "Accessory Type", sample_accessory_cb, NULL);
    esp_rmaker_device_add_name_param("Sample Accessory Name", "name");
    esp_rmaker_device_add_<parameter1_name>_param("Sample Accessory Name", "PARAM1_NAME", DEFAULT_PARAM1_VALUE);
    esp_rmaker_device_add_<parameter2_name>_param("Sample Accessory Name", "PARAM2_NAME", DEFAULT_PARAM2_VALUE);

    return ESP_OK;
}

esp_err_t sample_accessory_register(void)
{
    /* Populate the parameters below. Refer the documentation in main/app_ble.h for details on the parameters */
    ble_cfg_t ble_cfg;
    ble_cfg.adv_name = "";
    ble_cfg.svc_uuid = ;
    ble_cfg.chr_uuid = ;
    ble_cfg.add = sample_accessory_add_dev;

    s_dev = app_ble_add_dev(&ble_cfg);
    if (!s_dev) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
