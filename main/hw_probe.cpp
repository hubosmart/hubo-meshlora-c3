// SPDX-License-Identifier: MIT
#include "sdkconfig.h"
#if CONFIG_HUBO_HW_PROBE
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "RadioLibWrappers.h"
#include "hubo_radio.h"
#include "NativeSX1268.h"
#include "target.h"

// GNSS bring-up probe: powers the GNSS, receives NMEA on UART1 (sending queries if needed) and stores the results in globals,
// which are read out over JTAG (gdb memory reads). No serial log is used: the USB port prints no text while the companion firmware runs.
extern "C" {
struct HuboHwProbe {
    uint32_t magic;          // 0x48575052 "HWPR": confirms this is the expected structure
    uint32_t stage;          // 0 not started, 1 powered on, 2 trying baud rates, 3 all done
    uint32_t baud;           // current (or last tried) baud rate
    uint32_t rx_bytes;       // bytes received at this baud rate
    uint32_t nmea_ok;        // NMEA sentences with a valid checksum
    uint32_t nmea_bad;       // '$' sentences with a bad checksum
    uint32_t ubx_frames;     // UBX frame headers (B5 62) seen
    uint32_t best_baud;      // baud rate at which valid NMEA or UBX was received; 0 means neither
    uint32_t total_bytes;    // total bytes received across all baud rates
    char head[160];          // first 160 bytes received (non-printable bytes replaced with '.')
    char last_sentence[96];  // last NMEA sentence with a valid checksum
    // —— LoRa（SX1268）——
    uint32_t mesh_alive;     // the "mesh" task is still alive after 6 s: E22 init and BLE start both succeeded (the failure paths delete this task)
    int32_t noise_floor;     // noise floor in dBm (RX is sampled periodically in the mesh loop; a plausible value means SPI and the RX chain work)
    uint32_t tx_requested;   // whether a zero-hop advert has been requested
    uint32_t pkts_sent;      // RadioLib transmit count when the probe finishes (>= 1 means the TX-done interrupt came back)
    uint32_t pkts_recv;      // receive count (only increases when another Meshlora nearby on the same frequency sends an advert)
    uint32_t pkts_recv_err;
    int32_t last_rssi_x10;   // latest received RSSI (x10 dBm)
    int32_t last_snr_x10;
};
volatile uint32_t g_hw_probe_fail = 0;             // written by app_main.cpp: why the mesh task exited early
volatile uint32_t g_hw_probe_advert_request = 0;   // the probe sets it to 1 and the mesh task sends a zero-hop advert in its own loop (avoids touching the send queue from another task)
volatile HuboHwProbe g_hw_probe = {0x48575052, 0, 0, 0, 0, 0, 0, 0, 0, {0}, {0}, 0, 0, 0, 0, 0, 0, 0, 0};
}

namespace {
constexpr gpio_num_t kGpsEnN = static_cast<gpio_num_t>(CONFIG_HUBO_GPS_EN_N_GPIO);
constexpr uart_port_t kUart = UART_NUM_1;
constexpr int kPinRx = 20;   // GNSS TXD -> C3
constexpr int kPinTx = 21;   // C3 -> GNSS RXD (through a series resistor)

int hexValue(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// s starts with '$' and ends with '\n', in the form $....*hh\r\n
bool nmeaChecksumOk(const char* s, size_t n) {
    if (n < 9 || s[0] != '$') return false;
    uint8_t x = 0;
    size_t i = 1;
    for (; i < n && s[i] != '*'; ++i) x ^= static_cast<uint8_t>(s[i]);
    if (i + 2 >= n) return false;
    const int h = hexValue(s[i + 1]), l = hexValue(s[i + 2]);
    return h >= 0 && l >= 0 && x == static_cast<uint8_t>(h * 16 + l);
}

void listen(uint32_t baud, uint32_t ms) {
    uart_set_baudrate(kUart, baud);
    uart_flush_input(kUart);
    g_hw_probe.baud = baud;
    g_hw_probe.rx_bytes = 0;
    char line[128];
    size_t len = 0;
    uint8_t buf[64];
    uint8_t prev = 0;
    const TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
    while (xTaskGetTickCount() < end) {
        const int n = uart_read_bytes(kUart, buf, sizeof buf, pdMS_TO_TICKS(100));
        for (int i = 0; i < n; ++i) {
            const uint8_t c = buf[i];
            if (g_hw_probe.total_bytes < sizeof g_hw_probe.head - 1)
                const_cast<char*>(g_hw_probe.head)[g_hw_probe.total_bytes] = (c >= 32 && c < 127) ? static_cast<char>(c) : '.';
            ++g_hw_probe.total_bytes;
            ++g_hw_probe.rx_bytes;
            if (prev == 0xB5 && c == 0x62) ++g_hw_probe.ubx_frames;
            prev = c;
            if (c == '$') len = 0;
            if (len < sizeof line - 1) line[len++] = static_cast<char>(c);
            if (c == '\n' && len > 5 && line[0] == '$') {
                if (nmeaChecksumOk(line, len)) {
                    ++g_hw_probe.nmea_ok;
                    g_hw_probe.best_baud = baud;
                    const size_t m = len < sizeof g_hw_probe.last_sentence - 1 ? len : sizeof g_hw_probe.last_sentence - 1;
                    memcpy(const_cast<char*>(g_hw_probe.last_sentence), line, m);
                    const_cast<char*>(g_hw_probe.last_sentence)[m] = 0;
                } else {
                    ++g_hw_probe.nmea_bad;
                }
                len = 0;
            }
        }
    }
    if (g_hw_probe.ubx_frames && !g_hw_probe.best_baud) g_hw_probe.best_baud = baud;
}

void sampleRadio() {
    auto& r = static_cast<RadioLibWrapper&>(hubo_radio());
    g_hw_probe.noise_floor = r.getNoiseFloor();
    g_hw_probe.pkts_sent = r.getPacketsSent();
    g_hw_probe.pkts_recv = r.getPacketsRecv();
    g_hw_probe.pkts_recv_err = r.getPacketsRecvErrors();
    g_hw_probe.last_rssi_x10 = static_cast<int32_t>(r.getLastRSSI() * 10);
    g_hw_probe.last_snr_x10 = static_cast<int32_t>(r.getLastSNR() * 10);
}

void probeTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(6000));   // let the mesh task start first
    g_hw_probe.mesh_alive = xTaskGetHandle("mesh") != nullptr ? 1 : 0;
    g_hw_probe_advert_request = 1;     // LoRa TX test: have the mesh task send a zero-hop advert
    g_hw_probe.tx_requested = 1;
    // Test with the production GNSS driver (HuboGnssSensorManager): turn on the "gps" setting; the mesh task loop receives and parses NMEA
    sensors.setSettingValue("gps", "1");
    g_hw_probe.stage = 2;
    vTaskDelay(pdMS_TO_TICKS(15000));
    g_hw_probe.nmea_ok = sensors.fix().sentences_ok;
    g_hw_probe.nmea_bad = sensors.fix().sentences_bad;
    g_hw_probe.best_baud = sensors.fix().sentences_ok ? 9600 : 0;
    g_hw_probe.baud = sensors.fix().valid ? 1 : 0;      // reused field: 1 = driver reports a fix
    g_hw_probe.rx_bytes = sensors.fix().sats;           // reused field: satellite count
    sensors.setSettingValue("gps", "0");                // power off
    vTaskDelay(pdMS_TO_TICKS(2000));
    sampleRadio();
    g_hw_probe.stage = 3;
    // Also print one line on the USB port so it can be read without JTAG (with the BLE transport the companion keeps logging enabled)
    printf("HWPROBE done best_baud=%u total_bytes=%u nmea_ok=%u nmea_bad=%u ubx=%u head=[%s] last=[%s]\n"
           "HWPROBE lora mesh_alive=%u noise_floor=%d tx_requested=%u sent=%u recv=%u recv_err=%u rssi_x10=%d snr_x10=%d mesh_fail=0x%x e22_init_error=%d\n",
           static_cast<unsigned>(g_hw_probe.best_baud), static_cast<unsigned>(g_hw_probe.total_bytes),
           static_cast<unsigned>(g_hw_probe.nmea_ok), static_cast<unsigned>(g_hw_probe.nmea_bad),
           static_cast<unsigned>(g_hw_probe.ubx_frames), const_cast<const char*>(g_hw_probe.head),
           const_cast<const char*>(g_hw_probe.last_sentence),
           static_cast<unsigned>(g_hw_probe.mesh_alive), static_cast<int>(g_hw_probe.noise_floor),
           static_cast<unsigned>(g_hw_probe.tx_requested), static_cast<unsigned>(g_hw_probe.pkts_sent),
           static_cast<unsigned>(g_hw_probe.pkts_recv), static_cast<unsigned>(g_hw_probe.pkts_recv_err),
           static_cast<int>(g_hw_probe.last_rssi_x10), static_cast<int>(g_hw_probe.last_snr_x10),
           static_cast<unsigned>(g_hw_probe_fail), static_cast<int>(g_e22_init_error));
    vTaskDelete(nullptr);
}
}  // namespace

void hubo_hw_probe_start() { xTaskCreate(probeTask, "hw_probe", 4096, nullptr, 3, nullptr); }
#else
void hubo_hw_probe_start() {}
#endif
