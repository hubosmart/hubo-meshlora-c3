// SPDX-License-Identifier: MIT
#include "hubo_store.h"
#include "esp_spiffs.h"

esp_err_t hubo_store_mount() {
    esp_vfs_spiffs_conf_t config = {};
    config.base_path = "/mesh";
    config.partition_label = "storage";
    config.max_files = 8;
    // A mount error leaves the state untouched; first-time initialization is done by an explicit provisioning flow.
    config.format_if_mount_failed = false;
    return esp_vfs_spiffs_register(&config);
}
