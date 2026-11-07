// SPDX-License-Identifier: MIT
#include "HuboSX1268Radio.h"
#include "hal/ESP-IDF/EspHal.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

namespace {
EspHal hal(10, 6, 7, SPI2_HOST, 2000000);
Module module(&hal, 8, 3, 5, 4);
CustomSX1268 chip(&module);
HuboSX1268Radio radio(chip);
}

HuboSX1268Radio& radio_driver = radio;

int16_t g_e22_init_error = -9999;   // -9999: native_init has not run yet

bool hubo_radio_init(const RadioSettings& settings) {
#if CONFIG_HUBO_GPS_EN_N_GPIO >= 0
    // GNSS variant: GPS_EN_N is active low, so drive it high to keep the GNSS powered off (this firmware has no GNSS driver yet); GPIO2 is also a strapping pin and must be high at power-up
    gpio_set_direction(static_cast<gpio_num_t>(CONFIG_HUBO_GPS_EN_N_GPIO), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_HUBO_GPS_EN_N_GPIO), 1);
#endif
    return radio.initialize(settings);
}
mesh::Radio& hubo_radio() { return radio; }
