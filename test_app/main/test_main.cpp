// SPDX-License-Identifier: MIT
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main() {
    // The SDK Unity menu busy-polls for input; let the idle task get CPU time so it can feed the watchdog.
    vTaskPrioritySet(nullptr, tskIDLE_PRIORITY);
    unity_run_menu();
}
