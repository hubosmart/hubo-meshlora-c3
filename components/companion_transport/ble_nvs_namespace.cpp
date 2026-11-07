// SPDX-License-Identifier: MIT
#include "nvs.h"
#include <cstring>

extern "C" esp_err_t __real_nvs_open(const char*, nvs_open_mode_t, nvs_handle_t*);

extern "C" esp_err_t __wrap_nvs_open(const char* name, nvs_open_mode_t mode, nvs_handle_t* handle) {
    // The SDK hardcodes nimble_bond, and the 80-byte records of older firmware are incompatible with this SDK's 88-byte layout.
    // Only the BLE namespace is isolated; old records are kept, and NVS is not erased nor the global SDK modified.
    const char* selected = strcmp(name, "nimble_bond") == 0 ? "hubo_ble_55" : name;
    return __real_nvs_open(selected, mode, handle);
}
