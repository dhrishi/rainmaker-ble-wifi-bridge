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
#include "syska_light.h"

#define RED_INDEX 11
#define GREEN_INDEX 12
#define BLUE_INDEX 13

#define DEFAULT_POWER       true
#define DEFAULT_HUE         180
#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  25

static const char *TAG = "syska_light";
static ble_dev_handle_t s_dev;
static uint16_t g_hue;
static uint16_t g_saturation;
static uint16_t g_value;
static bool g_power;

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

static esp_err_t syska_light_update_dev(uint32_t red, uint32_t green, uint32_t blue)
{
    int rc = ESP_FAIL;
    uint8_t value[18] = {0x00, 0x09, /* Hard coding the first 2 sequence number bytes*/ 0x00, 0x06, 0x00, 0x0a, 0x03, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memcpy(&value[RED_INDEX], &red, sizeof(uint8_t));
    memcpy(&value[GREEN_INDEX], &green, sizeof(uint8_t));
    memcpy(&value[BLUE_INDEX], &blue, sizeof(uint8_t));

    rc = app_ble_update_dev(s_dev, value, sizeof(value));
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update the light state");
    }
    return rc;
}

static esp_err_t app_light_set_led(const char *dev_name, uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    g_hue = hue;
    g_saturation = saturation;
    g_value = brightness;
    led_strip_hsv2rgb(g_hue, g_saturation, g_value, &red, &green, &blue);
    return syska_light_update_dev(red, green, blue);
}

static esp_err_t app_light_set(const char *dev_name, uint32_t hue, uint32_t saturation, uint32_t brightness)
{
    /* Whenever this function is called, light power will be ON */
    if (!g_power) {
        g_power = true;
        esp_rmaker_update_param(dev_name, ESP_RMAKER_DEF_POWER_NAME, esp_rmaker_bool(g_power));
    }
    return app_light_set_led(dev_name, hue, saturation, brightness);
}

static esp_err_t app_light_set_power(const char *dev_name, bool power)
{
    g_power = power;
    if (power) {
        return app_light_set(dev_name, g_hue, g_saturation, g_value);
    } else {
        return syska_light_update_dev(0, 0, 0);
    }
}

static esp_err_t app_light_set_brightness(const char *dev_name, uint16_t brightness)
{
    g_value = brightness;
    return app_light_set(dev_name, g_hue, g_saturation, g_value);
}

static esp_err_t app_light_set_hue(const char *dev_name, uint16_t hue)
{
    g_hue = hue;
    return app_light_set(dev_name, g_hue, g_saturation, g_value);
}

static esp_err_t app_light_set_saturation(const char *dev_name, uint16_t saturation)
{
    g_saturation = saturation;
    return app_light_set(dev_name, g_hue, g_saturation, g_value);
}

static esp_err_t syska_light_cb(const char *dev_name, const char *name, esp_rmaker_param_val_t val, void *priv_data)
{
    esp_err_t ret;
    if (strcmp(name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", dev_name, name);
        ret = app_light_set_power(dev_name, val.val.b);
    } else if (strcmp(name, "brightness") == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, dev_name, name);
        ret = app_light_set_brightness(dev_name, val.val.i);
    } else if (strcmp(name, "hue") == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, dev_name, name);
        ret = app_light_set_hue(dev_name, val.val.i);
    } else if (strcmp(name, "saturation") == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, dev_name, name);
        ret = app_light_set_saturation(dev_name, val.val.i);
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    if (ret == ESP_OK) {
        esp_rmaker_update_param(dev_name, name, val);
    }
    return ESP_OK;
}

esp_err_t syska_light_add_dev(void)
{
    /* Create a device and add the relevant parameters to it */
    esp_rmaker_create_lightbulb_device("Syska Light", syska_light_cb, NULL, DEFAULT_POWER);

    esp_rmaker_device_add_brightness_param("Syska Light", "brightness", DEFAULT_BRIGHTNESS);
    esp_rmaker_device_add_hue_param("Syska Light", "hue", DEFAULT_HUE);
    esp_rmaker_device_add_saturation_param("Syska Light", "saturation", DEFAULT_SATURATION);
    return ESP_OK;
}

esp_err_t syska_light_register(void)
{
    ble_cfg_t ble_cfg;
    ble_cfg.adv_name = "Cnligh";
    ble_cfg.svc_uuid = 0xf371;
    ble_cfg.chr_uuid = 0xfff1;
    ble_cfg.add = syska_light_add_dev;

    s_dev = app_ble_add_dev(&ble_cfg);
    if (!s_dev) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
