// SPDX-License-Identifier: MIT
// Host test: g++ -std=c++17 -I components/companion tools/tests/test_hubo_nmea.cpp -o /tmp/test_hubo_nmea && /tmp/test_hubo_nmea
#include "HuboNmea.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string>

namespace {
std::string withChecksum(const std::string& body) {   // body excludes $ and *hh
    uint8_t x = 0;
    for (char c : body) x ^= static_cast<uint8_t>(c);
    char tail[8];
    snprintf(tail, sizeof tail, "*%02X\r\n", x);
    return "$" + body + tail;
}
void feedAll(HuboNmea& n, const std::string& s) {
    for (char c : s) n.feed(c);
}
}  // namespace

int main() {
    HuboNmea n;
    // A no-fix sentence captured from a real device: valid checksum, produces no fix
    feedAll(n, "$GNGLL,,,,,,V,N*7A\r\n");
    feedAll(n, "$GPTXT,01,01,01,ANTENNA OPEN*25\r\n");
    assert(n.fix().sentences_ok == 2 && n.fix().sentences_bad == 0 && !n.fix().valid);
    // bad checksum is counted and does not change state
    feedAll(n, "$GNGLL,,,,,,V,N*00\r\n");
    assert(n.fix().sentences_bad == 1 && n.fix().sentences_ok == 2);
    // Valid RMC: 31 deg 13.5000' N = 31.225, 121 deg 28.2000' E = 121.47
    feedAll(n, withChecksum("GNRMC,081836.00,A,3113.5000,N,12128.2000,E,0.5,90.0,010126,,,A"));
    assert(n.fix().valid);
    assert(fabs(n.fix().lat - 31.225) < 1e-6 && fabs(n.fix().lon - 121.47) < 1e-6);
    // GGA: quality 1, 9 satellites, altitude 12.3 m
    feedAll(n, withChecksum("GNGGA,081836.00,3113.5000,N,12128.2000,E,1,09,1.0,12.3,M,0.0,M,,"));
    assert(n.fix().sats == 9 && fabs(n.fix().alt_m - 12.3) < 1e-9 && n.fix().valid);
    // south latitude and west longitude are negative
    HuboNmea s;
    feedAll(s, withChecksum("GPRMC,000000.00,A,3352.0000,S,15112.0000,W,0,0,010126,,,A"));
    assert(s.fix().valid && s.fix().lat < -33.8 && s.fix().lon < -151.1);
    // BeiDou talkers are accepted too; V means the fix is lost
    feedAll(s, withChecksum("BDRMC,000001.00,V,,,,,,,010126,,,N"));
    assert(!s.fix().valid);
    // garbage bytes and half sentences do not crash; the next sentence is parsed normally
    HuboNmea g;
    for (int i = 0; i < 300; ++i) g.feed(static_cast<char>(i * 7));
    feedAll(g, "$GPRMC,0000");
    feedAll(g, withChecksum("GPRMC,000000.00,A,0000.0000,N,00000.0000,E,0,0,010126,,,A"));
    assert(g.fix().valid);
    puts("PASS: hubo nmea");
}
