// SPDX-License-Identifier: MIT
#pragma once
// Replaces only the SDK boundary that cannot run on the host; the production transport and queue logic take part in the tests directly.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

struct StaticQueue_t {
    size_t capacity = 0, width = 0;
    std::deque<std::vector<uint8_t>> frames;
};
using QueueHandle_t = StaticQueue_t*;
constexpr int pdTRUE = 1;
inline QueueHandle_t xQueueCreateStatic(size_t count, size_t width, uint8_t*, StaticQueue_t* queue) {
    queue->capacity = count; queue->width = width; return queue;
}
inline int xQueueReset(QueueHandle_t queue) { queue->frames.clear(); return pdTRUE; }
inline size_t uxQueueSpacesAvailable(QueueHandle_t queue) { return queue->capacity - queue->frames.size(); }
inline int xQueueSend(QueueHandle_t queue, const void* frame, unsigned) {
    if (!uxQueueSpacesAvailable(queue)) return 0;
    auto* bytes = static_cast<const uint8_t*>(frame);
    queue->frames.emplace_back(bytes, bytes + queue->width); return pdTRUE;
}
inline int xQueuePeek(QueueHandle_t queue, void* frame, unsigned) {
    if (queue->frames.empty()) return 0;
    std::memcpy(frame, queue->frames.front().data(), queue->width); return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue, void* frame, unsigned wait) {
    if (!xQueuePeek(queue, frame, wait)) return 0;
    queue->frames.pop_front(); return pdTRUE;
}

extern "C" {
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 0x102;
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE = 0xffff;
enum {
    BLE_GAP_EVENT_CONNECT = 1, BLE_GAP_EVENT_DISCONNECT, BLE_GAP_EVENT_SUBSCRIBE,
    BLE_GAP_EVENT_ENC_CHANGE, BLE_GAP_EVENT_PARING_COMPLETE, BLE_GAP_EVENT_NOTIFY_TX,
    BLE_GAP_EVENT_MTU, BLE_GAP_EVENT_PASSKEY_ACTION, BLE_GAP_EVENT_ADV_COMPLETE,
    BLE_GATT_ACCESS_OP_READ_CHR = 0, BLE_GATT_ACCESS_OP_WRITE_CHR = 1,
    BLE_ATT_ERR_UNLIKELY = 14, BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN = 13,
    BLE_ATT_ERR_INSUFFICIENT_AUTHEN = 5, BLE_ATT_ERR_INSUFFICIENT_RES = 17,
    BLE_HS_ADV_F_DISC_GEN = 2, BLE_HS_ADV_F_BREDR_UNSUP = 4,
    BLE_GAP_CONN_MODE_UND = 2, BLE_GAP_DISC_MODE_GEN = 2, BLE_HS_FOREVER = -1,
    BLE_SM_IOACT_DISP = 3, BLE_STORE_OBJ_TYPE_PEER_SEC = 2,
    BLE_HS_IO_DISPLAY_ONLY = 0, BLE_SM_PAIR_KEY_DIST_ENC = 1, BLE_SM_PAIR_KEY_DIST_ID = 2,
    BLE_GATT_CHR_F_WRITE = 8, BLE_GATT_CHR_F_WRITE_ENC = 0x100,
    BLE_GATT_CHR_F_WRITE_AUTHEN = 0x200, BLE_GATT_CHR_F_READ = 2,
    BLE_GATT_CHR_F_READ_ENC = 0x20, BLE_GATT_CHR_F_READ_AUTHEN = 0x40,
    BLE_GATT_CHR_F_NOTIFY = 16, BLE_GATT_SVC_TYPE_PRIMARY = 1,
};
#define MYNEWT_VAL(option) 1
struct ble_uuid_t { uint8_t type; };
struct ble_uuid128_t { ble_uuid_t u; uint8_t value[16]; };
#define BLE_UUID128_INIT(...) {{128}, {__VA_ARGS__}}
struct ble_addr_t { uint8_t type; uint8_t val[6]; };
struct ble_gap_conn_desc {
    struct { uint8_t encrypted, authenticated, bonded, key_size; } sec_state;
    ble_addr_t peer_id_addr;
    uint16_t conn_handle;
};
struct ble_gap_event {
    uint8_t type;
    struct { int status; uint16_t conn_handle; } connect;
    struct { int reason; ble_gap_conn_desc conn; } disconnect;
    struct { uint16_t conn_handle, attr_handle; uint8_t cur_notify; } subscribe;
    struct { int status; uint16_t conn_handle; } enc_change;
    struct { int status; uint16_t conn_handle; } pairing_complete;
    struct { int status; uint16_t conn_handle, attr_handle; } notify_tx;
    struct { uint16_t conn_handle, value; } mtu;
    struct { uint16_t conn_handle; struct { uint8_t action; } params; } passkey;
};
struct os_mbuf { std::vector<uint8_t> bytes; };
#define OS_MBUF_PKTLEN(buffer) ((buffer)->bytes.size())
struct ble_gatt_access_ctxt { uint8_t op; os_mbuf* om; };
struct ble_hs_adv_fields {
    uint8_t flags;
    const ble_uuid128_t* uuids128;
    uint8_t num_uuids128, uuids128_is_complete;
    const uint8_t* name;
    uint8_t name_len, name_is_complete;
};
struct ble_gap_adv_params { uint8_t conn_mode, disc_mode; };
struct ble_sm_io { uint8_t action; uint32_t passkey; };
struct ble_npl_event {};
struct ble_npl_callout { void (*callback)(ble_npl_event*); };
struct ble_gatt_chr_def {
    const ble_uuid_t* uuid;
    int (*access_cb)(uint16_t, uint16_t, ble_gatt_access_ctxt*, void*);
    uint16_t flags;
    uint16_t* val_handle;
};
struct ble_gatt_svc_def { uint8_t type; const ble_uuid_t* uuid; ble_gatt_chr_def* characteristics; };
struct ble_hs_cfg_t {
    void (*sync_cb)();
    void (*reset_cb)(int);
    void (*store_status_cb)();
    uint8_t sm_io_cap, sm_bonding, sm_mitm, sm_sc, sm_our_key_dist, sm_their_key_dist;
};
inline ble_hs_cfg_t ble_hs_cfg{};
}

namespace ble_test {
inline uint32_t clock_ms = 1000;
inline uint16_t mtu = 247;
inline ble_gap_conn_desc description{};
inline int security_requests = 0, notify_result = 0;
inline bool fail_alloc = false;
struct Notification { uint32_t time; uint16_t handle; std::vector<uint8_t> bytes; int status; };
inline std::vector<Notification> notifications;
inline size_t successful() {
    size_t count = 0;
    for (const auto& item : notifications) count += item.status == 0;
    return count;
}
}

extern "C" {
inline uint32_t esp_log_timestamp() { return ble_test::clock_ms; }
inline esp_err_t nvs_flash_init() { return ESP_OK; }
inline esp_err_t nimble_port_init() { return ESP_OK; }
inline void nimble_port_run() {}
inline void nimble_port_freertos_deinit() {}
inline void nimble_port_freertos_init(void (*)(void*)) {}
inline void* nimble_port_get_dflt_eventq() { return nullptr; }
inline int ble_gap_conn_find(uint16_t handle, ble_gap_conn_desc* out) {
    if (handle == BLE_HS_CONN_HANDLE_NONE) return 1;
    *out = ble_test::description; out->conn_handle = handle; return 0;
}
inline uint16_t ble_att_mtu(uint16_t) { return ble_test::mtu; }
inline int os_mbuf_copydata(os_mbuf* buffer, int offset, size_t size, void* out) {
    if (size_t(offset) + size > buffer->bytes.size()) return 1;
    std::memcpy(out, buffer->bytes.data() + offset, size); return 0;
}
inline os_mbuf* ble_hs_mbuf_from_flat(const void* bytes, size_t size) {
    if (ble_test::fail_alloc) return nullptr;
    const auto* start = static_cast<const uint8_t*>(bytes);
    return new os_mbuf{{start, start + size}};
}
inline int ble_gatts_notify_custom(uint16_t handle, uint16_t, os_mbuf* buffer) {
    ble_test::notifications.push_back({ble_test::clock_ms, handle, buffer->bytes, ble_test::notify_result});
    delete buffer; return ble_test::notify_result;
}
inline int ble_gap_security_initiate(uint16_t) { ++ble_test::security_requests; return 0; }
inline int ble_gap_adv_set_fields(const ble_hs_adv_fields*) { return 0; }
inline int ble_gap_adv_rsp_set_fields(const ble_hs_adv_fields*) { return 0; }
inline int ble_gap_adv_start(uint8_t, void*, int32_t, const ble_gap_adv_params*,
                             int (*)(ble_gap_event*, void*), void*) { return 0; }
inline int ble_sm_inject_io(uint16_t, const ble_sm_io*) { return 0; }
inline int ble_npl_time_ms_to_ticks32(int ms) { return ms; }
inline void ble_npl_callout_reset(ble_npl_callout*, int) {}
inline void ble_npl_callout_init(ble_npl_callout* timer, void*, void (*callback)(ble_npl_event*), void*) {
    timer->callback = callback;
}
inline int ble_hs_util_ensure_addr(int) { return 0; }
inline int ble_hs_id_infer_auto(int, uint8_t* type) { *type = 0; return 0; }
inline int ble_store_util_count(int, int* count) { *count = 1; return 0; }
inline void ble_store_util_status_rr() {}
inline void ble_svc_gap_init() {}
inline void ble_svc_gatt_init() {}
inline const char* ble_svc_gap_device_name() { return "MeshCore-test"; }
inline int ble_svc_gap_device_name_set(const char*) { return 0; }
inline int ble_att_set_preferred_mtu(uint16_t) { return 0; }
inline int ble_gatts_count_cfg(ble_gatt_svc_def*) { return 0; }
inline int ble_gatts_add_svcs(ble_gatt_svc_def* services) {
    *services[0].characteristics[1].val_handle = 42; return 0;
}
inline void ble_store_config_init() {}
}
