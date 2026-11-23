#include "display_power.hpp"

#include <algorithm>
#include <cinttypes>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs.h"

namespace display_power {
namespace {

constexpr const char *TAG = "display_power";
constexpr const char *NVS_NAMESPACE = "display_power";
constexpr const char *NVS_KEY_BRIGHTNESS = "brightness";
constexpr const char *NVS_KEY_TIMEOUT = "timeout_s";

constexpr int DEFAULT_BRIGHTNESS = 100;
constexpr uint32_t DEFAULT_TIMEOUT_S = 60;
constexpr int DIMMED_BRIGHTNESS = 10;
constexpr uint32_t IDLE_POLL_MS = 200;

constexpr gpio_num_t PWR_KEY_GPIO = BSP_PWR_KEY_IO;
constexpr int PWR_KEY_PRESSED_LEVEL = 1;
constexpr uint32_t PWR_POLL_MS = 20;
constexpr uint32_t PWR_PRESS_MIN_MS = 50;
// A 4 s hold powers the board off in the AXP2101, so only short presses toggle.
constexpr uint32_t PWR_PRESS_MAX_MS = 1500;

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

constexpr bool is_timeout_option(uint32_t seconds)
{
    return std::any_of(TIMEOUT_OPTIONS.begin(), TIMEOUT_OPTIONS.end(),
                       [seconds](const TimeoutOption &option) { return option.seconds == seconds; });
}

constexpr bool is_valid_brightness(int percent)
{
    return percent >= BRIGHTNESS_MIN && percent <= BRIGHTNESS_MAX;
}

static_assert(is_timeout_option(DEFAULT_TIMEOUT_S));
static_assert(is_timeout_option(TIMEOUT_NEVER));
static_assert(is_valid_brightness(DEFAULT_BRIGHTNESS));
static_assert(target_state(UINT32_MAX, 0) == ScreenState::Awake);
static_assert(target_state(49999, 60000) == ScreenState::Awake);
static_assert(target_state(50000, 60000) == ScreenState::Dimmed);
static_assert(target_state(59999, 60000) == ScreenState::Dimmed);
static_assert(target_state(60000, 60000) == ScreenState::Off);
static_assert(target_state(9999, 15000) == ScreenState::Awake);
static_assert(target_state(10000, 15000) == ScreenState::Dimmed);
static_assert(target_state(15000, 15000) == ScreenState::Off);

struct Runtime {
    lv_display_t *display = nullptr;
    lv_indev_read_cb_t touch_read = nullptr;
    ScreenState state = ScreenState::Awake;
    int brightness = DEFAULT_BRIGHTNESS;
    uint32_t timeout_s = DEFAULT_TIMEOUT_S;
    bool touch_down = false;
    bool swallow_touch = false;
    bool pwr_down = false;
    uint32_t pwr_down_since = 0;
};

Runtime s;

const char *state_name(ScreenState state)
{
    switch (state) {
    case ScreenState::Awake:
        return "Awake";
    case ScreenState::Dimmed:
        return "Dimmed";
    case ScreenState::Off:
        return "Off";
    }
    return "?";
}

int brightness_for(ScreenState state)
{
    switch (state) {
    case ScreenState::Awake:
        return s.brightness;
    case ScreenState::Dimmed:
        return std::min(s.brightness, DIMMED_BRIGHTNESS);
    case ScreenState::Off:
        return 0;
    }
    return s.brightness;
}

void apply_brightness()
{
    const esp_err_t result = bsp_display_brightness_set(brightness_for(s.state));
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Set brightness failed: %s", esp_err_to_name(result));
    }
}

void transition(ScreenState next)
{
    if (next == s.state) {
        return;
    }
    ESP_LOGI(TAG, "Screen %s -> %s (idle %" PRIu32 " ms, timeout %" PRIu32 " s)",
             state_name(s.state), state_name(next), lv_display_get_inactive_time(s.display), s.timeout_s);
    const bool was_off = s.state == ScreenState::Off;
    s.state = next;
    apply_brightness();
    // A dark panel needs no frames, so LVGL stops rendering and flushing them.
    lv_display_enable_invalidation(s.display, next != ScreenState::Off);
    if (was_off) {
        lv_obj_invalidate(lv_display_get_screen_active(s.display));
    }
    if (next == ScreenState::Awake) {
        lv_display_trigger_activity(s.display);
    }
}

// Brookesia gestures poll the touch device directly instead of hit-testing
// objects, so the wake touch is hidden here, before LVGL or gestures see it.
void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    s.touch_read(indev, data);
    const bool down = data->state == LV_INDEV_STATE_PRESSED;
    if (down && !s.touch_down && s.state != ScreenState::Awake) {
        s.swallow_touch = true;
        transition(ScreenState::Awake);
    }
    s.touch_down = down;
    if (!down) {
        s.swallow_touch = false;
    }
    if (s.swallow_touch) {
        data->state = LV_INDEV_STATE_RELEASED;
        lv_display_trigger_activity(s.display);
    }
}

void idle_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const ScreenState target = target_state(lv_display_get_inactive_time(s.display), s.timeout_s * 1000);
    if (target > s.state) {
        transition(target);
    }
}

void pwr_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const bool down = gpio_get_level(PWR_KEY_GPIO) == PWR_KEY_PRESSED_LEVEL;
    if (down == s.pwr_down) {
        return;
    }
    s.pwr_down = down;
    if (down) {
        s.pwr_down_since = lv_tick_get();
        return;
    }
    const uint32_t held_ms = lv_tick_elaps(s.pwr_down_since);
    if (held_ms >= PWR_PRESS_MIN_MS && held_ms <= PWR_PRESS_MAX_MS) {
        transition(s.state == ScreenState::Awake ? ScreenState::Off : ScreenState::Awake);
    }
}

esp_err_t start_pwr_key()
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << PWR_KEY_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }
    const int level = gpio_get_level(PWR_KEY_GPIO);
    ESP_LOGI(TAG, "PWR key GPIO%d level at start: %d", PWR_KEY_GPIO, level);
    // A press still held from power-on has no start time, so its release never toggles.
    s.pwr_down = level == PWR_KEY_PRESSED_LEVEL;
    return lv_timer_create(pwr_timer_cb, PWR_POLL_MS, nullptr) != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
}

lv_indev_t *find_touch(lv_display_t *display)
{
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER && lv_indev_get_display(indev) == display) {
            return indev;
        }
    }
    return nullptr;
}

void load_settings()
{
    nvs_handle_t handle;
    const esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (result != ESP_OK) {
        if (result != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Open NVS failed: %s", esp_err_to_name(result));
        }
        return;
    }
    uint8_t brightness = 0;
    if (nvs_get_u8(handle, NVS_KEY_BRIGHTNESS, &brightness) == ESP_OK && is_valid_brightness(brightness)) {
        s.brightness = brightness;
    }
    uint32_t timeout = 0;
    if (nvs_get_u32(handle, NVS_KEY_TIMEOUT, &timeout) == ESP_OK && is_timeout_option(timeout)) {
        s.timeout_s = timeout;
    }
    nvs_close(handle);
}

template <typename Write>
void save_setting(const char *key, Write write)
{
    nvs_handle_t handle;
    esp_err_t result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        result = write(handle);
        if (result == ESP_OK) {
            result = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Save %s failed: %s", key, esp_err_to_name(result));
    }
}

} // namespace

esp_err_t start(lv_display_t *display)
{
    if (display == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s.display != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    lv_indev_t *touch = find_touch(display);
    if (touch == nullptr) {
        return ESP_ERR_NOT_FOUND;
    }
    s.display = display;
    load_settings();
    ESP_LOGI(TAG, "Loaded brightness %d%%, timeout %" PRIu32 " s", s.brightness, s.timeout_s);
    apply_brightness();

    s.touch_read = lv_indev_get_read_cb(touch);
    lv_indev_set_read_cb(touch, touch_read_cb);
    if (lv_timer_create(idle_timer_cb, IDLE_POLL_MS, nullptr) == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    const esp_err_t pwr_result = start_pwr_key();
    if (pwr_result != ESP_OK) {
        ESP_LOGW(TAG, "PWR key screen toggle unavailable: %s", esp_err_to_name(pwr_result));
    }
    return ESP_OK;
}

int brightness()
{
    return s.brightness;
}

void set_brightness(int percent)
{
    s.brightness = std::clamp(percent, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
    if (s.display != nullptr) {
        apply_brightness();
    }
}

void save_brightness()
{
    save_setting(NVS_KEY_BRIGHTNESS, [](nvs_handle_t handle) {
        return nvs_set_u8(handle, NVS_KEY_BRIGHTNESS, static_cast<uint8_t>(s.brightness));
    });
}

uint32_t timeout_s()
{
    return s.timeout_s;
}

void set_timeout_s(uint32_t seconds)
{
    if (!is_timeout_option(seconds)) {
        ESP_LOGW(TAG, "Ignore unknown timeout %" PRIu32 " s", seconds);
        return;
    }
    s.timeout_s = seconds;
    save_setting(NVS_KEY_TIMEOUT, [seconds](nvs_handle_t handle) {
        return nvs_set_u32(handle, NVS_KEY_TIMEOUT, seconds);
    });
}

} // namespace display_power
