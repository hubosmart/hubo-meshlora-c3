// SPDX-License-Identifier: MIT
#pragma once
#include <MeshCore.h>
#include <helpers/SensorManager.h>
#include "HuboSX1268Radio.h"
#include "hubo_platform.h"
#include "esp_system.h"
#include "sdkconfig.h"

class NativeMillis : public mesh::MillisecondClock {
public:
    unsigned long getMillis() override { return millis(); }
};
class HuboBoard : public mesh::MainBoard {
public:
    // This board has no battery ADC; keep the protocol convention that 0 means no measurement provided.
    uint16_t getBattMilliVolts() override { return 0; }
    const char* getManufacturerName() const override { return "Hubo C3 E22-400M30S"; }
    void reboot() override { esp_restart(); }
    uint8_t getStartupReason() const override { return BD_STARTUP_NORMAL; }
};
extern HuboBoard board;
#if CONFIG_HUBO_GPS_EN_N_GPIO >= 0
#include "HuboGnss.h"
extern HuboGnssSensorManager sensors;   // GNSS variant
#else
extern SensorManager sensors;
#endif
extern HuboSX1268Radio& radio_driver;
inline mesh::LocalIdentity radio_new_identity() { return mesh::LocalIdentity(&hubo_rng()); }
