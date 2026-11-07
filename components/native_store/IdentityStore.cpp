// SPDX-License-Identifier: MIT
// Based on MeshCore helpers/IdentityStore.cpp from SOURCE_LOCK.
// Keeps the file format; the original ignores short writes, whereas this version must propagate write/close/replace errors.
#include <helpers/IdentityStore.h>
#include <algorithm>

namespace {
bool make_path(const char* directory, const char* name, char* output, size_t size) {
    const int length = snprintf(output, size, "%s/%s.id", directory, name);
    return length >= 0 && static_cast<size_t>(length) < size;
}
bool save_identity(fs::FS& fs, const char* path, const mesh::LocalIdentity& identity,
                   const char* display_name) {
    char temporary[48];
    if (snprintf(temporary, sizeof(temporary), "%s.new", path) >= sizeof(temporary)) return false;
    auto file = fs.open(temporary, "w");
    if (!file) return false;
    bool success = identity.writeTo(file);
    if (success && display_name) {
        char name[32] = {};
        strncpy(name, display_name, sizeof(name) - 1);
        success = file.write(reinterpret_cast<const uint8_t*>(name), sizeof(name)) == sizeof(name);
    }
    const bool closed = file.close();
    return success && closed && fs.replace(temporary, path);
}
}

bool IdentityStore::load(const char* name, mesh::LocalIdentity& identity) {
    char path[40];
    if (!make_path(_dir, name, path, sizeof(path)) || !_fs->recover(path)) return false;
    auto file = _fs->open(path);
    if (!file) return false;
    mesh::LocalIdentity loaded;
    const bool success = loaded.readFrom(file);
    const bool closed = file.close();
    if (success && closed) identity = loaded;
    return success && closed;
}

bool IdentityStore::load(const char* name, mesh::LocalIdentity& identity,
                        char display_name[], int capacity) {
    if (capacity <= 0) return false;
    char path[40];
    if (!make_path(_dir, name, path, sizeof(path)) || !_fs->recover(path)) return false;
    auto file = _fs->open(path);
    if (!file) return false;
    mesh::LocalIdentity loaded;
    char display[32];
    const bool success = loaded.readFrom(file) && file.readBytes(display, sizeof(display)) == sizeof(display);
    const bool closed = file.close();
    if (!success || !closed) return false;
    identity = loaded;
    const size_t count = std::min(capacity - 1, 31);
    memcpy(display_name, display, count);
    display_name[count] = 0;
    return true;
}

bool IdentityStore::save(const char* name, const mesh::LocalIdentity& identity) {
    char path[40];
    return make_path(_dir, name, path, sizeof(path)) && save_identity(*_fs, path, identity, nullptr);
}

bool IdentityStore::save(const char* name, const mesh::LocalIdentity& identity, const char display_name[]) {
    char path[40];
    return make_path(_dir, name, path, sizeof(path)) && save_identity(*_fs, path, identity, display_name);
}
