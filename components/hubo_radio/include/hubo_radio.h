// SPDX-License-Identifier: MIT
#pragma once
#include <Dispatcher.h>

struct RadioSettings {
    float frequency_mhz;
    float bandwidth_khz;
    uint8_t spreading_factor;
    uint8_t coding_rate;
    int8_t chip_power_dbm;
};

bool hubo_radio_validate(const RadioSettings& settings);
bool hubo_radio_init(const RadioSettings& settings);
mesh::Radio& hubo_radio();
