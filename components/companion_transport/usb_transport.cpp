// SPDX-License-Identifier: MIT
#include "companion_transport.h"
#include "stream_framing.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/task.h"

namespace {
void usb_task(void* context) {
    auto& transport = *static_cast<NativeSerialInterface*>(context);
    CompanionStreamParser parser;
    uint8_t payload[MAX_FRAME_SIZE], encoded[MAX_FRAME_SIZE + 3];
    size_t output_length = 0, output_position = 0;
    bool pending_input = false;
    TickType_t last_input = xTaskGetTickCount();
    for (;;) {
        const bool connected = usb_serial_jtag_is_connected();
        transport.setConnected(connected);
        if (!connected) {
            parser.reset(); pending_input = false;
            output_length = output_position = 0;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (pending_input) {
            if (transport.submit(parser.data(), parser.length())) pending_input = false;
        } else {
            uint8_t byte;
            if (usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(2)) == 1) {
                if (xTaskGetTickCount() - last_input > pdMS_TO_TICKS(1000)) parser.reset();
                last_input = xTaskGetTickCount();
                pending_input = parser.feed(byte);
            }
        }
        if (!output_length) {
            size_t size;
            if (transport.take(payload, sizeof(payload), &size)) {
                output_length = companion_stream_encode(payload, size, encoded, sizeof(encoded));
                output_position = 0;
            }
        }
        if (output_length) {
            const int written = usb_serial_jtag_write_bytes(encoded + output_position,
                output_length - output_position, pdMS_TO_TICKS(2));
            if (written > 0) output_position += written;
            if (output_position == output_length) output_length = 0;
        }
        if (pending_input) vTaskDelay(1);
    }
}
}

esp_err_t companion_usb_start(NativeSerialInterface& transport) {
    usb_serial_jtag_driver_config_t config = {};
    config.rx_buffer_size = 1024;
    config.tx_buffer_size = 1024;
    const esp_err_t result = usb_serial_jtag_driver_install(&config);
    if (result != ESP_OK) return result;
    if (xTaskCreate(usb_task, "companion_usb", 4096, &transport, 4, nullptr) != pdPASS) {
        usb_serial_jtag_driver_uninstall();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
