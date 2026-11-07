// SPDX-License-Identifier: MIT
#include <Identity.h>
#include <SHA256.h>
#include <Utils.h>
#include <cstring>
#include "unity.h"

namespace {
void hex_bytes(const char* hex, uint8_t* out, size_t size) {
    TEST_ASSERT_EQUAL(size * 2, std::strlen(hex));
    auto nibble = [](char c) -> uint8_t {
        return c <= '9' ? c - '0' : c - 'a' + 10;
    };
    for (size_t i = 0; i < size; ++i) {
        out[i] = (nibble(hex[2 * i]) << 4) | nibble(hex[2 * i + 1]);
    }
}

// Public RFC 8032 test seed, used only in this test fixture.
class VectorRng : public mesh::RNG {
public:
    void random(uint8_t* out, size_t size) override {
        TEST_ASSERT_EQUAL(32, size);
        hex_bytes("9d61b19deffd5a60ba844af492ec2cc4"
                  "4449c5697b326919703bac031cae7f60", out, size);
    }
};
}

TEST_CASE("AES128 keeps the upstream block encoding", "[crypto]") {
    const uint8_t key[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    const uint8_t plain[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
                             0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const uint8_t expected[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
                                0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    uint8_t encrypted[16], decrypted[16];
    TEST_ASSERT_EQUAL(16, mesh::Utils::encrypt(key, encrypted, plain, sizeof(plain)));
    TEST_ASSERT_EQUAL_MEMORY(expected, encrypted, sizeof(expected));
    TEST_ASSERT_EQUAL(16, mesh::Utils::decrypt(key, decrypted, expected, sizeof(expected)));
    TEST_ASSERT_EQUAL_MEMORY(plain, decrypted, sizeof(plain));
}

TEST_CASE("SHA256 supports split input and truncation", "[crypto]") {
    uint8_t expected[32], actual[32];
    hex_bytes("ba7816bf8f01cfea414140de5dae2223"
              "b00361a396177a9cb410ff61f20015ad", expected, sizeof(expected));
    mesh::Utils::sha256(actual, 32, reinterpret_cast<const uint8_t*>("a"), 1,
                      reinterpret_cast<const uint8_t*>("bc"), 2);
    TEST_ASSERT_EQUAL_MEMORY(expected, actual, 32);
    std::memset(actual, 0xcc, sizeof(actual));
    mesh::Utils::sha256(actual, 8, reinterpret_cast<const uint8_t*>("abc"), 3);
    TEST_ASSERT_EQUAL_MEMORY(expected, actual, 8);
    TEST_ASSERT_EQUAL_HEX8(0xcc, actual[8]);
}

TEST_CASE("HMAC SHA256 matches RFC 4231", "[crypto]") {
    uint8_t key[20], expected[32], actual[32];
    std::memset(key, 0x0b, sizeof(key));
    hex_bytes("b0344c61d8db38535ca8afceaf0bf12b"
              "881dc200c9833da726e9376c2e32cff7", expected, sizeof(expected));
    SHA256 sha;
    sha.resetHMAC(key, sizeof(key));
    sha.update("Hi There", 8);
    sha.finalizeHMAC(key, sizeof(key), actual, sizeof(actual));
    TEST_ASSERT_EQUAL_MEMORY(expected, actual, 32);
}

TEST_CASE("Ed25519 identity matches RFC 8032 and rejects tampering", "[crypto]") {
    VectorRng rng;
    mesh::LocalIdentity id(&rng);
    uint8_t expected_pub[32], expected_sig[64], signature[64];
    hex_bytes("d75a980182b10ab7d54bfed3c964073a"
              "0ee172f3daa62325af021a68f707511a", expected_pub, sizeof(expected_pub));
    hex_bytes("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
              "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
              expected_sig, sizeof(expected_sig));
    const uint8_t empty = 0;
    TEST_ASSERT_EQUAL_MEMORY(expected_pub, id.pub_key, sizeof(expected_pub));
    id.sign(signature, &empty, 0);
    TEST_ASSERT_EQUAL_MEMORY(expected_sig, signature, sizeof(signature));
    TEST_ASSERT_TRUE(id.verify(expected_sig, &empty, 0));
    signature[0] ^= 1;
    TEST_ASSERT_FALSE(id.verify(signature, &empty, 0));
    uint8_t wrong_pub[32];
    std::memcpy(wrong_pub, expected_pub, sizeof(wrong_pub));
    wrong_pub[0] ^= 1;
    mesh::Identity other(wrong_pub);
    TEST_ASSERT_FALSE(other.verify(expected_sig, &empty, 0));
}

TEST_CASE("identity byte export remains private then public", "[crypto]") {
    VectorRng rng;
    mesh::LocalIdentity original(&rng), restored;
    uint8_t full[96], private_only[64], short_buffer[63];
    std::memset(short_buffer, 0xaa, sizeof(short_buffer));
    TEST_ASSERT_EQUAL(0, original.writeTo(short_buffer, sizeof(short_buffer)));
    TEST_ASSERT_EQUAL_HEX8(0xaa, short_buffer[0]);
    TEST_ASSERT_EQUAL(96, original.writeTo(full, sizeof(full)));
    TEST_ASSERT_EQUAL(64, original.writeTo(private_only, sizeof(private_only)));
    TEST_ASSERT_EQUAL_MEMORY(private_only, full, 64);
    TEST_ASSERT_EQUAL_MEMORY(original.pub_key, full + 64, 32);
    restored.readFrom(full, sizeof(full));
    TEST_ASSERT_TRUE(original.matches(restored));
}
