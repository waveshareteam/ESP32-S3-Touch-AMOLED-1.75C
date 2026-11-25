#pragma once

#include <array>
#include <cstdint>

#include "esp_err.h"
#include "lvgl.h"

// Owns the screen brightness and the screen timeout, which dims the screen and
// then turns it off. Call every function in LVGL context: an LVGL timer or
// event callback, or under LvLockGuard.
namespace display_power {

struct TimeoutOption {
    uint32_t seconds;
    const char *label;
};

inline constexpr uint32_t TIMEOUT_NEVER = 0;

inline constexpr std::array<TimeoutOption, 7> TIMEOUT_OPTIONS = {{
    {15, "15 seconds"},
    {30, "30 seconds"},
    {60, "1 minute"},
    {120, "2 minutes"},
    {300, "5 minutes"},
    {600, "10 minutes"},
    {TIMEOUT_NEVER, "Never"},
}};

inline constexpr int BRIGHTNESS_MIN = 20;
inline constexpr int BRIGHTNESS_MAX = 100;

esp_err_t start(lv_display_t *display);

int brightness();
void set_brightness(int percent);
void save_brightness();

uint32_t timeout_s();
void set_timeout_s(uint32_t seconds);

} // namespace display_power
