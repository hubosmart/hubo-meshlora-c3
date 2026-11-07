// SPDX-License-Identifier: MIT
#include "MyMesh.h"
#include "NativeSerialInterface.h"
#include "companion_transport.h"
#include "hubo_store.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace {
fs::FS filesystem("/mesh");
DataStore store(filesystem, hubo_rtc());
SimpleMeshTables tables;
NativeSerialInterface transport;
}
MyMesh the_mesh(hubo_radio(), hubo_rng(), hubo_rtc(), tables, store);
#if CONFIG_HUBO_HW_PROBE
extern "C" volatile uint32_t g_hw_probe_advert_request;   // hw_probe.cpp
extern "C" volatile uint32_t g_hw_probe_fail;             // reason the mesh task exited: 1 storage, 2 radio, 3 identity/settings, 4 transport; the esp_err is in the upper bits
#endif

namespace {
int discard_log(const char*, va_list) { return 0; }

void mesh_task(void*) {
    const esp_err_t mounted = hubo_store_mount();
    if (mounted != ESP_OK) {
        printf("StoreError: %s; storage preserved\n", esp_err_to_name(mounted));
#if CONFIG_HUBO_HW_PROBE
        g_hw_probe_fail = 1 | (static_cast<uint32_t>(mounted) << 8);
#endif
        vTaskDelete(nullptr);
        return;
    }
    hubo_boot_entropy_enable();
    if (!hubo_radio_init({433.0f, 62.5f, 8, 5, 1})) {
        printf("RadioError: E22 initialization failed\n");
#if CONFIG_HUBO_HW_PROBE
        g_hw_probe_fail = 2;
#endif
        vTaskDelete(nullptr);
        return;
    }
    store.begin();
    if (!the_mesh.begin(false)) {
        printf("StartupError: identity or settings invalid; storage preserved\n");
#if CONFIG_HUBO_HW_PROBE
        g_hw_probe_fail = 3;
#endif
        vTaskDelete(nullptr);
        return;
    }
    the_mesh.startInterface(transport);
#if ENV_INCLUDE_GPS == 1
    sensors.begin();
    the_mesh.applyGpsPrefs();   // switch GNSS according to the saved preference (off by default)
#endif
    // No text logs are written to the same USB port while the binary companion is running.
#if CONFIG_HUBO_TRANSPORT_BLE
    hubo_boot_entropy_disable();
    char name[30];
    snprintf(name, sizeof(name), "MeshCore-%.20s", the_mesh.getNodeName());
    const uint32_t pin = the_mesh.getNodePrefs()->ble_pin;
    const esp_err_t started = companion_ble_start(transport, name, pin ? pin : 123456);
#else
    esp_log_set_vprintf(discard_log);
    const esp_err_t started = companion_usb_start(transport);
#endif
    if (started != ESP_OK) {
        printf("TransportError: %s\n", esp_err_to_name(started));
#if CONFIG_HUBO_HW_PROBE
        g_hw_probe_fail = 4 | (static_cast<uint32_t>(started) << 8);
#endif
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
#if CONFIG_HUBO_HW_PROBE
        if (g_hw_probe_advert_request) {   // bring-up probe: send a zero-hop advert to verify RF transmission
            g_hw_probe_advert_request = 0;
            if (mesh::Packet* pkt = the_mesh.createSelfAdvert(the_mesh.getNodeName())) the_mesh.sendZeroHop(pkt);
        }
#endif
        the_mesh.loop();
#if ENV_INCLUDE_GPS == 1
        sensors.loop();
#endif
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
}

void hubo_hw_probe_start();   // hw_probe.cpp: GNSS bring-up probe, empty unless CONFIG_HUBO_HW_PROBE is enabled

extern "C" void app_main() {
    hubo_hw_probe_start();
    if (xTaskCreate(mesh_task, "mesh", 8192, nullptr, 5, nullptr) != pdPASS) {
        printf("StartupError: cannot create mesh task\n");
    }
}
