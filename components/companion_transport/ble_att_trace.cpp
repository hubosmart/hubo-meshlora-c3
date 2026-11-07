// SPDX-License-Identifier: MIT
// Enabled only in a temporary diagnostic configuration; observes the SDK through link wrapping without modifying its source or return values.
extern "C" {
#include "host/ble_hs.h"
#include "host/ble_store.h"
int __real_ble_att_svr_rx_write(uint16_t, uint16_t, struct os_mbuf**);
int __real_ble_hci_trans_hs_acl_tx(struct os_mbuf*);
int __real_ble_store_write_peer_sec(const struct ble_store_value_sec*);
}
#include "esp_log.h"
#include <cstdio>

extern "C" int __wrap_ble_att_svr_rx_write(uint16_t handle, uint16_t cid, struct os_mbuf** packet) {
    const uint32_t started = esp_log_timestamp();
    const int rc = __real_ble_att_svr_rx_write(handle, cid, packet);
    printf("BLE ATT write-return: time_ms=%lu handle=%u cid=%u rc=%d elapsed_ms=%lu\n",
        (unsigned long)esp_log_timestamp(), handle, cid, rc,
        (unsigned long)(esp_log_timestamp() - started));
    return rc;
}

extern "C" int __wrap_ble_hci_trans_hs_acl_tx(struct os_mbuf* packet) {
    // Reads only the HCI/L2CAP headers and the ATT opcode; the SMP channel and any payload are never printed.
    uint8_t header[9]{};
    const bool readable = OS_MBUF_PKTLEN(packet) >= sizeof(header) &&
        os_mbuf_copydata(packet, 0, sizeof(header), header) == 0;
    const uint16_t handle_flags = uint16_t(header[0]) | (uint16_t(header[1]) << 8);
    const uint16_t cid = uint16_t(header[6]) | (uint16_t(header[7]) << 8);
    const uint8_t boundary = (handle_flags >> 12) & 3;
    const bool trace = readable && boundary != 1 && cid == 4 &&
        (header[8] == 0x13 || header[8] == 0x01 || header[8] == 0x1b);
    // The mbuf must not be accessed after the SDK takes ownership; the header has already been copied to the stack.
    const int rc = __real_ble_hci_trans_hs_acl_tx(packet);
    if (trace) printf("BLE ATT controller-tx: time_ms=%lu handle=%u opcode=0x%02x rc=%d\n",
        (unsigned long)esp_log_timestamp(), handle_flags & 0x0fff, header[8], rc);
    return rc;
}

extern "C" int __wrap_ble_store_write_peer_sec(const struct ble_store_value_sec* value) {
    const int rc = __real_ble_store_write_peer_sec(value);
    printf("BLE bond-store: time_ms=%lu rc=%d\n", (unsigned long)esp_log_timestamp(), rc);
    return rc;
}
