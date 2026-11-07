// SPDX-License-Identifier: MIT
#include "unity.h"
#include "stream_framing.h"

TEST_CASE("USB framing keeps fragmented commands and rejects oversized frames", "[transport]") {
    CompanionStreamParser parser{};
    const uint8_t input[] = {'<', 2, 0, 22, 3};
    for (size_t i = 0; i < sizeof(input) - 1; ++i) TEST_ASSERT_FALSE(parser.feed(input[i]));
    TEST_ASSERT_TRUE(parser.feed(input[4]));
    TEST_ASSERT_EQUAL(2, parser.length());
    TEST_ASSERT_EQUAL_MEMORY(input + 3, parser.data(), 2);
    TEST_ASSERT_FALSE(parser.feed('<'));
    TEST_ASSERT_FALSE(parser.feed(177));
    TEST_ASSERT_FALSE(parser.feed(0));
    for (int i = 0; i < 177; ++i) TEST_ASSERT_FALSE(parser.feed('<'));
    for (size_t i = 0; i < sizeof(input) - 1; ++i) TEST_ASSERT_FALSE(parser.feed(input[i]));
    TEST_ASSERT_TRUE(parser.feed(input[4]));
    const uint8_t payload[] = {13, 13};
    uint8_t output[5];
    TEST_ASSERT_EQUAL(5, companion_stream_encode(payload, 2, output, sizeof(output)));
    const uint8_t expected[] = {'>', 2, 0, 13, 13};
    TEST_ASSERT_EQUAL_MEMORY(expected, output, 5);
}
