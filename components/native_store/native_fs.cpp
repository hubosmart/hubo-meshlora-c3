// SPDX-License-Identifier: MIT
#include "FS.h"
#include <sys/stat.h>
#include <unistd.h>
#include <climits>

namespace fs {
size_t File::size() const {
    struct stat info = {};
    return file_ && fstat(fileno(file_), &info) == 0 ? info.st_size : 0;
}

int File::available() {
    if (!file_) return 0;
    const long position = ftell(file_);
    const size_t length = size();
    return position >= 0 && static_cast<size_t>(position) < length
        ? static_cast<int>(length - position) : 0;
}

int File::read() { return file_ ? fgetc(file_) : -1; }
size_t File::read(uint8_t* bytes, size_t length) {
    return file_ ? fread(bytes, 1, length, file_) : 0;
}
int File::peek() {
    const int byte = read();
    if (byte >= 0) ungetc(byte, file_);
    return byte;
}
size_t File::write(const uint8_t* bytes, size_t length) {
    if (!file_) return 0;
    const size_t count = fwrite(bytes, 1, length, file_);
    if (count != length) failed_ = true;
    return count;
}
void File::flush() {
    if (file_ && fflush(file_) != 0) failed_ = true;
}
bool File::close() {
    if (!file_) return !failed_;
    if (ferror(file_)) failed_ = true;
    if (fclose(file_) != 0) failed_ = true;
    file_ = nullptr;
    return !failed_;
}
bool File::seek(uint32_t offset) {
    return file_ && offset <= LONG_MAX && fseek(file_, offset, SEEK_SET) == 0;
}

bool FS::path(const char* name, char* out, size_t capacity) const {
    if (!name || name[0] != '/') return false;
    const int length = snprintf(out, capacity, "%s%s", base_, name);
    return length >= 0 && static_cast<size_t>(length) < capacity;
}
File FS::open(const char* name, const char* mode, bool) {
    char full[128];
    return File(path(name, full, sizeof(full)) ? fopen(full, mode) : nullptr);
}
bool FS::exists(const char* name) const {
    char full[128];
    struct stat info = {};
    return path(name, full, sizeof(full)) && stat(full, &info) == 0;
}
bool FS::remove(const char* name) {
    char full[128];
    return path(name, full, sizeof(full)) && unlink(full) == 0;
}
bool FS::rename(const char* source, const char* destination) {
    char from[128], to[128];
    return path(source, from, sizeof(from)) && path(destination, to, sizeof(to))
        && ::rename(from, to) == 0;
}
bool FS::mkdir(const char* name) {
    char full[128];
    return path(name, full, sizeof(full)) && ::mkdir(full, 0700) == 0;
}
bool FS::recover(const char* name) {
    char backup[64];
    if (snprintf(backup, sizeof(backup), "%s.bak", name) >= sizeof(backup)) return false;
    // Never overwrite an existing main file with the backup automatically; corruption should be reported by the caller.
    if (exists(name)) return true;
    if (!exists(backup)) return true;
    return rename(backup, name);
}
bool FS::replace(const char* source, const char* destination) {
    if (!exists(source) || !recover(destination)) return false;
    char backup[64];
    if (snprintf(backup, sizeof(backup), "%s.bak", destination) >= sizeof(backup)) return false;
    const bool had_old = exists(destination);
    if (had_old) {
        if (exists(backup) && !remove(backup)) return false;
        if (!rename(destination, backup)) return false;
    }
    if (!rename(source, destination)) {
        if (had_old) rename(backup, destination);
        return false;
    }
    // Keep the previous version; the next replacement reclaims the backup only if the current main file exists.
    return true;
}
}
