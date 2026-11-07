// SPDX-License-Identifier: MIT
#include "unity.h"
#include "NativeSerialInterface.h"
#include "MyMesh.h"
#include "meshcore_companion.h"

namespace {
fs::FS store_fs("/testmesh");
DataStore data_store(store_fs, hubo_rtc());
SimpleMeshTables tables;
MyMesh companion_mesh(hubo_radio(), hubo_rng(), hubo_rtc(), tables, data_store);
NativeSerialInterface command_transport;

size_t exchange(const uint8_t* request, size_t length, uint8_t* response) {
    companion_mesh.startInterface(command_transport);
    command_transport.setConnected(true);
    TEST_ASSERT_TRUE(command_transport.submit(request, length));
    companion_mesh.checkSerialInterface();
    size_t response_length = 0;
    TEST_ASSERT_TRUE(command_transport.take(response, MAX_FRAME_SIZE, &response_length));
    return response_length;
}
}

TEST_CASE("upstream companion device query retains protocol version 13", "[companion]") {
    const uint8_t request[] = {22, 3};
    uint8_t response[MAX_FRAME_SIZE];
    size_t length = exchange(request, sizeof(request), response);
    mc_event_t event{};
    TEST_ASSERT_TRUE(mc_parse(response, length, &event));
    TEST_ASSERT_EQUAL(MC_RESP_DEVICE_INFO, event.code);
    TEST_ASSERT_EQUAL(13, event.u.device_info.fw_ver);
    TEST_ASSERT_EQUAL_STRING("Hubo C3 E22-400M30S", event.u.device_info.model);
    const uint8_t short_start[] = {1};
    length = exchange(short_start, sizeof(short_start), response);
    TEST_ASSERT_EQUAL(2, length);
    TEST_ASSERT_EQUAL(1, response[0]);
}

TEST_CASE("companion rejects radio settings outside the E22 band", "[companion]") {
    uint8_t request[11] = {11};
    const uint32_t frequency = 869618, bandwidth = 62500;
    memcpy(request + 1, &frequency, 4);
    memcpy(request + 5, &bandwidth, 4);
    request[9] = 8; request[10] = 5;
    uint8_t response[MAX_FRAME_SIZE];
    const size_t length = exchange(request, sizeof(request), response);
    TEST_ASSERT_EQUAL(2, length);
    TEST_ASSERT_EQUAL(1, response[0]);
    TEST_ASSERT_EQUAL(6, response[1]);
    TEST_ASSERT_EQUAL_FLOAT(433.0f, companion_mesh.getNodePrefs()->freq);
}

TEST_CASE("companion queue preserves frame boundaries and rejects overflow", "[companion]") {
    static NativeSerialInterface transport;
    transport.enable();
    transport.setConnected(true);
    const uint8_t request[] = {22, 3};
    uint8_t frame[177] = {};
    TEST_ASSERT_FALSE(transport.submit(frame, 177));
    TEST_ASSERT_FALSE(transport.submit(frame, 0));
    TEST_ASSERT_TRUE(transport.submit(request, sizeof(request)));
    TEST_ASSERT_EQUAL(2, transport.checkRecvFrame(frame));
    TEST_ASSERT_EQUAL_MEMORY(request, frame, 2);
    TEST_ASSERT_EQUAL(0, transport.checkRecvFrame(frame));
    for (unsigned i = 0; i < NativeSerialInterface::queue_depth; ++i) {
        frame[0] = i;
        TEST_ASSERT_EQUAL(1, transport.writeFrame(frame, 1));
    }
    TEST_ASSERT_TRUE(transport.isWriteBusy());
    TEST_ASSERT_EQUAL(0, transport.writeFrame(request, sizeof(request)));
    for (unsigned i = 0; i < NativeSerialInterface::queue_depth; ++i) {
        size_t size = 99;
        TEST_ASSERT_TRUE(transport.take(frame, sizeof(frame), &size));
        TEST_ASSERT_EQUAL(1, size);
        TEST_ASSERT_EQUAL(i, frame[0]);
    }
    transport.setConnected(false);
    TEST_ASSERT_FALSE(transport.isConnected());
    TEST_ASSERT_FALSE(transport.submit(request, sizeof(request)));
}
