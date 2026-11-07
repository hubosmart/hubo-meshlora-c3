// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>

// Keeps the upstream serial format: < / > markers with a 16-bit little-endian length; an oversized frame is dropped as a whole.
class CompanionStreamParser {
    uint8_t state_ = 0;
    uint16_t expected_ = 0, received_ = 0;
    uint8_t payload_[176]{};
public:
    bool feed(uint8_t byte);
    void reset() { state_ = 0; expected_ = received_ = 0; }
    const uint8_t* data() const { return payload_; }
    size_t length() const { return expected_; }
};
size_t companion_stream_encode(const uint8_t* data, size_t length, uint8_t* output, size_t capacity);
