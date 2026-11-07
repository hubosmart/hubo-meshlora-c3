// SPDX-License-Identifier: MIT
#include "sdkconfig.h"
#if CONFIG_HUBO_GPS_EN_N_GPIO >= 0
#include "HuboGnss.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"

namespace {
constexpr gpio_num_t kGpsEnN = static_cast<gpio_num_t>(CONFIG_HUBO_GPS_EN_N_GPIO);
constexpr uart_port_t kUart = UART_NUM_1;
constexpr gpio_num_t kPinRx = GPIO_NUM_20;   // GNSS TXD -> C3
constexpr gpio_num_t kPinTx = GPIO_NUM_21;   // C3 -> GNSS RXD (through a series resistor)
}  // namespace

bool HuboGnssSensorManager::begin() {
    powerOff();   // always power off at boot (GPIO2 is also a strapping pin and must be high)
    return true;
}

void HuboGnssSensorManager::powerOn() {
    gpio_set_direction(kGpsEnN, GPIO_MODE_OUTPUT);
    gpio_set_level(kGpsEnN, 0);
    uart_config_t cfg = {};
    cfg.baud_rate = 9600;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.source_clk = UART_SCLK_DEFAULT;
    if (uart_driver_install(kUart, 512, 0, 0, nullptr, 0) == ESP_OK) {
        uart_param_config(kUart, &cfg);
        uart_set_pin(kUart, kPinTx, kPinRx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    nmea_.reset();
    refreshStatus();
}

namespace {
// degrees -> "[-]ddd.dddddd" (avoids floating-point printf)
void formatDegrees(double deg, char* out, size_t cap) {
    long long micro = llround(deg * 1e6);
    const bool neg = micro < 0;
    if (neg) micro = -micro;
    snprintf(out, cap, "%s%lld.%06lld", neg ? "-" : "", micro / 1000000, micro % 1000000);
}
}  // namespace

void HuboGnssSensorManager::refreshStatus() {
    const HuboNmeaFix& f = nmea_.fix();
    fix_str_[0] = f.valid ? '1' : '0';
    fix_str_[1] = 0;
    snprintf(sats_str_, sizeof sats_str_, "%u", static_cast<unsigned>(f.sats));
    snprintf(nmea_str_, sizeof nmea_str_, "%lu", static_cast<unsigned long>(f.sentences_ok % 1000000000UL));
    if (f.valid) {
        formatDegrees(f.lat, lat_str_, sizeof lat_str_);
        formatDegrees(f.lon, lon_str_, sizeof lon_str_);
        snprintf(alt_str_, sizeof alt_str_, "%ld", lround(f.alt_m));
    } else {
        lat_str_[0] = lon_str_[0] = alt_str_[0] = 0;   // no fix: all three are empty
    }
}

void HuboGnssSensorManager::powerOff() {
    if (uart_is_driver_installed(kUart)) uart_driver_delete(kUart);
    // float GPIO21 while powered off so current cannot back-feed through the GNSS RXD protection diode
    gpio_reset_pin(kPinTx);
    gpio_set_direction(kPinTx, GPIO_MODE_INPUT);
    gpio_set_direction(kGpsEnN, GPIO_MODE_OUTPUT);
    gpio_set_level(kGpsEnN, 1);
    nmea_.reset();
    refreshStatus();   // clear status after power-off
}

bool HuboGnssSensorManager::setSettingValue(const char* name, const char* value) {
    if (!name || !value) return false;
    if (!strcmp(name, "gps")) {
        const bool on = value[0] == '1';
        if (on == enabled_) return true;
        enabled_ = on;
        if (on) powerOn(); else powerOff();
        return true;
    }
    if (!strcmp(name, "gps_interval")) {
        snprintf(interval_, sizeof interval_, "%lu", strtoul(value, nullptr, 10));
        return true;
    }
    return false;
}

void HuboGnssSensorManager::loop() {
    if (!enabled_) return;
    uint8_t buf[64];
    const int n = uart_read_bytes(kUart, buf, sizeof buf, 0);
    for (int i = 0; i < n; ++i) nmea_.feed(static_cast<char>(buf[i]));
    if (n > 0) refreshStatus();
    if (n > 0 && nmea_.fix().valid) {
        node_lat = nmea_.fix().lat;
        node_lon = nmea_.fix().lon;
        node_altitude = nmea_.fix().alt_m;
    }
}

bool HuboGnssSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
    if (!enabled_ || !nmea_.fix().valid || !(requester_permissions & TELEM_PERM_LOCATION)) return false;
    telemetry.addGPS(TELEM_CHANNEL_SELF, static_cast<float>(nmea_.fix().lat), static_cast<float>(nmea_.fix().lon),
                     static_cast<float>(nmea_.fix().alt_m));
    return true;
}
#endif
