// SPDX-License-Identifier: MIT
#pragma once
#include <helpers/SensorManager.h>
#include "HuboNmea.h"

// GNSS variant: GPS_EN_N is active low and switches GNSS power; UART1 (GPIO20 RX / GPIO21 TX) receives NMEA at 9600 baud.
// Plugged into the companion as MeshCore's SensorManager: the custom variable "gps" (0/1) switches it; with a fix it updates the node lat/lon (used by advert and SELF_INFO),
// and adds GPS to telemetry. Off by default (prefs.gps_enabled = 0). "gps_interval" is only stored and does not duty-cycle the receiver (it runs continuously).
class HuboGnssSensorManager : public SensorManager {
public:
    bool begin() override;
    void loop() override;
    bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
    // gps and gps_interval are writable; gps_fix (0/1), gps_sats and gps_nmea (running count of NMEA sentences with a valid checksum) are read-only, so a host can confirm the UART works even indoors
    // After a fix, read-only gps_lat, gps_lon (degrees, 6 decimals) and gps_alt (whole meters) are also available; they are empty without a fix
    int getNumSettings() const override { return 8; }
    const char* getSettingName(int i) const override {
        static const char* const names[] = {"gps", "gps_interval", "gps_fix", "gps_sats", "gps_nmea", "gps_lat", "gps_lon", "gps_alt"};
        return i >= 0 && i < 8 ? names[i] : nullptr;
    }
    const char* getSettingValue(int i) const override {
        switch (i) {
            case 0: return enabled_ ? "1" : "0";
            case 1: return interval_;
            case 2: return fix_str_;
            case 3: return sats_str_;
            case 4: return nmea_str_;
            case 5: return lat_str_;
            case 6: return lon_str_;
            case 7: return alt_str_;
            default: return nullptr;
        }
    }
    bool setSettingValue(const char* name, const char* value) override;

    bool hasFix() const { return nmea_.fix().valid; }
    const HuboNmeaFix& fix() const { return nmea_.fix(); }

private:
    void powerOn();
    void powerOff();
    void refreshStatus();   // write the parse result into the read-only variable strings

    HuboNmea nmea_;
    bool enabled_ = false;
    char interval_[12] = "0";
    char fix_str_[2] = "0";
    char sats_str_[4] = "0";
    char nmea_str_[12] = "0";
    char lat_str_[14] = "";
    char lon_str_[14] = "";
    char alt_str_[9] = "";
};
