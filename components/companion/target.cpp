// SPDX-License-Identifier: MIT
#include "target.h"

HuboBoard board;
#if CONFIG_HUBO_GPS_EN_N_GPIO >= 0
HuboGnssSensorManager sensors;
#else
SensorManager sensors;
#endif
