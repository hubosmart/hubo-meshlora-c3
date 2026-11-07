// SPDX-License-Identifier: MIT
#pragma once

// Provides only the language/time interfaces needed by the reused core; it does not define ARDUINO or start the Arduino Core.
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <Stream.h>
#include <algorithm>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

inline unsigned long millis() {
    return static_cast<unsigned long>(esp_timer_get_time() / 1000);
}

inline void yield() { taskYIELD(); }

inline char* ltoa(long value, char* output, int base) {
    // int and long are both 32-bit in the ESP32 ABI, so the newlib conversion implementation is reused.
    static_assert(sizeof(long) == sizeof(int));
    return itoa(static_cast<int>(value), output, base);
}

using std::min;
using std::max;

inline void delay(unsigned long milliseconds) {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

template <typename T, typename L, typename H> constexpr T constrain(T value, L low, H high) {
    return value < low ? low : (value > high ? high : value);
}
