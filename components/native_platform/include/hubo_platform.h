// SPDX-License-Identifier: MIT
#pragma once

#include <MeshCore.h>
#include <Utils.h>

mesh::RTCClock& hubo_rtc();
mesh::RNG& hubo_rng();

// Enables hardware entropy while BLE/Wi-Fi are not yet running; this phase must end before the radio controller is enabled.
void hubo_boot_entropy_enable();
void hubo_boot_entropy_disable();
