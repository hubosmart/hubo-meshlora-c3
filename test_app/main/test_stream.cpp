// SPDX-License-Identifier: MIT
#include <Identity.h>
#include <Stream.h>
#include <cstring>
#include "unity.h"

namespace {
class MemoryReadStream : public Stream {
    const uint8_t* data_;
    size_t length_, cursor_ = 0;
public:
    MemoryReadStream(const uint8_t* data, size_t length) : data_(data), length_(length) {}
    int available() override { return static_cast<int>(length_ - cursor_); }
    int read() override { return cursor_ < length_ ? data_[cursor_++] : -1; }
    int peek() override { return cursor_ < length_ ? data_[cursor_] : -1; }
    size_t write(uint8_t) override { return 0; }
    size_t write(const uint8_t*, size_t) override { return 0; }
    void flush() override {}
};
}

TEST_CASE("identity refuses a truncated stream", "[stream]") {
    const uint8_t short_key[31] = {};
    MemoryReadStream input(short_key, sizeof(short_key));
    mesh::Identity id;
    TEST_ASSERT_FALSE(id.readFrom(input));
    TEST_ASSERT_EQUAL(0, input.available());
}

TEST_CASE("stream readBytes does not fill beyond EOF", "[stream]") {
    const uint8_t source[] = {1,2,3};
    uint8_t dest[5];
    std::memset(dest, 0xcc, sizeof(dest));
    MemoryReadStream input(source, sizeof(source));
    TEST_ASSERT_EQUAL(3, input.readBytes(dest, sizeof(dest)));
    TEST_ASSERT_EQUAL_MEMORY(source, dest, 3);
    TEST_ASSERT_EQUAL_HEX8(0xcc, dest[3]);
}
