// SPDX-License-Identifier: MIT
#include "ble_stubs.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include "../../components/companion_transport/ble_transport.cpp"

using ble_test::clock_ms;
using ble_test::notifications;
void require(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}
struct Fixture {
    NativeSerialInterface interface;
    explicit Fixture(bool secure = true) {
        interface.enable();
        require(companion_ble_start(interface, "MeshCore-test", 123456) == ESP_OK, "start failed");
        ble_test::description.sec_state.encrypted = secure;
        ble_test::description.sec_state.authenticated = secure;
        connect(11);
    }
    void connect(uint16_t handle) {
        ble_gap_event event{}; event.type = BLE_GAP_EVENT_CONNECT;
        event.connect.conn_handle = handle; gap_event(&event, nullptr);
        event = {}; event.type = BLE_GAP_EVENT_SUBSCRIBE;
        event.subscribe = {handle, 42, 1}; gap_event(&event, nullptr);
    }
    void disconnect() {
        ble_gap_event event{}; event.type = BLE_GAP_EVENT_DISCONNECT;
        event.disconnect.conn.conn_handle = connection; gap_event(&event, nullptr);
    }
    void enqueue(uint8_t code, uint8_t body) {
        const uint8_t bytes[] = {code, body};
        require(interface.writeFrame(bytes, sizeof(bytes)) == sizeof(bytes), "frame rejected");
    }
};

void burst_spacing() {
    Fixture f; f.enqueue(5, 0x11); f.enqueue(6, 0x22);
    clock_ms = 1000; pump_output(nullptr);
    clock_ms = 1010; pump_output(nullptr);
    require(ble_test::successful() == 1, "second notification sent before 60ms");
    clock_ms = 1059; pump_output(nullptr);
    require(ble_test::successful() == 1, "second notification sent at 59ms");
    clock_ms = 1060; pump_output(nullptr);
    require(ble_test::successful() == 2, "second notification missing at 60ms");
    require(notifications[0].bytes == std::vector<uint8_t>({5, 0x11}) &&
            notifications[1].bytes == std::vector<uint8_t>({6, 0x22}), "notification payload/order changed");
}
void device_info_spacing() {
    Fixture f; f.enqueue(13, 0x33); f.enqueue(5, 0x44);
    clock_ms = 1000; pump_output(nullptr);
    clock_ms = 1059; pump_output(nullptr);
    require(notifications.empty(), "DeviceInfo compatibility window removed");
    clock_ms = 1060; pump_output(nullptr);
    require(ble_test::successful() == 1, "DeviceInfo not sent at window end");
    clock_ms = 1070; pump_output(nullptr);
    require(ble_test::successful() == 1, "notification immediately after DeviceInfo");
    clock_ms = 1120; pump_output(nullptr);
    require(ble_test::successful() == 2, "notification blocked after 60ms");
}
void failed_notify_retry() {
    Fixture f; f.enqueue(5, 0x11); f.enqueue(6, 0x22);
    ble_test::notify_result = 12; clock_ms = 1000; pump_output(nullptr);
    require(ble_test::successful() == 0, "failed notify treated as success");
    ble_test::notify_result = 0; clock_ms = 1010; pump_output(nullptr);
    require(notifications.size() == 2 && notifications[1].bytes == std::vector<uint8_t>({5, 0x11}), "failed frame lost or reordered");
    clock_ms = 1020; pump_output(nullptr);
    require(ble_test::successful() == 1, "retry success did not start spacing window");
    clock_ms = 1070; pump_output(nullptr);
    require(ble_test::successful() == 2 && notifications.back().bytes == std::vector<uint8_t>({6, 0x22}), "second frame lost after retry");
}
void allocation_retry() {
    Fixture f; f.enqueue(5, 0x11);
    ble_test::fail_alloc = true; pump_output(nullptr);
    require(notifications.empty(), "sent without mbuf");
    ble_test::fail_alloc = false; clock_ms = 1010; pump_output(nullptr);
    require(notifications.size() == 1 && notifications[0].bytes == std::vector<uint8_t>({5, 0x11}), "allocation failure lost frame");
}
void wrap_spacing() {
    Fixture f; f.enqueue(5, 0x11); f.enqueue(6, 0x22);
    clock_ms = 0xffffffd0u; pump_output(nullptr);
    clock_ms = 0x0000000bu; pump_output(nullptr);
    require(ble_test::successful() == 1, "notification early across clock wrap");
    clock_ms = 0x0000000cu; pump_output(nullptr);
    require(ble_test::successful() == 2, "notification blocked across clock wrap");
}
void wrap_device_info() {
    Fixture f; f.enqueue(13, 0x11);
    clock_ms = 0xfffffff0u; pump_output(nullptr);
    clock_ms = 0x0000002bu; pump_output(nullptr);
    require(notifications.empty(), "DeviceInfo early across clock wrap");
    clock_ms = 0x0000002cu; pump_output(nullptr);
    require(ble_test::successful() == 1, "DeviceInfo blocked across clock wrap");
}
void clear_session(bool reset) {
    Fixture f; f.enqueue(13, 0x11); f.enqueue(5, 0x22); pump_output(nullptr);
    const uint8_t input[] = {22, 3}; require(f.interface.submit(input, sizeof(input)), "input rejected");
    if (reset) on_reset(3); else f.disconnect();
    f.connect(22); f.enqueue(7, 0x33); clock_ms = 2000; pump_output(nullptr);
    require(notifications.size() == 1 && notifications[0].handle == 22 &&
            notifications[0].bytes == std::vector<uint8_t>({7, 0x33}), "previous session TX frame leaked");
    uint8_t output[MAX_FRAME_SIZE];
    require(f.interface.checkRecvFrame(output) == 0, "previous session RX frame leaked");
}
void security_restored() {
    Fixture f;
    require(ble_test::security_requests == 0, "secure connection started repeated SMP");
    require(f.interface.isConnected(), "secure/subscribed connection not ready");
}
void readiness() {
    Fixture f(false);
    require(ble_test::security_requests == 1 && !f.interface.isConnected(), "unsecured connection ready");
    os_mbuf input{{22, 3}}; ble_gatt_access_ctxt context{BLE_GATT_ACCESS_OP_WRITE_CHR, &input};
    require(access(11, 0, &context, nullptr) == BLE_ATT_ERR_INSUFFICIENT_AUTHEN, "unsecured write accepted");
    ble_test::description.sec_state = {1, 1, 1, 16}; ble_test::mtu = 178;
    ble_gap_event event{}; event.type = BLE_GAP_EVENT_ENC_CHANGE; event.enc_change.conn_handle = 11;
    gap_event(&event, nullptr);
    require(!f.interface.isConnected(), "undersized MTU accepted");
    ble_test::mtu = 247; event.type = BLE_GAP_EVENT_MTU; event.mtu.conn_handle = 11;
    gap_event(&event, nullptr);
    require(access(11, 0, &context, nullptr) == 0, "ready write rejected");
    uint8_t output[MAX_FRAME_SIZE];
    require(f.interface.checkRecvFrame(output) == 2 && output[0] == 22 && output[1] == 3, "ready write payload changed");
    event.type = BLE_GAP_EVENT_SUBSCRIBE; event.subscribe = {11, 42, 0}; gap_event(&event, nullptr);
    require(!f.interface.isConnected(), "unsubscribed connection ready");
}
void stale_subscription() {
    Fixture f; f.enqueue(5, 0x11);
    ble_gap_event old{}; old.type = BLE_GAP_EVENT_SUBSCRIBE; old.subscribe = {10, 42, 0};
    gap_event(&old, nullptr); pump_output(nullptr);
    require(f.interface.isConnected() && notifications.size() == 1, "old connection subscription changed live session");
}
int main(int argc, char** argv) {
    try {
        require(argc == 2, "test name required"); const std::string test = argv[1];
        if (test == "burst") burst_spacing();
        else if (test == "device-info") device_info_spacing();
        else if (test == "notify-retry") failed_notify_retry();
        else if (test == "alloc-retry") allocation_retry();
        else if (test == "wrap") wrap_spacing();
        else if (test == "wrap-device-info") wrap_device_info();
        else if (test == "disconnect") clear_session(false);
        else if (test == "reset") clear_session(true);
        else if (test == "secure") security_restored();
        else if (test == "readiness") readiness();
        else if (test == "stale-subscription") stale_subscription();
        else throw std::runtime_error("unknown test");
        std::printf("PASS: %s\n", argv[1]); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
    }
}
