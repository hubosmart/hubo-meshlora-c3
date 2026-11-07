// SPDX-License-Identifier: MIT
#pragma once
#include <Stream.h>
#include <utility>

namespace fs {
class File : public Stream {
    FILE* file_ = nullptr;
    bool failed_ = false;
public:
    explicit File(FILE* file = nullptr) : file_(file) {}
    ~File() override { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&& other) noexcept
        : file_(std::exchange(other.file_, nullptr)), failed_(other.failed_) {}
    File& operator=(File&& other) noexcept {
        if (this != &other) {
            close();
            file_ = std::exchange(other.file_, nullptr);
            failed_ = other.failed_;
        }
        return *this;
    }
    explicit operator bool() const { return file_ != nullptr; }
    int available() override;
    int read() override;
    size_t read(uint8_t* bytes, size_t length);
    int peek() override;
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* bytes, size_t length) override;
    void flush() override;
    bool close();
    size_t size() const;
    bool seek(uint32_t offset);
};

class FS {
    const char* base_;
    bool path(const char* name, char* out, size_t capacity) const;
public:
    explicit FS(const char* base) : base_(base) {}
    File open(const char* name, const char* mode = "r", bool create = false);
    bool exists(const char* name) const;
    bool remove(const char* name);
    bool rename(const char* source, const char* destination);
    bool mkdir(const char* name);
    bool recover(const char* name);
    bool replace(const char* source, const char* destination);
};
}
using fs::File;
