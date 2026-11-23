/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <ctime>
#include <atomic>
#include <new>

#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_brookesia.hpp"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "Drawpanel.hpp"
#include "MusicPlayer.hpp"
#include "Settings.hpp"
#include "SpecAnalyzer.hpp"
#include "XiaozhiApp.hpp"
#include "bsp_board_extra.h"
#include "chat_history.h"
#include "display_perf_monitor.hpp"
#include "display_power.hpp"
#include "esp_brookesia_app_calculator.hpp"
#include "storage_service.h"
#include "system_status.hpp"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BrookesiaFirmware"
#include "esp_lib_utils.h"

using namespace esp_brookesia;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems::phone;

namespace {

constexpr int ROUND_STATUS_BAR_HEIGHT = 80;
constexpr int ROUND_STATUS_BAR_CONTENT_INSET = 126;
constexpr uint32_t BROOKESIA_LVGL_TASK_STACK_SIZE = 20 * 1024;
constexpr uint16_t BROOKESIA_LCD_DRAW_BUFFER_HEIGHT = 12;
constexpr size_t BROOKESIA_LCD_MAX_TRANSFER_SIZE =
    BSP_LCD_H_RES * BROOKESIA_LCD_DRAW_BUFFER_HEIGHT * BSP_LCD_BITS_PER_PIXEL / 8;

class BootLoadingUi {
public:
    static constexpr uint32_t STATUS_UPDATE_PERIOD_MS = 30;
    static constexpr uint32_t READY_HOLD_MS = 300;
    static constexpr uint32_t FADE_OUT_TIME_MS = 240;

    bool create()
    {
        root_ = lv_obj_create(lv_layer_top());
        if (root_ == nullptr) {
            return false;
        }

        spinner_ = lv_spinner_create(root_);
        title_ = lv_label_create(root_);
        phase_ = lv_label_create(root_);
        status_ = lv_label_create(root_);
        progress_ = lv_bar_create(root_);
        if (spinner_ == nullptr || title_ == nullptr || phase_ == nullptr ||
                status_ == nullptr || progress_ == nullptr) {
            destroy();
            return false;
        }

        // This is the same loading composition used by the P4 reference
        // firmware, fitted to the 466 x 466 round AMOLED.  All important
        // elements remain inside the panel's central visible chord.
        lv_obj_remove_style_all(root_);
        lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
        lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(root_, lv_color_hex(0x101417), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(
            root_,
            LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE
        );

        lv_obj_set_size(spinner_, 64, 64);
        lv_obj_align(spinner_, LV_ALIGN_CENTER, 0, -110);
        lv_obj_set_style_arc_width(spinner_, 5, LV_PART_MAIN);
        lv_obj_set_style_arc_width(spinner_, 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(spinner_, lv_color_hex(0x313B42), LV_PART_MAIN);
        lv_obj_set_style_arc_color(spinner_, lv_color_hex(0x27C1A8), LV_PART_INDICATOR);

        lv_label_set_text(title_, "ESP32-S3");
        lv_obj_set_style_text_font(title_, &lv_font_montserrat_32, LV_PART_MAIN);
        lv_obj_set_style_text_color(title_, lv_color_hex(0xF7F9FA), LV_PART_MAIN);
        lv_obj_align(title_, LV_ALIGN_CENTER, 0, -36);

        lv_label_set_text(phase_, "LOADING");
        lv_obj_set_style_text_font(phase_, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(phase_, lv_color_hex(0xF2B84B), LV_PART_MAIN);
        lv_obj_align(phase_, LV_ALIGN_CENTER, 0, 4);

        lv_obj_set_width(status_, lv_pct(84));
        lv_label_set_long_mode(status_, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(status_, &lv_font_montserrat_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(status_, lv_color_hex(0xA8B1B8), LV_PART_MAIN);
        lv_obj_align(status_, LV_ALIGN_CENTER, 0, 44);

        lv_obj_set_size(progress_, lv_pct(52), 7);
        lv_obj_align(progress_, LV_ALIGN_CENTER, 0, 82);
        lv_bar_set_range(progress_, 0, 100);
        lv_obj_set_style_radius(progress_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(progress_, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(progress_, lv_color_hex(0x2A343B), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(progress_, lv_color_hex(0x27C1A8), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(progress_, LV_OPA_COVER, LV_PART_INDICATOR);

        update_timer_ = lv_timer_create(onUpdateTimer, STATUS_UPDATE_PERIOD_MS, this);
        if (update_timer_ == nullptr) {
            destroy();
            return false;
        }

        setStageNow("Preparing display...", 8);
        lv_obj_invalidate(root_);
        return true;
    }

    void queueStage(const char *status, int progress)
    {
        if (status == nullptr || root_ == nullptr) {
            return;
        }

        if (progress < 0) {
            progress = 0;
        } else if (progress > 100) {
            progress = 100;
        }

        // Status strings passed during startup are string literals.  Atomics
        // let app_main publish progress without touching LVGL from its task;
        // the LVGL timer applies the pending state under the adapter's owner.
        pending_status_.store(status, std::memory_order_relaxed);
        pending_progress_.store(progress, std::memory_order_relaxed);
        pending_revision_.fetch_add(1, std::memory_order_release);
    }

    void showError(const char *status)
    {
        if (root_ == nullptr) {
            return;
        }

        stopUpdateTimer();
        lv_obj_add_flag(spinner_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(phase_, "STARTUP FAILED");
        lv_obj_set_style_text_color(phase_, lv_color_hex(0xF06A6A), LV_PART_MAIN);
        lv_obj_align(phase_, LV_ALIGN_CENTER, 0, 4);
        lv_label_set_text(status_, status != nullptr ? status : "Unable to finish startup.");
        lv_obj_set_style_text_color(status_, lv_color_hex(0xF7F9FA), LV_PART_MAIN);
        lv_bar_set_value(progress_, 100, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(progress_, lv_color_hex(0xF06A6A), LV_PART_INDICATOR);
        lv_obj_move_foreground(root_);
        lv_obj_invalidate(root_);
    }

    void finish()
    {
        if (root_ == nullptr) {
            return;
        }

        stopUpdateTimer();
        setStageNow("Ready", 100);

        lv_anim_t fade;
        lv_anim_init(&fade);
        lv_anim_set_var(&fade, root_);
        lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&fade, FADE_OUT_TIME_MS);
        lv_anim_set_delay(&fade, READY_HOLD_MS);
        lv_anim_set_exec_cb(&fade, [](void *object, int32_t opacity) {
            lv_obj_set_style_opa(static_cast<lv_obj_t *>(object), opacity, LV_PART_MAIN);
        });
        lv_anim_set_completed_cb(&fade, lv_obj_delete_anim_completed_cb);
        lv_anim_start(&fade);

        // LVGL owns and deletes the root when the fade completes.
        clearObjectPointers();
    }

    void attachToTopLayer(lv_display_t *display)
    {
        attachToLayer(display != nullptr ? lv_display_get_layer_top(display) : nullptr);
    }

    void attachToSystemLayer(lv_display_t *display)
    {
        attachToLayer(display != nullptr ? lv_display_get_layer_sys(display) : nullptr);
    }

    void destroy()
    {
        stopUpdateTimer();
        if (root_ != nullptr) {
            lv_obj_delete(root_);
        }
        clearObjectPointers();
    }

private:
    static void onUpdateTimer(lv_timer_t *timer)
    {
        auto *self = static_cast<BootLoadingUi *>(lv_timer_get_user_data(timer));
        if (self != nullptr) {
            self->applyPendingStage();
        }
    }

    void applyPendingStage()
    {
        const uint32_t revision = pending_revision_.load(std::memory_order_acquire);
        if (revision == applied_revision_) {
            return;
        }

        const char *status = pending_status_.load(std::memory_order_relaxed);
        const int progress = pending_progress_.load(std::memory_order_relaxed);
        if (status != nullptr) {
            setStageNow(status, progress);
        }
        applied_revision_ = revision;
    }

    void setStageNow(const char *status, int progress)
    {
        if (root_ == nullptr) {
            return;
        }
        lv_label_set_text(status_, status);
        lv_bar_set_value(progress_, progress, LV_ANIM_OFF);
        lv_obj_move_foreground(root_);
    }

    void stopUpdateTimer()
    {
        if (update_timer_ != nullptr) {
            lv_timer_delete(update_timer_);
            update_timer_ = nullptr;
        }
    }

    void clearObjectPointers()
    {
        root_ = nullptr;
        spinner_ = nullptr;
        title_ = nullptr;
        phase_ = nullptr;
        status_ = nullptr;
        progress_ = nullptr;
    }

    void attachToLayer(lv_obj_t *parent)
    {
        if (root_ == nullptr || parent == nullptr) {
            return;
        }
        if (lv_obj_get_parent(root_) != parent) {
            lv_obj_set_parent(root_, parent);
            lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
            lv_obj_align(root_, LV_ALIGN_CENTER, 0, 0);
        }
        lv_obj_move_foreground(root_);
    }

    lv_obj_t *root_ = nullptr;
    lv_obj_t *spinner_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *phase_ = nullptr;
    lv_obj_t *status_ = nullptr;
    lv_obj_t *progress_ = nullptr;
    lv_timer_t *update_timer_ = nullptr;
    std::atomic<const char *> pending_status_{nullptr};
    std::atomic<int> pending_progress_{0};
    std::atomic<uint32_t> pending_revision_{0};
    uint32_t applied_revision_ = 0;
};

void refresh_boot_loading(BootLoadingUi &loading, const char *status, int progress)
{
    ESP_UTILS_LOGI("Startup stage: %s (%d%%)", status, progress);
    loading.queueStage(status, progress);
}

class BootLoadingFailureGuard {
public:
    BootLoadingFailureGuard(BootLoadingUi &loading, lv_display_t *display)
        : _loading(loading), _display(display) {}

    ~BootLoadingFailureGuard()
    {
        if (!_armed) {
            return;
        }
        LvLockGuard gui_guard;
        _loading.attachToSystemLayer(_display);
        _loading.showError("Unable to finish startup. Please restart the device.");
    }

    BootLoadingFailureGuard(const BootLoadingFailureGuard &) = delete;
    BootLoadingFailureGuard &operator=(const BootLoadingFailureGuard &) = delete;

    void dismiss() { _armed = false; }

private:
    BootLoadingUi &_loading;
    lv_display_t *_display;
    bool _armed = true;
};

void set_display_inputs_enabled_locked(lv_display_t *display, bool enabled)
{
    for (lv_indev_t *indev = lv_indev_get_next(nullptr); indev != nullptr;
            indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_display(indev) == display) {
            lv_indev_enable(indev, enabled);
        }
    }
}

class DisplayInputGuard {
public:
    explicit DisplayInputGuard(lv_display_t *display) : _display(display), _active(false)
    {
        if (_display != nullptr && esp_lv_adapter_lock(-1) == ESP_OK) {
            set_display_inputs_enabled_locked(_display, false);
            esp_lv_adapter_unlock();
            _active = true;
        }
    }

    ~DisplayInputGuard() { enable(); }

    DisplayInputGuard(const DisplayInputGuard &) = delete;
    DisplayInputGuard &operator=(const DisplayInputGuard &) = delete;

    void enable()
    {
        if (_active && esp_lv_adapter_lock(-1) == ESP_OK) {
            set_display_inputs_enabled_locked(_display, true);
            esp_lv_adapter_unlock();
            _active = false;
        }
    }

private:
    lv_display_t *_display;
    bool _active;
};

void round_display_flush_area(lv_event_t *event)
{
    auto *area = static_cast<lv_area_t *>(lv_event_get_param(event));
    if (area == nullptr) {
        return;
    }

    // CO5300 QSPI transfers must start and end on an even pixel boundary.
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

lv_display_t *start_brookesia_display()
{
    esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter_config.task_stack_size = BROOKESIA_LVGL_TASK_STACK_SIZE;
    /* Brookesia runs application entry points directly from LVGL event
     * callbacks. Some applications access SPIFFS/NVS there; ESP-IDF briefly
     * disables the flash/PSRAM cache for those operations, so the executing
     * task's stack must remain in internal RAM. An external LVGL stack trips
     * esp_task_stack_is_sane_cache_disabled() before MusicPlayer can create
     * its UI. */
    adapter_config.stack_in_psram = false;
    if (esp_lv_adapter_init(&adapter_config) != ESP_OK) {
        return nullptr;
    }

    // esp_lcd's SPI panel IO does not mark queued color transactions with
    // SPI_TRANS_DMA_USE_PSRAM. A PSRAM draw buffer therefore needs a large
    // temporary internal-DMA copy for every flush and exhausts SRAM once all
    // Brookesia apps are installed. Use a small internal single-buffer SPI
    // profile. Twelve rows reduce a full 466-line refresh from 47 to 39
    // transfers while keeping the total internal-RAM cost below the previous
    // 10-row/full-frame-max-transfer setup.
    const bsp_display_config_t panel_config = {
        .max_transfer_sz = BROOKESIA_LCD_MAX_TRANSFER_SIZE,
    };
    esp_lcd_panel_handle_t panel = nullptr;
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    if (bsp_display_new(&panel_config, &panel, &panel_io) != ESP_OK) {
        return nullptr;
    }

    esp_lv_adapter_display_config_t display_config =
        ESP_LV_ADAPTER_DISPLAY_SPI_WITHOUT_PSRAM_DEFAULT_CONFIG(
            panel,
            panel_io,
            BSP_LCD_H_RES,
            BSP_LCD_V_RES,
            ESP_LV_ADAPTER_ROTATE_0
        );
    display_config.profile.buffer_height = BROOKESIA_LCD_DRAW_BUFFER_HEIGHT;
    lv_display_t *display = esp_lv_adapter_register_display(&display_config);
    if (display == nullptr) {
        return nullptr;
    }
    lv_display_add_event_cb(display, round_display_flush_area, LV_EVENT_INVALIDATE_AREA, nullptr);

    const bsp_display_cfg_t board_config = {
        .lv_adapter_cfg = adapter_config,
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    esp_lcd_touch_handle_t touch = nullptr;
    if (bsp_touch_new(&board_config, &touch) != ESP_OK) {
        return nullptr;
    }
    const esp_lv_adapter_touch_config_t touch_config =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch);
    if (esp_lv_adapter_register_touch(&touch_config) == nullptr) {
        return nullptr;
    }

    if (bsp_display_brightness_init() != ESP_OK) {
        return nullptr;
    }
    // Match the P4 reference startup sequence: keep the panel dark until the
    // first complete loading frame is ready, avoiding a flash of an empty LVGL
    // screen between panel initialization and UI creation.
    if (bsp_display_backlight_off() != ESP_OK || esp_lv_adapter_start() != ESP_OK) {
        return nullptr;
    }
    return display;
}

Stylesheet make_round_display_stylesheet()
{
    Stylesheet stylesheet = STYLESHEET_DEFAULT_DARK;
    stylesheet.core.name = "466 Round Dark";

    // On a round panel, the top-left and top-right corners do not exist.  Keep
    // the status widgets inside the chord that is visible around y=40 instead
    // of placing them at the rectangular display edges.
    auto &status_bar = stylesheet.display.status_bar.data;
    status_bar.main.size = StyleSize::RECT_W_PERCENT(100, ROUND_STATUS_BAR_HEIGHT);
    status_bar.main.background_color = StyleColor::COLOR(0x1A1A1A);
    status_bar.main.text_font = StyleFont::SIZE(18);
    status_bar.flags.enable_main_size_min = 0;
    status_bar.flags.enable_main_size_max = 0;
    status_bar.flags.enable_battery_label = 1;
    status_bar.icon_common_size = StyleSize::SQUARE(24);

    for (int i = 0; i < status_bar.area.num; ++i) {
        status_bar.area.data[i].layout_column_start_offset = ROUND_STATUS_BAR_CONTENT_INSET;
        status_bar.area.data[i].layout_column_pad = 4;
    }

    // Keep launcher icons and their labels inside the round panel's visible
    // chords.  A 360 x 300 table with 140 px cells resolves to a centered 2 x 2
    // grid, while retaining the native 112 x 112 launcher artwork.
    auto &launcher = stylesheet.display.app_launcher.data;
    launcher.table.size = StyleSize::RECT(360, 300);
    launcher.icon.main.size = StyleSize::SQUARE(140);
    launcher.icon.image.default_size = StyleSize::SQUARE(112);
    launcher.icon.image.press_size = StyleSize::SQUARE(100);

    return stylesheet;
}

esp_err_t init_nvs()
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), ESP_UTILS_LOG_TAG, "Erase NVS failed");
        result = nvs_flash_init();
    }
    return result;
}

void update_status_bar_clock(lv_timer_t *timer)
{
    auto *phone = static_cast<Phone *>(lv_timer_get_user_data(timer));
    if (phone == nullptr) {
        return;
    }

    time_t now = 0;
    struct tm time_info = {};
    time(&now);
    localtime_r(&now, &time_info);

    auto *status_bar = phone->getDisplay().getStatusBar();
    if (status_bar == nullptr) {
        return;
    }
    status_bar->setClock(time_info.tm_hour, time_info.tm_min);
}

}  // namespace

extern "C" void app_main(void)
{
    ESP_UTILS_LOGI("Starting ESP32-S3-Touch-AMOLED-1.75C Brookesia firmware");
    lv_display_t *display = start_brookesia_display();
    ESP_UTILS_CHECK_NULL_EXIT(display, "Start display failed");

    LvLock::registerCallbacks([](int timeout_ms) {
        // Keep the signed timeout intact: esp_lv_adapter_lock() treats every
        // negative value (Brookesia uses -1) as an infinite wait.
        return esp_lv_adapter_lock(timeout_ms) == ESP_OK;
    }, []() {
        esp_lv_adapter_unlock();
        return true;
    });

    // Do not allow touches to reach the partially constructed Brookesia tree.
    // The loading root is also clickable, so it continues shielding the home
    // screen during the short Ready hold and fade after input is re-enabled.
    DisplayInputGuard input_guard(display);
    BootLoadingUi boot_loading;
    bool boot_loading_created = false;
    {
        LvLockGuard gui_guard;
        boot_loading_created = boot_loading.create();
        if (boot_loading_created) {
            lv_refr_now(display);
        }
    }
    if (!boot_loading_created) {
        ESP_UTILS_LOGW("P4-style boot loading UI is unavailable");
    } else {
        ESP_UTILS_LOGI("P4-style boot loading UI started");
    }

    // Give the SPI display one complete frame before revealing the AMOLED.
    vTaskDelay(pdMS_TO_TICKS(50));
    const esp_err_t backlight_result = bsp_display_backlight_on();
    if (backlight_result != ESP_OK) {
        ESP_UTILS_LOGW("Turn display on failed: %s", esp_err_to_name(backlight_result));
    }

    BootLoadingFailureGuard boot_failure_guard(boot_loading, display);

#if CONFIG_BROOKESIA_DISPLAY_PERF_LOG
    const esp_err_t perf_result = display_perf_monitor_start(display);
    if (perf_result != ESP_OK) {
        ESP_UTILS_LOGW("Display performance monitor unavailable: %s", esp_err_to_name(perf_result));
    }
#endif

    refresh_boot_loading(boot_loading, "Initializing settings...", 14);
    ESP_UTILS_CHECK_ERROR_EXIT(init_nvs(), "Initialize NVS failed");

    ESP_UTILS_CHECK_ERROR_EXIT(storage_service_init(), "Initialize storage service failed");
    const esp_err_t chat_history_result = chat_history_init();
    if (chat_history_result != ESP_OK) {
        ESP_UTILS_LOGW(
            "AIChats SD history unavailable: %s", esp_err_to_name(chat_history_result)
        );
    }

    refresh_boot_loading(boot_loading, "Mounting internal storage...", 34);
    const esp_err_t storage_result = bsp_spiffs_mount();
    if (storage_result != ESP_OK && storage_result != ESP_ERR_INVALID_STATE) {
        ESP_UTILS_LOGW("SPIFFS mount skipped: %s", esp_err_to_name(storage_result));
    }

    // Settings exposes the target-board volume control.  Keep the shell usable
    // if an audio peripheral is unavailable during early hardware bring-up.
    refresh_boot_loading(boot_loading, "Starting audio...", 42);
    const esp_err_t audio_result = bsp_extra_codec_init();
    if (audio_result != ESP_OK) {
        ESP_UTILS_LOGW("Audio initialization skipped: %s", esp_err_to_name(audio_result));
    }

    refresh_boot_loading(boot_loading, "Building home screen...", 52);
    Phone *phone = new (std::nothrow) Phone();
    ESP_UTILS_CHECK_NULL_EXIT(phone, "Create phone failed");
    StatusBar *status_bar = nullptr;

    const Stylesheet round_stylesheet = make_round_display_stylesheet();
    {
        // Keep begin() atomic, then reparent the loading root because
        // Phone::begin() replaces LVGL's system layer.
        LvLockGuard gui_guard;
        ESP_UTILS_CHECK_FALSE_EXIT(
            phone->addStylesheet(round_stylesheet), "Add round display stylesheet failed"
        );
        ESP_UTILS_CHECK_FALSE_EXIT(
            phone->activateStylesheet(round_stylesheet),
            "Activate round display stylesheet failed"
        );
        ESP_UTILS_CHECK_FALSE_EXIT(phone->begin(), "Begin phone failed");
        boot_loading.attachToSystemLayer(display);
    }

    refresh_boot_loading(boot_loading, "Loading application registry...", 62);
    std::vector<systems::base::Manager::RegistryAppInfo> registry_apps;
    {
        LvLockGuard gui_guard;
        ESP_UTILS_CHECK_FALSE_EXIT(
            phone->initAppFromRegistry(registry_apps), "Initialize registry apps failed"
        );
        boot_loading.attachToSystemLayer(display);
    }

    auto install_app = [phone, display, &boot_loading](
                           systems::base::App *app, const char *name
                       ) {
        int app_id = systems::base::App::APP_ID_MIN - 1;
        ESP_UTILS_LOGI("Installing %s", name);
        {
            LvLockGuard gui_guard;
            // Registry entries are typed as base::App. Use the manager's base
            // install path; all registered products here are phone apps and the
            // phone display performs their normal phone-specific setup.
            app_id = phone->getManager().installApp(app);
            boot_loading.attachToSystemLayer(display);
        }
        if (app_id < systems::base::App::APP_ID_MIN) {
            ESP_UTILS_LOGE("Install %s failed", name);
            return false;
        }
        ESP_UTILS_LOGI("Installed %s (id: %d)", name, app_id);
        // Let the LVGL task animate the spinner and apply queued progress before
        // the next application obtains the recursive adapter lock.
        taskYIELD();
        return true;
    };

    for (auto &[name, app] : registry_apps) {
        ESP_UTILS_CHECK_FALSE_EXIT(
            install_app(app.get(), name.c_str()), "Install registry app failed"
        );
    }

    refresh_boot_loading(boot_loading, "Loading tools...", 72);
    ESP_UTILS_CHECK_FALSE_EXIT(
        install_app(apps::Calculator::requestInstance(), "Calculator"),
        "Install Calculator failed"
    );
    ESP_UTILS_CHECK_FALSE_EXIT(
        install_app(apps::Drawpanel::requestInstance(), "DrawPanel"),
        "Install DrawPanel failed"
    );
    ESP_UTILS_CHECK_FALSE_EXIT(
        install_app(apps::SpecAnalyzer::requestInstance(), "SpecAnalyzer"),
        "Install SpecAnalyzer failed"
    );

    refresh_boot_loading(boot_loading, "Loading media...", 82);
    ESP_UTILS_CHECK_FALSE_EXIT(
        install_app(apps::MusicPlayer::requestInstance(), "MusicPlayer"),
        "Install MusicPlayer failed"
    );

    refresh_boot_loading(boot_loading, "Loading system apps...", 91);
    ESP_UTILS_CHECK_FALSE_EXIT(
        install_app(apps::Settings::requestInstance(), "Settings"),
        "Install Settings failed"
    );
    ESP_UTILS_CHECK_FALSE_EXIT(
        install_app(apps::XiaozhiApp::requestInstance(), "AIChats"),
        "Install AIChats failed"
    );

    refresh_boot_loading(boot_loading, "Starting status services...", 96);
    {
        LvLockGuard gui_guard;
        lv_timer_create(update_status_bar_clock, 1000, phone);
        status_bar = phone->getDisplay().getStatusBar();

        // The adapter starts before Brookesia builds its object tree.  Invalidate
        // the completed screen once so the first frame is not dependent on the
        // collection of small dirty regions produced during app installation.
        lv_obj_invalidate(lv_display_get_screen_active(display));
        boot_loading.attachToSystemLayer(display);
    }

    // Start the monitor only after releasing the initialization lock.  Its
    // worker updates LVGL immediately and would otherwise time out waiting for
    // the main task's still-held recursive lock during the first sample.
    const esp_err_t status_result = brookesia::system_status::start(status_bar, 1000);
    if (status_result != ESP_OK) {
        ESP_UTILS_LOGW(
            "Battery/Wi-Fi status monitor unavailable: %s", esp_err_to_name(status_result)
        );
    }

    {
        LvLockGuard gui_guard;
        boot_loading.attachToSystemLayer(display);
        boot_loading.finish();
    }
    boot_failure_guard.dismiss();
    ESP_UTILS_CHECK_ERROR_EXIT(
        esp_lv_adapter_refresh_now(display), "Refresh initial Brookesia frame failed"
    );

    // Wi-Fi callbacks and the status bar service are now initialized, so user
    // interaction can safely enter Settings or launch an application.
    input_guard.enable();
    {
        LvLockGuard gui_guard;
        const esp_err_t display_power_result = display_power::start(display);
        if (display_power_result != ESP_OK) {
            ESP_UTILS_LOGW(
                "Screen timeout unavailable: %s", esp_err_to_name(display_power_result)
            );
        }
    }
    ESP_UTILS_LOGI("Brookesia firmware is ready");
}
