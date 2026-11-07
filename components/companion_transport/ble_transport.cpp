// SPDX-License-Identifier: MIT
#include "companion_transport.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
void ble_store_config_init(void);
}
#include "nvs_flash.h"
#include "esp_log.h"
#include <cstring>

namespace {
const ble_uuid128_t service_uuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x01,0x00,0x40,0x6e);
const ble_uuid128_t rx_uuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x02,0x00,0x40,0x6e);
const ble_uuid128_t tx_uuid = BLE_UUID128_INIT(
    0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,0x93,0xf3,0xa3,0xb5,0x03,0x00,0x40,0x6e);
NativeSerialInterface* transport;
uint16_t connection = BLE_HS_CONN_HANDLE_NONE, tx_handle;
uint8_t address_type;
uint32_t passkey;
bool subscribed = false;
ble_npl_callout pump;
uint8_t pending[MAX_FRAME_SIZE];
size_t pending_length = 0;
uint32_t pending_notify_at = 0;
uint32_t last_notify_ms = 0;
bool notified = false;
ble_gatt_chr_def characteristics[3]{};
ble_gatt_svc_def services[2]{};

void update_ready() {
    ble_gap_conn_desc description{};
    const bool known = connection != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_find(connection, &description) == 0;
    const bool ready = connection != BLE_HS_CONN_HANDLE_NONE && subscribed &&
        known &&
        description.sec_state.encrypted && description.sec_state.authenticated &&
        ble_att_mtu(connection) >= MAX_FRAME_SIZE + 3;
    transport->setConnected(ready);
    printf("BLE state: connected=%d encrypted=%d authenticated=%d subscribed=%d mtu=%u ready=%d\n",
        known, description.sec_state.encrypted, description.sec_state.authenticated, subscribed,
        known ? ble_att_mtu(connection) : 0, ready);
    if (!ready) {
        pending_length = 0;
        notified = false;
    }
}

int access(uint16_t handle, uint16_t, ble_gatt_access_ctxt* context, void*) {
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR) return 0;
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR || handle != connection) return BLE_ATT_ERR_UNLIKELY;
    const size_t size = OS_MBUF_PKTLEN(context->om);
    printf("BLE write: bytes=%u mtu=%u ready=%d\n", (unsigned)size, ble_att_mtu(handle), transport->isConnected());
    if (!size || size > MAX_FRAME_SIZE || size > ble_att_mtu(handle) - 3) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (!transport->isConnected()) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    uint8_t frame[MAX_FRAME_SIZE];
    if (os_mbuf_copydata(context->om, 0, size, frame) != 0) return BLE_ATT_ERR_UNLIKELY;
    printf("BLE rx: time_ms=%lu command=%u bytes=%u\n", (unsigned long)esp_log_timestamp(), frame[0], (unsigned)size);
    return transport->submit(frame, size) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

int gap_event(ble_gap_event* event, void*);
void advertise() {
    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    if (ble_gap_adv_set_fields(&fields) != 0) return;
    ble_hs_adv_fields scan{};
    scan.name = reinterpret_cast<const uint8_t*>(ble_svc_gap_device_name());
    scan.name_len = strlen(ble_svc_gap_device_name());
    scan.name_is_complete = 1;
    if (ble_gap_adv_rsp_set_fields(&scan) != 0) return;
    ble_gap_adv_params params{};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(address_type, nullptr, BLE_HS_FOREVER, &params, gap_event, nullptr);
}

int gap_event(ble_gap_event* event, void*) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        printf("BLE connect status=%d time_ms=%lu\n", event->connect.status, (unsigned long)esp_log_timestamp());
        if (event->connect.status == 0) {
            connection = event->connect.conn_handle;
            ble_gap_conn_desc description{};
            const bool known = ble_gap_conn_find(connection, &description) == 0;
            printf("BLE connection security: known=%d encrypted=%d authenticated=%d\n",
                known, description.sec_state.encrypted, description.sec_state.authenticated);
            if (known) {
                const auto& peer = description.peer_id_addr;
                printf("BLE peer: %02X:%02X:%02X:%02X:%02X:%02X type=%u\n",
                    peer.val[5], peer.val[4], peer.val[3], peer.val[2], peer.val[1], peer.val[0], peer.type);
            }
            // iOS may already have finished pairing/restoring before the connect callback; requesting security again would interfere with the first GATT write.
            if (known && description.sec_state.encrypted && description.sec_state.authenticated) {
                printf("BLE security restored; skip initiate\n");
            } else {
                const int rc = ble_gap_security_initiate(connection);
                printf("BLE security initiate rc=%d\n", rc);
            }
            update_ready();
        } else advertise();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        printf("BLE disconnect reason=%d\n", event->disconnect.reason);
        connection = BLE_HS_CONN_HANDLE_NONE;
        subscribed = false;
        update_ready();
        advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        // The same characteristic handle is reused across connections, so events from an old connection must not cancel the current subscription or clear the queue.
        if (event->subscribe.conn_handle != connection) break;
        if (event->subscribe.attr_handle == tx_handle) subscribed = event->subscribe.cur_notify;
        update_ready();
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        printf("BLE encryption status=%d\n", event->enc_change.status);
        update_ready();
        break;
    case BLE_GAP_EVENT_PARING_COMPLETE:
        printf("BLE pairing status=%d time_ms=%lu\n", event->pairing_complete.status, (unsigned long)esp_log_timestamp());
        break;
    case BLE_GAP_EVENT_NOTIFY_TX:
        printf("BLE notify: status=%d attr=%u time_ms=%lu\n", event->notify_tx.status,
            event->notify_tx.attr_handle, (unsigned long)esp_log_timestamp());
        break;
    case BLE_GAP_EVENT_MTU:
        update_ready();
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        ble_sm_io input{};
        input.action = event->passkey.params.action;
        printf("BLE passkey action=%u\n", input.action);
        if (input.action == BLE_SM_IOACT_DISP) {
            input.passkey = passkey;
            const int rc = ble_sm_inject_io(event->passkey.conn_handle, &input);
            printf("BLE passkey inject rc=%d\n", rc);
        }
        break;
    }
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        break;
    }
    return 0;
}

void pump_output(ble_npl_event*) {
    if (transport->isConnected()) {
        if (!pending_length) {
            transport->take(pending, sizeof(pending), &pending_length);
            // Compatibility window for the first iOS device-info packet; not the same as the upstream 60 ms spacing between all notification frames.
            pending_notify_at = esp_log_timestamp() + (pending_length && pending[0] == 13 ? 60 : 0);
        }
        // Upstream spaces all notifications 60 ms apart; the first-packet window is kept separately, and a failed retry does not drop the frame.
        const uint32_t now = esp_log_timestamp();
        if (pending_length && (!notified || uint32_t(now - last_notify_ms) >= 60) &&
            static_cast<int32_t>(now - pending_notify_at) >= 0) {
            auto* buffer = ble_hs_mbuf_from_flat(pending, pending_length);
            // NimBLE takes ownership of the mbuf on both success and failure; on failure the original frame is kept for the next retry.
            if (buffer) {
                const int rc = ble_gatts_notify_custom(connection, tx_handle, buffer);
                printf("BLE tx: time_ms=%lu response=%u bytes=%u rc=%d\n",
                    (unsigned long)esp_log_timestamp(), pending[0], (unsigned)pending_length, rc);
                if (rc == 0) {
                    pending_length = 0;
                    last_notify_ms = esp_log_timestamp();
                    notified = true;
                }
            }
        }
    }
    ble_npl_callout_reset(&pump, ble_npl_time_ms_to_ticks32(10));
}
void on_sync() {
    if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &address_type) != 0) return;
    int bond_count = 0;
    ble_store_util_count(BLE_STORE_OBJ_TYPE_PEER_SEC, &bond_count);
    printf("BLE diagnostic: protocol-v3 repeat_pairing=%d\n", MYNEWT_VAL(BLE_HANDLE_REPEAT_PAIRING_DELETION));
    printf("BLE restored peer bonds=%d\n", bond_count);
    advertise();
    ble_npl_callout_reset(&pump, ble_npl_time_ms_to_ticks32(10));
}
void on_reset(int reason) {
    printf("BLE host reset reason=%d\n", reason);
    connection = BLE_HS_CONN_HANDLE_NONE;
    subscribed = false;
    update_ready();
}
void host_task(void*) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}
}

esp_err_t companion_ble_start(NativeSerialInterface& interface, const char* name, uint32_t pin) {
    if (!name || strlen(name) > 29 || pin > 999999) return ESP_ERR_INVALID_ARG;
    esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) return result;
    result = nimble_port_init();
    if (result != ESP_OK) return result;
    transport = &interface;
    passkey = pin;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    characteristics[0].uuid = &rx_uuid.u;
    characteristics[0].access_cb = access;
    characteristics[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN;
    characteristics[1].uuid = &tx_uuid.u;
    characteristics[1].access_cb = access;
    characteristics[1].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC | BLE_GATT_CHR_F_READ_AUTHEN | BLE_GATT_CHR_F_NOTIFY;
    characteristics[1].val_handle = &tx_handle;
    services[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services[0].uuid = &service_uuid.u;
    services[0].characteristics = characteristics;
    if (ble_gatts_count_cfg(services) != 0 || ble_gatts_add_svcs(services) != 0 ||
        ble_svc_gap_device_name_set(name) != 0 || ble_att_set_preferred_mtu(247) != 0) return ESP_FAIL;
    ble_store_config_init();
    ble_npl_callout_init(&pump, nimble_port_get_dflt_eventq(), pump_output, nullptr);
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}
