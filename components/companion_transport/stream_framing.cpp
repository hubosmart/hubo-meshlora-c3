// SPDX-License-Identifier: MIT
#include "stream_framing.h"
#include <cstring>

bool CompanionStreamParser::feed(uint8_t byte) {
    switch (state_) {
    case 0:
        if (byte == '<') state_ = 1;
        break;
    case 1:
        expected_ = byte;
        state_ = 2;
        break;
    case 2:
        expected_ |= static_cast<uint16_t>(byte) << 8;
        received_ = 0;
        state_ = expected_ ? 3 : 0;
        break;
    case 3:
        if (received_ < sizeof(payload_)) payload_[received_] = byte;
        if (++received_ == expected_) {
            state_ = 0;
            return expected_ <= sizeof(payload_);
        }
        break;
    }
    return false;
}

size_t companion_stream_encode(const uint8_t* data, size_t length, uint8_t* output, size_t capacity) {
    if (!data || !output || !length || length > 176 || capacity < length + 3) return 0;
    output[0] = '>';
    output[1] = length & 0xff;
    output[2] = length >> 8;
    memcpy(output + 3, data, length);
    return length + 3;
}
