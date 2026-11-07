// SPDX-License-Identifier: MIT
#pragma once
#include <helpers/BaseSerialInterface.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <mutex>

class NativeSerialInterface : public BaseSerialInterface {
public:
    static constexpr unsigned queue_depth = 8;
    NativeSerialInterface();
    void enable() override;
    void disable() override;
    bool isEnabled() const override;
    bool isConnected() const override;
    bool isWriteBusy() const override;
    size_t writeFrame(const uint8_t* bytes, size_t length) override;
    size_t checkRecvFrame(uint8_t* bytes) override;
    void setConnected(bool connected);
    bool submit(const uint8_t* bytes, size_t length);
    bool take(uint8_t* bytes, size_t capacity, size_t* length);
private:
    struct Frame { size_t length; uint8_t bytes[MAX_FRAME_SIZE]; };
    mutable std::mutex mutex_;
    bool enabled_ = false, connected_ = false;
    StaticQueue_t rx_control_{}, tx_control_{};
    uint8_t rx_storage_[queue_depth * sizeof(Frame)]{};
    uint8_t tx_storage_[queue_depth * sizeof(Frame)]{};
    QueueHandle_t rx_, tx_;
};
