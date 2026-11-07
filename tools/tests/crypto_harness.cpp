// SPDX-License-Identifier: MIT
// Host test program: computes AES-128 single block, SHA-256 and HMAC-SHA256 with rweather/Crypto (the same source as the firmware).
// Per-line input: AES <key32hex> <block32hex> | SHA <hex or -> | HMAC <keyhex> <msghex or -> <outlen>
#include <AES.h>
#include <SHA256.h>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2) v.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return v;
}

static void hex(const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i) std::printf("%02x", p[i]);
    std::printf("\n");
}

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string op;
        in >> op;
        if (op == "AES") {
            std::string k, b;
            in >> k >> b;
            auto key = unhex(k), blk = unhex(b);
            AES128 aes;
            aes.setKey(key.data(), 16);
            uint8_t enc[16], dec[16];
            aes.encryptBlock(enc, blk.data());
            aes.decryptBlock(dec, enc);
            hex(enc, 16);
            hex(dec, 16);
        } else if (op == "SHA") {
            std::string m;
            in >> m;
            auto msg = unhex(m == "-" ? "" : m);
            SHA256 sha;
            sha.reset();
            sha.update(msg.data(), msg.size());
            uint8_t out[32];
            sha.finalize(out, 32);
            hex(out, 32);
        } else if (op == "HMAC") {
            std::string k, m;
            size_t outlen = 32;
            in >> k >> m >> outlen;
            auto key = unhex(k), msg = unhex(m == "-" ? "" : m);
            SHA256 sha;
            sha.resetHMAC(key.data(), key.size());
            sha.update(msg.data(), msg.size());
            uint8_t out[32];
            sha.finalizeHMAC(key.data(), key.size(), out, outlen);
            hex(out, outlen);
        }
        std::fflush(stdout);
    }
    return 0;
}
