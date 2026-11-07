// SPDX-License-Identifier: MIT
#pragma once
#include "NativeSX1268.h"
#include "RadioLibWrappers.h"

class HuboSX1268Radio : public RadioLibWrapper {
    CustomSX1268& chip_;
    RadioSettings settings_{};
public:
    explicit HuboSX1268Radio(CustomSX1268& chip) : RadioLibWrapper(chip), chip_(chip) {}
    bool initialize(const RadioSettings& settings) {
        if (!chip_.native_init(settings)) return false;
        settings_ = settings;
        begin();
        return applySettings(settings);
    }
    bool applySettings(const RadioSettings& settings) {
        if (!hubo_radio_validate(settings)) return false;
        if (chip_.standby() != RADIOLIB_ERR_NONE ||
            chip_.setFrequency(settings.frequency_mhz) != RADIOLIB_ERR_NONE ||
            chip_.setSpreadingFactor(settings.spreading_factor) != RADIOLIB_ERR_NONE ||
            chip_.setBandwidth(settings.bandwidth_khz) != RADIOLIB_ERR_NONE ||
            chip_.setCodingRate(settings.coding_rate) != RADIOLIB_ERR_NONE ||
            chip_.setOutputPower(settings.chip_power_dbm) != RADIOLIB_ERR_NONE ||
            chip_.setPaRampTime(RADIOLIB_SX126X_PA_RAMP_3400U) != RADIOLIB_ERR_NONE ||
            chip_.setPreambleLength(preambleLengthForSF(settings.spreading_factor)) != RADIOLIB_ERR_NONE) return false;
        settings_ = settings;
        const auto timing = calcMaxPacketMillis(settings.spreading_factor,
            settings.bandwidth_khz, settings.coding_rate, preambleLengthForSF(settings.spreading_factor));
        chip_.setPreambleMillis(timing.preambleMillis);
        chip_.setMaxPayloadMillis(timing.payloadMillis);
        idle();
        return true;
    }
    void setParams(float frequency, float bandwidth, uint8_t sf, uint8_t cr) override {
        auto settings = settings_;
        settings.frequency_mhz = frequency;
        settings.bandwidth_khz = bandwidth;
        settings.spreading_factor = sf;
        settings.coding_rate = cr;
        applySettings(settings);
    }
    bool isReceivingPacket() override { return chip_.isReceiving(); }
    bool isSendComplete() override {
        // A stale RX/CAD notification must not be mistaken for TxDone.
        if (!(chip_.getIrqFlags() & RADIOLIB_SX126X_IRQ_TX_DONE)) return false;
        return RadioLibWrapper::isSendComplete();
    }
    float getCurrentRSSI() override { return chip_.getRSSI(false); }
    uint8_t getSpreadingFactor() const override { return settings_.spreading_factor; }
    float packetScore(float snr, int length) override {
        return packetScoreInt(snr, settings_.spreading_factor, length);
    }
    bool setRxBoostedGainMode(bool enabled) override {
        return chip_.setRxBoostedGainMode(enabled) == RADIOLIB_ERR_NONE;
    }
    bool getRxBoostedGainMode() const override { return chip_.getRxBoostedGainMode(); }
    bool getRxBoostedGainModeChecked(bool& enabled) const { return chip_.getRxBoostedGainModeChecked(enabled); }
};
