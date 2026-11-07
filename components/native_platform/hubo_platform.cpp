// SPDX-License-Identifier: MIT
#include "hubo_platform.h"
#include "bootloader_random.h"
#include "esp_random.h"
#include <sys/time.h>

namespace {
class NativeClock final : public mesh::RTCClock {
public:
    uint32_t getCurrentTime() override {
        timeval now{};
        gettimeofday(&now, nullptr);
        return static_cast<uint32_t>(now.tv_sec);
    }
    void setCurrentTime(uint32_t epoch) override {
        timeval now{static_cast<time_t>(epoch), 0};
        settimeofday(&now, nullptr);
    }
};

class NativeRng final : public mesh::RNG {
public:
    void random(uint8_t* out, size_t size) override {
        // The caller must have enabled boot-time hardware entropy or the IDF radio entropy source; fixed user-supplied seeds are not accepted.
        esp_fill_random(out, size);
    }
};
}

mesh::RTCClock& hubo_rtc() {
    static NativeClock clock;
    return clock;
}

mesh::RNG& hubo_rng() {
    static NativeRng rng;
    return rng;
}

void hubo_boot_entropy_enable() { bootloader_random_enable(); }
void hubo_boot_entropy_disable() { bootloader_random_disable(); }
