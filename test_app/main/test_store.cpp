// SPDX-License-Identifier: MIT
#include "unity.h"
#include <FS.h>
#include <Identity.h>
#include <cstring>
#include "esp_spiffs.h"
#include <helpers/IdentityStore.h>
#include "DataStore.h"
#include "hubo_platform.h"

namespace {
void mount_test_store() {
    if (esp_spiffs_mounted("test_store")) return;
    esp_vfs_spiffs_conf_t config = {};
    config.base_path = "/testmesh";
    config.partition_label = "test_store";
    config.max_files = 4;
    config.format_if_mount_failed = false;
    TEST_ASSERT_EQUAL(ESP_OK, esp_vfs_spiffs_register(&config));
}

TEST_CASE("DataStore preferences roundtrip keeps escaped text and radio settings", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    DataStore store(storage, hubo_rtc());
    NodePrefs expected;
    strcpy(expected.node_name, "Hubo\"test");
    expected.default_scope_name[0] = 0;
    memset(expected.default_scope_key, 0, sizeof(expected.default_scope_key));
    expected.freq = 433.0f;
    expected.bw = 62.5f;
    expected.sf = 8;
    expected.cr = 5;
    TEST_ASSERT_TRUE(store.savePrefs(expected));
    NodePrefs actual;
    store.loadPrefs(actual);
    TEST_ASSERT_EQUAL_STRING(expected.node_name, actual.node_name);
    TEST_ASSERT_EQUAL_FLOAT(expected.freq, actual.freq);
    TEST_ASSERT_EQUAL_FLOAT(expected.bw, actual.bw);
    TEST_ASSERT_EQUAL(8, actual.sf);
    TEST_ASSERT_EQUAL(5, actual.cr);
}
}

TEST_CASE("native file preserves identity stream byte order", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    uint8_t bytes[96];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = i;
    mesh::LocalIdentity identity;
    identity.readFrom(bytes, sizeof(bytes));
    auto output = storage.open("/fixture.id", "w");
    TEST_ASSERT_TRUE(static_cast<bool>(output));
    TEST_ASSERT_TRUE(identity.writeTo(output));
    TEST_ASSERT_TRUE(output.close());
    auto input = storage.open("/fixture.id");
    TEST_ASSERT_EQUAL(96, input.size());
    uint8_t actual[96];
    TEST_ASSERT_EQUAL(96, input.read(actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_MEMORY(bytes + 64, actual, 32);
    TEST_ASSERT_EQUAL_MEMORY(bytes, actual + 32, 64);
}

TEST_CASE("native file refuses truncated identity without changing file", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    const uint8_t bytes[] = {1,2,3};
    auto output = storage.open("/truncated.id", "w");
    TEST_ASSERT_EQUAL(3, output.write(bytes, sizeof(bytes)));
    TEST_ASSERT_TRUE(output.close());
    auto input = storage.open("/truncated.id");
    mesh::LocalIdentity identity;
    TEST_ASSERT_FALSE(identity.readFrom(input));
    TEST_ASSERT_TRUE(input.close());
    input = storage.open("/truncated.id");
    TEST_ASSERT_EQUAL(3, input.size());
    uint8_t actual[3];
    TEST_ASSERT_EQUAL(3, input.read(actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_MEMORY(bytes, actual, 3);
}

TEST_CASE("native file replacement preserves old file on rename failure", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    auto file = storage.open("/old", "w");
    TEST_ASSERT_EQUAL(3, file.print("old"));
    TEST_ASSERT_TRUE(file.close());
    TEST_ASSERT_FALSE(storage.rename("/missing", "/old"));
    auto old = storage.open("/old");
    char actual[3];
    TEST_ASSERT_EQUAL(3, old.readBytes(actual, 3));
    TEST_ASSERT_EQUAL_MEMORY("old", actual, 3);
    TEST_ASSERT_TRUE(old.close());
    auto replacement = storage.open("/replacement", "w");
    TEST_ASSERT_EQUAL(3, replacement.print("new"));
    TEST_ASSERT_TRUE(replacement.close());
    // SPIFFS does not overwrite an existing target; the higher layer must keep a backup and handle recovery.
    TEST_ASSERT_FALSE(storage.rename("/replacement", "/old"));
    old = storage.open("/old");
    TEST_ASSERT_EQUAL(3, old.readBytes(actual, 3));
    TEST_ASSERT_EQUAL_MEMORY("old", actual, 3);
}

TEST_CASE("identity store saves loads and preserves state on corrupt input", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    IdentityStore store(storage, "/identity");
    uint8_t bytes[96];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = i + 1;
    mesh::LocalIdentity original, restored;
    original.readFrom(bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(store.save("test", original));
    TEST_ASSERT_TRUE(store.load("test", restored));
    uint8_t actual[96];
    TEST_ASSERT_EQUAL(96, restored.writeTo(actual, sizeof(actual)));
    TEST_ASSERT_EQUAL_MEMORY(bytes, actual, sizeof(bytes));
    // The second save must handle the existing file and must not delete the original identity first.
    TEST_ASSERT_TRUE(store.save("test", original));
    auto corrupt = storage.open("/identity/bad.id", "w");
    TEST_ASSERT_EQUAL(3, corrupt.write(bytes, 3));
    TEST_ASSERT_TRUE(corrupt.close());
    TEST_ASSERT_FALSE(store.load("bad", restored));
    restored.writeTo(actual, sizeof(actual));
    TEST_ASSERT_EQUAL_MEMORY(bytes, actual, sizeof(bytes));
    auto retained = storage.open("/identity/bad.id");
    TEST_ASSERT_EQUAL(3, retained.size());
}

namespace {
struct SingleRecordHost : DataStoreHost {
    ContactInfo contact{};
    ChannelDetails channel{};
    bool contact_loaded = false, channel_loaded = false;
    bool onContactLoaded(const ContactInfo& value) override {
        contact = value; contact_loaded = true; return true;
    }
    bool getContactForSave(uint32_t index, ContactInfo& value) override {
        if (index) return false;
        value = contact; return true;
    }
    bool onChannelLoaded(uint8_t index, const ChannelDetails& value) override {
        if (index) return false;
        channel = value; channel_loaded = true; return true;
    }
    bool getChannelForSave(uint8_t index, ChannelDetails& value) override {
        if (index) return false;
        value = channel; return true;
    }
};
}

TEST_CASE("DataStore preserves contact and channel layouts", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    DataStore store(storage, hubo_rtc());
    SingleRecordHost original, restored;
    strcpy(original.contact.name, "peer");
    memset(original.contact.id.pub_key, 0x42, 32);
    original.contact.out_path_len = 0xff;
    original.contact.gps_lat = 123456;
    strcpy(original.channel.name, "test-channel");
    memset(original.channel.channel.secret, 0x35, 32);
    store.saveContacts(&original);
    store.saveChannels(&original);
    auto contacts = storage.open("/contacts3");
    auto channels = storage.open("/channels2");
    TEST_ASSERT_EQUAL(152, contacts.size());
    TEST_ASSERT_EQUAL(68, channels.size());
    contacts.close(); channels.close();
    store.loadContacts(&restored);
    store.loadChannels(&restored);
    TEST_ASSERT_TRUE(restored.contact_loaded);
    TEST_ASSERT_TRUE(restored.channel_loaded);
    TEST_ASSERT_EQUAL_STRING("peer", restored.contact.name);
    TEST_ASSERT_EQUAL_MEMORY(original.contact.id.pub_key, restored.contact.id.pub_key, 32);
    TEST_ASSERT_EQUAL(123456, restored.contact.gps_lat);
    TEST_ASSERT_EQUAL_STRING("test-channel", restored.channel.name);
    TEST_ASSERT_EQUAL_MEMORY(original.channel.channel.secret, restored.channel.channel.secret, 32);
}

TEST_CASE("identity store recovers backup after interrupted replacement", "[store]") {
    mount_test_store();
    fs::FS storage("/testmesh");
    IdentityStore store(storage, "/identity");
    uint8_t bytes[96] = {};
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = i + 7;
    mesh::LocalIdentity original, restored;
    original.readFrom(bytes, sizeof(bytes));
    TEST_ASSERT_TRUE(store.save("recover", original));
    storage.remove("/identity/recover.id.bak");
    // Simulate an interruption after the old file was kept but before the new file was committed.
    TEST_ASSERT_TRUE(storage.rename("/identity/recover.id", "/identity/recover.id.bak"));
    TEST_ASSERT_TRUE(store.load("recover", restored));
    uint8_t actual[96];
    restored.writeTo(actual, sizeof(actual));
    TEST_ASSERT_EQUAL_MEMORY(bytes, actual, sizeof(bytes));
}
