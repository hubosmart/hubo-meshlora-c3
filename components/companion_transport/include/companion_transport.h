// SPDX-License-Identifier: MIT
#pragma once
#include "NativeSerialInterface.h"
#include "esp_err.h"

esp_err_t companion_usb_start(NativeSerialInterface& transport);
esp_err_t companion_ble_start(NativeSerialInterface& transport, const char* name, uint32_t pin);
