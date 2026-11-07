// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

// Concrete streams must implement real I/O so the core cannot report persistence success from default empty reads/writes.
class Stream {
public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual size_t write(uint8_t byte) = 0;
    virtual size_t write(const uint8_t* bytes, size_t length) = 0;
    virtual void flush() = 0;

    size_t readBytes(uint8_t* bytes, size_t length) {
        size_t count = 0;
        while (count < length) {
            const int value = read();
            if (value < 0) break;
            bytes[count++] = static_cast<uint8_t>(value);
        }
        return count;
    }
    size_t readBytes(char* bytes, size_t length) {
        return readBytes(reinterpret_cast<uint8_t*>(bytes), length);
    }
    size_t print(const char* text) {
        return write(reinterpret_cast<const uint8_t*>(text), strlen(text));
    }
    size_t print(char value) { return write(static_cast<uint8_t>(value)); }
    size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3))) {
        va_list args;
        va_start(args, format);
        char* text = nullptr;
        const int length = vasprintf(&text, format, args);
        va_end(args);
        if (length < 0) return 0;
        const size_t written = write(reinterpret_cast<const uint8_t*>(text), length);
        free(text);
        return written;
    }
    size_t print(long value, int base = 10) {
        char text[34];
        static_assert(sizeof(long) == sizeof(int));
        itoa(static_cast<int>(value), text, base);
        return print(text);
    }
    size_t print(unsigned long value, int base = 10) {
        char text[33];
        utoa(static_cast<unsigned int>(value), text, base);
        return print(text);
    }
    size_t print(double value, int precision = 2) {
        char text[64];
        const int length = snprintf(text, sizeof(text), "%.*f", precision, value);
        if (length < 0 || static_cast<size_t>(length) >= sizeof(text)) return 0;
        return print(text);
    }
    size_t print(int value) {
        char text[16];
        snprintf(text, sizeof(text), "%d", value);
        return print(text);
    }
    size_t println(const char* text = "") { return print(text) + write('\n'); }
    size_t println(int value) { return print(value) + write('\n'); }
};
