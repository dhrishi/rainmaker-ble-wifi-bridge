/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
/* BLE */
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "app_ble.h"
#include "app_priv.h"

static const char *TAG = "app_ble";

struct ble_dev {
    const char *adv_name;
    uint16_t svc_uuid;
    uint16_t chr_uuid;
    add_func_t add;
    uint16_t conn_handle;
    ble_addr_t addr;
    struct ble_gatt_svc svc;
    struct ble_gatt_chr chr;
};

static struct ble_dev s_ble_dev[MAX_DEV];
static SemaphoreHandle_t x_sem;

static int app_ble_gap_event(struct ble_gap_event *event, void *arg);

ble_dev_handle_t app_ble_add_dev(ble_cfg_t *cfg)
{
    int i;
    if (!cfg->adv_name || !cfg->add) {
        ESP_LOGE(TAG, "Incorrect input");
        return NULL;
    }

    for (i = 0; i < MAX_DEV; i++) {
        if (!s_ble_dev[i].adv_name) {
           break;
        }
    }
    if (i == MAX_DEV) {
        ESP_LOGE(TAG, "Max limit reached");
        return NULL;
    }
    ESP_LOGD(TAG, "Adding device at index %d", i);
    s_ble_dev[i].adv_name = cfg->adv_name;
    s_ble_dev[i].svc_uuid = cfg->svc_uuid;
    s_ble_dev[i].chr_uuid = cfg->chr_uuid;
    s_ble_dev[i].add = cfg->add;
    s_ble_dev[i].conn_handle = BLE_HS_CONN_HANDLE_NONE;

    return (void *)&s_ble_dev[i];
}

void ble_store_config_init(void);

/**
 * Initiates the GAP general discovery procedure.
 */
static void app_ble_scan(bool first_time)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc, duration_ms;
    static int64_t time;

    if (first_time) {
        duration_ms = SCAN_DURATION_MS;
        time = esp_timer_get_time();
    } else {
        duration_ms = SCAN_DURATION_MS - ((esp_timer_get_time() - time) / 1000);
    }
    if (duration_ms < 0) {
        duration_ms = 0;
    }
    ESP_LOGI(TAG, "Starting scan for duration: %u", duration_ms);
    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, duration_ms, &disc_params,
                      app_ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
    }
}

static int app_ble_should_connect(const struct ble_gap_disc_desc *disc, uint32_t *dev_index)
{
    struct ble_hs_adv_fields fields;
    int rc;

    /* The device has to be advertising connectability. */
    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {

        return 0;
    }

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return rc;
    }

    char s[BLE_HS_ADV_MAX_SZ];
    int i;
    memcpy(s, fields.name, fields.name_len);
    s[fields.name_len] = '\0';
    for (i = 0; i < MAX_DEV; i++) {
        if (s_ble_dev[i].adv_name) {
            if (strncmp((const char *)s, s_ble_dev[i].adv_name, strlen(s_ble_dev[i].adv_name)) == 0
                    && (s_ble_dev[i].conn_handle == BLE_HS_CONN_HANDLE_NONE)) {
                *dev_index = i;
                return 1;
            }
        }
    }
    return 0;
}

static char *addr_str(const void *addr)
{
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return buf;
}

static void app_ble_connect_if_interesting(const struct ble_gap_disc_desc *disc)
{
    uint8_t own_addr_type;
    int rc;
    uint32_t dev_index = 0xff;

    /* Don't do anything if we don't care about this advertiser. */
    if (!app_ble_should_connect(disc, &dev_index)) {
        return;
    }

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        ESP_LOGD(TAG, "Failed to cancel scan; rc=%d", rc);
        return;
    }

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }

    /* Save addr in dev_index */
    s_ble_dev[dev_index].addr = disc->addr;

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
     * timeout.
     */
    rc = ble_gap_connect(own_addr_type, &disc->addr, 30000, NULL,
                         app_ble_gap_event, (void *)dev_index);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to connect to device; addr_type=%d addr=%s; rc=%d",
                disc->addr.type, addr_str(disc->addr.val), rc);
        return;
    }
}

static int app_disc_chr_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
            const struct ble_gatt_chr *chr, void *arg)
{
    uint32_t dev_index = (uint32_t)arg;
    if (error && error->status == 0) {
        if (chr) {
            s_ble_dev[dev_index].chr = *chr;
            ESP_LOGD(TAG, "Characteristic value handle: %u", s_ble_dev[dev_index].chr.val_handle);
        }
    }
    if (error && error->status == BLE_HS_EDONE) {
        s_ble_dev[dev_index].add();
        ESP_LOGI(TAG, "Added BLE device %s", s_ble_dev[dev_index].adv_name);
    }
    return 0;
}

static int app_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
            const struct ble_gatt_svc *service, void *arg)
{
    uint32_t dev_index = (uint32_t)arg;

    if (error && error->status == 0) {
        if (service) {
            s_ble_dev[dev_index].svc = *service;
            ESP_LOGD(TAG, "Service start handle: %u end handle: %u", s_ble_dev[dev_index].svc.start_handle, s_ble_dev[dev_index].svc.end_handle);
        }
    }
    if (error && error->status == BLE_HS_EDONE) {
            ble_gattc_disc_chrs_by_uuid(conn_handle, s_ble_dev[dev_index].svc.start_handle,
                    s_ble_dev[dev_index].svc.end_handle, BLE_UUID16_DECLARE(s_ble_dev[dev_index].chr_uuid), app_disc_chr_cb, (void *)dev_index);
    }
    return 0;
}

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that is
 * established.  app_ble uses the same callback for all connections.
 *
 * @param event                 The event being signalled.
 * @param arg                   Application-specified argument; unused by
 *                                  app_ble.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int app_ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    char s[BLE_HS_ADV_MAX_SZ];
    int rc;
    uint32_t dev_index = (uint32_t)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        rc = ble_hs_adv_parse_fields(&fields, event->disc.data,
                                     event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        /* An advertisment report was received during GAP discovery. */
        if (fields.name != NULL) {
            assert(fields.name_len < sizeof s - 1);
            memcpy(s, fields.name, fields.name_len);
            s[fields.name_len] = '\0';
            ESP_LOGI(TAG, "Found a BLE device with %s name: %s",
                    fields.name_is_complete ? "complete" : "incomplete", s);
        }

        /* Try to connect to the advertiser if it looks interesting. */
        app_ble_connect_if_interesting(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        if (event->connect.status == 0) {
            /* Connection successfully established. */
            ESP_LOGI(TAG, "BLE connection established");
            s_ble_dev[dev_index].conn_handle = event->connect.conn_handle;

            ble_gattc_disc_svc_by_uuid(event->connect.conn_handle, BLE_UUID16_DECLARE(s_ble_dev[dev_index].svc_uuid),
                    app_disc_svc_cb, (void *)dev_index);
        } else {
            ESP_LOGI(TAG, "Failed to establish BLE connection; status=%d", event->connect.status);
        }

        app_ble_scan(false);

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection terminated. */
        ESP_LOGI(TAG, "BLE connection disconnected; reason=%d", event->disconnect.reason);
        s_ble_dev[dev_index].conn_handle = BLE_HS_CONN_HANDLE_NONE;
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "Discovery complete; reason=%d", event->disc_complete.reason);
        xSemaphoreGive( x_sem );
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        ESP_LOGI(TAG, "Encryption change event; status=%d", event->enc_change.status);
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
        /* Peer sent us a notification or indication. */
        ESP_LOGI(TAG, "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));

        /* Attribute data is contained in event->notify_rx.attr_data. */
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU update event; conn_handle=%d cid=%d mtu=%d",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;

    default:
        return 0;
    }
}

static int app_ble_chr_on_write(uint16_t conn_handle, const struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr, void *arg)
{
    ESP_LOGI(TAG, "Write complete; status=%d conn_handle=%d attr_handle=%d",
            error->status, conn_handle, attr->handle);
    return 0;
}

esp_err_t app_ble_update_dev(ble_dev_handle_t dev, uint8_t *data, int len)
{
    int rc = ESP_FAIL, i;
    uint16_t conn_handle, val_handle;
    uint32_t dev_index = 0xff;
    for (i = 0; i < MAX_DEV; i++) {
        if (dev == (void *)&s_ble_dev[i]) {
            dev_index = i;
            break;
        }
    }
    if (dev_index != 0xff) {
        conn_handle = s_ble_dev[i].conn_handle;
        val_handle = s_ble_dev[i].chr.val_handle;
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            rc = ble_gattc_write_flat(conn_handle, val_handle,
                    data, len, app_ble_chr_on_write, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to write characteristic; rc=%d", rc);
            }
        } else {
            /* ToDo: Reconnect */
        }
    }
    return rc;
}

static void app_ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void app_ble_on_sync(void)
{
    int rc;
    /* Make sure we have proper identity address set (public preferred) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Begin scanning for a peripheral to connect to. */
    app_ble_scan(true);
}

void app_ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void app_ble_start(void)
{
    int rc;

    x_sem = xSemaphoreCreateBinary();
    if (!x_sem) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
    nimble_port_init();
    /* Configure the host. */
    ble_hs_cfg.reset_cb = app_ble_on_reset;
    ble_hs_cfg.sync_cb = app_ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("nimble-central");
    assert(rc == 0);

    ble_store_config_init();

    nimble_port_freertos_init(app_ble_host_task);

    xSemaphoreTake( x_sem, portMAX_DELAY );
}
