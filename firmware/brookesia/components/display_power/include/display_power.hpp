#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "esp_err.h"
#include "lvgl.h"

// Owns the screen brightness and the Awake/Dimmed/Off screen state. Call every
// function in LVGL context: an LVGL timer or event callback, or under LvLockGuard.
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

enum class ScreenState { Awake, Dimmed, Off };

constexpr ScreenState target_state(uint32_t idle_ms, uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return ScreenState::Awake;
    }
    const uint32_t dim_lead_ms = std::min<uint32_t>(10000, timeout_ms / 3);
    if (idle_ms >= timeout_ms) {
        return ScreenState::Off;
    }
    if (idle_ms >= timeout_ms - dim_lead_ms) {
        return ScreenState::Dimmed;
    }
    return ScreenState::Awake;
}

esp_err_t start(lv_display_t *display);

int brightness();
void set_brightness(int percent);
void save_brightness();

uint32_t timeout_s();
void set_timeout_s(uint32_t seconds);

} // namespace display_power
