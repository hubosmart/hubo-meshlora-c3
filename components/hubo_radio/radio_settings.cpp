// SPDX-License-Identifier: MIT
#include "hubo_radio.h"
#include <cmath>
#include <initializer_list>

bool hubo_radio_validate(const RadioSettings& settings) {
    if (!std::isfinite(settings.frequency_mhz) || settings.frequency_mhz < 410.0f ||
        settings.frequency_mhz > 493.0f || settings.spreading_factor < 5 ||
        settings.spreading_factor > 12 || settings.coding_rate < 5 ||
        settings.coding_rate > 8 || settings.chip_power_dbm < -9 ||
        settings.chip_power_dbm > 10) return false;   // same value as the companion's MAX_LORA_TX_POWER (chip setting; the E22 has an external PA)
    // Use the discrete bandwidths of RadioLib/SX1268; reject NAN and approximate arbitrary bandwidths.
    for (float bandwidth : {7.8f, 10.4f, 15.6f, 20.8f, 31.25f, 41.7f, 62.5f, 125.0f, 250.0f, 500.0f}) {
        if (settings.bandwidth_khz == bandwidth) return true;
    }
    return false;
}
