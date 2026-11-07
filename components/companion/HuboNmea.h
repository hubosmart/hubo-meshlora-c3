// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Minimal NMEA parser (pure logic, host-testable): checksum, $xxRMC (fix validity, lat/lon), $xxGGA (quality, satellite count, altitude).
// Only talkers starting with GP/GN/BD/GL/GA are accepted; no dependency on Arduino or ESP-IDF.
struct HuboNmeaFix {
    bool valid = false;        // latest RMC status is A, or GGA quality > 0
    double lat = 0, lon = 0;   // degrees; south latitude / west longitude are negative
    double alt_m = 0;
    uint8_t sats = 0;
    uint32_t sentences_ok = 0; // number of sentences with a valid checksum (link health)
    uint32_t sentences_bad = 0;
};

class HuboNmea {
public:
    // Feed one byte at a time; returns true once a full line (ending in \n) has been processed.
    bool feed(char c) {
        if (c == '$') len_ = 0;
        if (len_ >= sizeof line_ - 1) { len_ = 0; return false; }
        line_[len_++] = c;
        if (c != '\n') return false;
        line_[len_] = 0;
        const bool ok = parseLine(line_, len_);
        len_ = 0;
        return ok;
    }
    const HuboNmeaFix& fix() const { return fix_; }
    void reset() { fix_ = HuboNmeaFix(); len_ = 0; }

    static bool checksumOk(const char* s, size_t n) {
        if (n < 9 || s[0] != '$') return false;
        uint8_t x = 0;
        size_t i = 1;
        for (; i < n && s[i] != '*'; ++i) x ^= static_cast<uint8_t>(s[i]);
        if (i + 2 >= n) return false;
        const int h = hex(s[i + 1]), l = hex(s[i + 2]);
        return h >= 0 && l >= 0 && x == static_cast<uint8_t>(h * 16 + l);
    }

private:
    static int hex(char c) {
        return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    }
    // ddmm.mmmm / dddmm.mmmm -> degrees
    static bool degrees(const char* f, size_t len, int deg_digits, double* out) {
        if (len < static_cast<size_t>(deg_digits) + 3) return false;
        double deg = 0;
        for (int i = 0; i < deg_digits; ++i) {
            if (f[i] < '0' || f[i] > '9') return false;
            deg = deg * 10 + (f[i] - '0');
        }
        double min = 0, frac = 0.1;
        size_t i = static_cast<size_t>(deg_digits);
        for (; i < len && f[i] != '.'; ++i) {
            if (f[i] < '0' || f[i] > '9') return false;
            min = min * 10 + (f[i] - '0');
        }
        if (i < len && f[i] == '.') {
            for (++i; i < len; ++i, frac /= 10) {
                if (f[i] < '0' || f[i] > '9') return false;
                min += (f[i] - '0') * frac;
            }
        }
        *out = deg + min / 60.0;
        return true;
    }
    static double number(const char* f, size_t len) {   // plain decimal; a minus sign and decimal point are allowed
        double v = 0, frac = 0.1;
        bool neg = false, dot = false;
        for (size_t i = 0; i < len; ++i) {
            if (f[i] == '-') neg = true;
            else if (f[i] == '.') dot = true;
            else if (f[i] >= '0' && f[i] <= '9') {
                if (!dot) v = v * 10 + (f[i] - '0');
                else { v += (f[i] - '0') * frac; frac /= 10; }
            }
        }
        return neg ? -v : v;
    }

    bool parseLine(const char* s, size_t n) {
        if (!checksumOk(s, n)) {
            if (s[0] == '$') ++fix_.sentences_bad;
            return false;
        }
        ++fix_.sentences_ok;
        // Field split: up to '*', comma separated, at most 16 fields
        const char* f[16];
        size_t fl[16];
        int nf = 0;
        const char* p = s + 1;
        const char* start = p;
        for (; *p && *p != '*' && nf < 16; ++p) {
            if (*p == ',') { f[nf] = start; fl[nf++] = static_cast<size_t>(p - start); start = p + 1; }
        }
        if (nf < 15 && *p == '*') { f[nf] = start; fl[nf++] = static_cast<size_t>(p - start); }
        if (nf < 1 || fl[0] < 5) return true;
        const char* id = f[0];
        const bool known = (id[0] == 'G' && (id[1] == 'P' || id[1] == 'N' || id[1] == 'L' || id[1] == 'A')) || (id[0] == 'B' && id[1] == 'D');
        if (!known) return true;
        if (!strncmp(id + 2, "RMC", 3) && nf >= 7) {
            if (fl[2] == 1 && f[2][0] == 'A') {
                double lat, lon;
                if (degrees(f[3], fl[3], 2, &lat) && degrees(f[5], fl[5], 3, &lon)) {
                    fix_.lat = (fl[4] == 1 && f[4][0] == 'S') ? -lat : lat;
                    fix_.lon = (fl[6] == 1 && f[6][0] == 'W') ? -lon : lon;
                    fix_.valid = true;
                }
            } else {
                fix_.valid = false;   // V: fix lost
            }
        } else if (!strncmp(id + 2, "GGA", 3) && nf >= 10) {
            const int quality = fl[6] ? f[6][0] - '0' : 0;
            fix_.sats = static_cast<uint8_t>(number(f[7], fl[7]));
            if (quality > 0) fix_.alt_m = number(f[9], fl[9]);
            else fix_.valid = false;
        }
        return true;
    }

    char line_[120] = {};
    size_t len_ = 0;
    HuboNmeaFix fix_;
};
