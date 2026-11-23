#include "display_power.hpp"

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
    ScreenState state = ScreenState::Awake;
    int brightness = DEFAULT_BRIGHTNESS;
    uint32_t timeout_s = DEFAULT_TIMEOUT_S;
    lv_obj_t *shield = nullptr;
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

void transition(ScreenState next);

void shield_event_cb(lv_event_t *e)
{
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        transition(ScreenState::Awake);
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        // The PWR key can turn the screen off again while this touch is held.
        if (s.state == ScreenState::Awake) {
            lv_obj_delete_async(s.shield);
            s.shield = nullptr;
        }
        break;
    default:
        break;
    }
}

// Swallows the wake touch so it never reaches the app under it. LVGL hit-tests
// the system layer first, and Brookesia keeps its status and navigation bars there.
void ensure_shield()
{
    if (s.shield != nullptr) {
        return;
    }
    s.shield = lv_obj_create(lv_display_get_layer_sys(s.display));
    lv_obj_remove_style_all(s.shield);
    lv_obj_set_size(s.shield, lv_pct(100), lv_pct(100));
    lv_obj_align(s.shield, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_foreground(s.shield);
    lv_obj_add_flag(s.shield, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s.shield, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s.shield, shield_event_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(s.shield, shield_event_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(s.shield, shield_event_cb, LV_EVENT_PRESS_LOST, nullptr);
}

void transition(ScreenState next)
{
    if (next == s.state) {
        return;
    }
    ESP_LOGI(TAG, "Screen %s -> %s (idle %" PRIu32 " ms, timeout %" PRIu32 " s)",
             state_name(s.state), state_name(next), lv_display_get_inactive_time(s.display), s.timeout_s);
    s.state = next;
    apply_brightness();
    if (next == ScreenState::Awake) {
        lv_display_trigger_activity(s.display);
    } else {
        ensure_shield();
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

void toggle_screen()
{
    if (s.state == ScreenState::Awake) {
        transition(ScreenState::Off);
        return;
    }
    transition(ScreenState::Awake);
    if (s.shield != nullptr) {
        lv_obj_delete(s.shield);
        s.shield = nullptr;
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
        toggle_screen();
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
    const int idle_level = gpio_get_level(PWR_KEY_GPIO);
    ESP_LOGI(TAG, "PWR key GPIO3 idle level: %d", idle_level);
    // The active-high polarity is unverified. A key that already reads pressed
    // would turn every gap between real presses into a toggle, so stay off.
    if (idle_level == PWR_KEY_PRESSED_LEVEL) {
        ESP_LOGW(TAG, "PWR key reads pressed at start, PWR screen toggle disabled");
        return ESP_ERR_INVALID_STATE;
    }
    return lv_timer_create(pwr_timer_cb, PWR_POLL_MS, nullptr) != nullptr ? ESP_OK : ESP_ERR_NO_MEM;
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
    s.display = display;
    load_settings();
    ESP_LOGI(TAG, "Loaded brightness %d%%, timeout %" PRIu32 " s", s.brightness, s.timeout_s);
    apply_brightness();

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
