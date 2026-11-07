// SPDX-License-Identifier: MIT
#include "NativeSerialInterface.h"
#include <cstring>

NativeSerialInterface::NativeSerialInterface() {
    rx_ = xQueueCreateStatic(queue_depth, sizeof(Frame), rx_storage_, &rx_control_);
    tx_ = xQueueCreateStatic(queue_depth, sizeof(Frame), tx_storage_, &tx_control_);
}
void NativeSerialInterface::enable() {
    std::lock_guard lock(mutex_);
    enabled_ = true;
}
void NativeSerialInterface::disable() {
    std::lock_guard lock(mutex_);
    enabled_ = connected_ = false;
    xQueueReset(rx_); xQueueReset(tx_);
}
bool NativeSerialInterface::isEnabled() const {
    std::lock_guard lock(mutex_);
    return enabled_;
}
bool NativeSerialInterface::isConnected() const {
    std::lock_guard lock(mutex_);
    return enabled_ && connected_;
}
void NativeSerialInterface::setConnected(bool connected) {
    std::lock_guard lock(mutex_);
    if (connected_ != connected) {
        // Clear transport frames of the old connection; the MyMesh offline message queue is not handled here.
        xQueueReset(rx_); xQueueReset(tx_);
    }
    connected_ = connected;
}
bool NativeSerialInterface::isWriteBusy() const {
    std::lock_guard lock(mutex_);
    return !enabled_ || !connected_ || uxQueueSpacesAvailable(tx_) == 0;
}
bool NativeSerialInterface::submit(const uint8_t* bytes, size_t length) {
    if (!bytes || !length || length > MAX_FRAME_SIZE) return false;
    std::lock_guard lock(mutex_);
    if (!enabled_ || !connected_) return false;
    Frame frame{};
    frame.length = length;
    memcpy(frame.bytes, bytes, length);
    return xQueueSend(rx_, &frame, 0) == pdTRUE;
}
size_t NativeSerialInterface::checkRecvFrame(uint8_t* bytes) {
    std::lock_guard lock(mutex_);
    Frame frame;
    if (!bytes || !enabled_ || !connected_ || xQueueReceive(rx_, &frame, 0) != pdTRUE) return 0;
    memcpy(bytes, frame.bytes, frame.length);
    return frame.length;
}
size_t NativeSerialInterface::writeFrame(const uint8_t* bytes, size_t length) {
    if (!bytes || !length || length > MAX_FRAME_SIZE) return 0;
    std::lock_guard lock(mutex_);
    if (!enabled_ || !connected_) return 0;
    Frame frame{};
    frame.length = length;
    memcpy(frame.bytes, bytes, length);
    return xQueueSend(tx_, &frame, 0) == pdTRUE ? length : 0;
}
bool NativeSerialInterface::take(uint8_t* bytes, size_t capacity, size_t* length) {
    if (!bytes || !length) return false;
    std::lock_guard lock(mutex_);
    Frame frame;
    if (!enabled_ || !connected_ || xQueuePeek(tx_, &frame, 0) != pdTRUE || capacity < frame.length) return false;
    xQueueReceive(tx_, &frame, 0);
    memcpy(bytes, frame.bytes, frame.length);
    *length = frame.length;
    return true;
}
