// SPDX-License-Identifier: MIT
#include "unity.h"
#include "hubo_radio.h"
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TEST_CASE("E22 configuration rejects unsupported band power and modulation", "[radio]") {
    RadioSettings settings{433.0f, 62.5f, 8, 5, 1};
    TEST_ASSERT_TRUE(hubo_radio_validate(settings));
    settings.frequency_mhz = 869.618f;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
    settings.frequency_mhz = NAN;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
    settings.frequency_mhz = 433.0f;
    settings.chip_power_dbm = 30;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
    settings.chip_power_dbm = 11;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
    settings.chip_power_dbm = 10;
    TEST_ASSERT_TRUE(hubo_radio_validate(settings));
    settings.chip_power_dbm = 1;
    settings.bandwidth_khz = 63.0f;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
    settings.bandwidth_khz = 62.5f;
    settings.spreading_factor = 13;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
    settings.spreading_factor = 8;
    settings.coding_rate = 4;
    TEST_ASSERT_FALSE(hubo_radio_validate(settings));
}

TEST_CASE("invalid E22 configuration fails before hardware initialization", "[radio]") {
    const RadioSettings unsupported{869.618f, 62.5f, 8, 5, 1};
    TEST_ASSERT_FALSE(hubo_radio_init(unsupported));
}

TEST_CASE("E22 hardware initializes and enters receive mode", "[radio_hw]") {
    const RadioSettings settings{433.0f, 62.5f, 8, 5, 1};
    TEST_ASSERT_TRUE_MESSAGE(hubo_radio_init(settings), "SX1268 initialization failed");
    auto& radio = hubo_radio();
    uint8_t packet[MAX_TRANS_UNIT];
    radio.recvRaw(packet, sizeof(packet));
    TEST_ASSERT_TRUE_MESSAGE(radio.isInRecvMode(), "SX1268 did not enter RX");
    TEST_ASSERT_GREATER_THAN(0, radio.getEstAirtimeFor(32));
    for (int i = 0; i < 100; ++i) {
        radio.recvRaw(packet, sizeof(packet));
        radio.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    TEST_ASSERT_TRUE(radio.isInRecvMode());
    printf("E22 RX ready: 433 MHz BW62.5 SF8 CR5; noise floor=%d dBm\n", radio.getNoiseFloor());
}

