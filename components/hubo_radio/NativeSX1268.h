// SPDX-License-Identifier: MIT
#pragma once

#include <RadioLib.h>
#include "hubo_radio.h"
#include <Arduino.h>
#include "esp_log.h"

// The version string of a real E22-400M30S may map to the SX1262-compatible profile.
// It is selected explicitly by board configuration and does not skip version detection or relax the band/power checks.
#include "sdkconfig.h"
#if CONFIG_HUBO_E22_SX1262_COMPAT
using E22Chip = SX1262;
#else
using E22Chip = SX1268;
#endif

extern int16_t g_e22_init_error;   // result of the latest native_init: 0 on success, otherwise a RadioLib error code (defined in hubo_radio.cpp, read by the bring-up probe)

class CustomSX1268 : public E22Chip {
  uint32_t _preambleMillis = 66;
  uint32_t _maxPayloadMillis = 3934;
  uint32_t _activityAt = 0;
  bool _headerSeen = false;

  public:
    CustomSX1268(Module *mod) : E22Chip(mod) { }

    bool native_init(const RadioSettings& settings) {
      if (!hubo_radio_validate(settings)) { g_e22_init_error = -1000; return false; }
      int16_t result = begin(settings.frequency_mhz, settings.bandwidth_khz,
          settings.spreading_factor, settings.coding_rate,
          RADIOLIB_SX126X_SYNC_WORD_PRIVATE, settings.chip_power_dbm, 16, 2.2f);
      g_e22_init_error = result;
      if (result != RADIOLIB_ERR_NONE) {
        ESP_LOGE("E22", "SX1268 begin failed: %d", result);
        return false;
      }
      setRfSwitchPins(CONFIG_HUBO_E22_RXEN_GPIO, RADIOLIB_NC);
      return setCRC(1) == RADIOLIB_ERR_NONE &&
             setDio2AsRfSwitch(true) == RADIOLIB_ERR_NONE &&
             setRxBoostedGainMode(true) == RADIOLIB_ERR_NONE;
    }

    int16_t startReceive() override {
      // include the PREAMBLE_DETECTED irq bit in reported flags
      return E22Chip::startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, RADIOLIB_IRQ_RX_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED), RADIOLIB_IRQ_RX_DEFAULT_MASK, 0);
    }

    bool isReceiving() {
      uint32_t irq = getIrqFlags();
      bool preamble = irq & RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED; // bit 2
      bool header   = irq & RADIOLIB_SX126X_IRQ_HEADER_VALID;      // bit 4
      bool hdrErr   = irq & RADIOLIB_SX126X_IRQ_HEADER_ERR;        // bit 5
      uint32_t now  = millis();
      if (hdrErr) {
        clearIrqFlags(RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED | RADIOLIB_SX126X_IRQ_HEADER_VALID | RADIOLIB_SX126X_IRQ_HEADER_ERR | RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID);
        _activityAt = 0;
        _headerSeen = false;
        return false;
      }
      if (!header && _headerSeen) {
        // something cleared the header flag, reset our state.
        _activityAt = 0; _headerSeen = false;
        return false;
      }

      if (header) {
        if (!_headerSeen) { _headerSeen = true; _activityAt = now; };
        if (now - _activityAt > _maxPayloadMillis) {
          MESH_DEBUG_PRINTLN("Clearing header IRQ after %ums", _maxPayloadMillis);
          clearIrqFlags(RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED | RADIOLIB_SX126X_IRQ_HEADER_VALID | RADIOLIB_SX126X_IRQ_HEADER_ERR | RADIOLIB_SX126X_IRQ_SYNC_WORD_VALID);
          _activityAt = 0; _headerSeen = false;
          return false;
        }
        return true;
      }
      if (preamble) {
        if (_activityAt == 0) _activityAt = now;
        if (now - _activityAt > _preambleMillis) {
          clearIrqFlags(RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
          _activityAt = 0;
          MESH_DEBUG_PRINTLN("Clearing preamble IRQ after %ums", _preambleMillis);

          return false;
        }
        return true;
      }
      _activityAt = 0; _headerSeen = false;
      return false;
    }
    
    void setPreambleMillis(uint32_t preambleMillis) {
      _preambleMillis = preambleMillis;
      MESH_DEBUG_PRINTLN("Set _preambleMillis=%u", _preambleMillis);
    }
    void setMaxPayloadMillis(uint32_t payloadMillis) {
      _maxPayloadMillis = payloadMillis;
      MESH_DEBUG_PRINTLN("Set _maxPayloadMillis=%u", _maxPayloadMillis);
    }


    bool getRxBoostedGainModeChecked(bool& enabled) {
      uint8_t rxGain = 0;
      if (readRegister(RADIOLIB_SX126X_REG_RX_GAIN, &rxGain, 1) != RADIOLIB_ERR_NONE ||
          (rxGain != RADIOLIB_SX126X_RX_GAIN_BOOSTED && rxGain != RADIOLIB_SX126X_RX_GAIN_POWER_SAVING)) return false;
      enabled = rxGain == RADIOLIB_SX126X_RX_GAIN_BOOSTED;
      return true;
    }
    bool getRxBoostedGainMode() { bool enabled = false; return getRxBoostedGainModeChecked(enabled) && enabled; }
};
