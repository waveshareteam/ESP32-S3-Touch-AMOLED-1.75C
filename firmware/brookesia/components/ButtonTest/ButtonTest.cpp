/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ButtonTest.hpp"

#include <algorithm>
#include <memory>

#include "bsp/esp-bsp.h"
#include "esp_brookesia.hpp"
#include "esp_err.h"
#include "esp_lib_utils.h"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:ButtonTest"

LV_IMG_DECLARE(img_app_button_test);

namespace esp_brookesia::apps {

namespace {

constexpr char APP_NAME[] = "Button Test";

constexpr uint32_t COLOR_BACKGROUND_TOP = 0x07111F;
constexpr uint32_t COLOR_BACKGROUND_BOTTOM = 0x111B3A;
constexpr uint32_t COLOR_CARD = 0x151F35;
constexpr uint32_t COLOR_CARD_BORDER = 0x334467;
constexpr uint32_t COLOR_TEXT = 0xF4F7FF;
constexpr uint32_t COLOR_MUTED = 0xAAB8D1;
constexpr uint32_t COLOR_RELEASED = 0x34435D;
constexpr uint32_t COLOR_PRESSED = 0x11A98A;
constexpr uint32_t COLOR_ERROR = 0xD44C5C;
constexpr uint32_t COLOR_PASS = 0x66E0B5;
constexpr uint32_t COLOR_WAITING = 0x7E8CA6;
constexpr uint32_t COLOR_WARNING = 0xF3BA63;
constexpr int REQUIRED_STABLE_SAMPLES = 3;

enum class TestPhase : uint8_t {
    WAIT_RELEASE,
    WAIT_PRESS,
    WAIT_FINAL_RELEASE,
    PASSED,
};

struct ButtonSequence {
    TestPhase phase = TestPhase::WAIT_RELEASE;
    int candidate_pressed = -1;
    int stable_samples = 0;
};

void makePassive(lv_obj_t *object)
{
    if (object == nullptr) {
        return;
    }
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

bool updateButtonSequence(ButtonSequence &sequence, int raw_level, bool active_high)
{
    if (sequence.phase == TestPhase::PASSED) {
        return false;
    }
    if (raw_level < 0) {
        sequence = {};
        return false;
    }

    const bool pressed = active_high ? raw_level == 1 : raw_level == 0;
    const int pressed_value = pressed ? 1 : 0;
    if (pressed_value != sequence.candidate_pressed) {
        sequence.candidate_pressed = pressed_value;
        sequence.stable_samples = 1;
    } else if (sequence.stable_samples < REQUIRED_STABLE_SAMPLES) {
        ++sequence.stable_samples;
    }

    if (sequence.stable_samples < REQUIRED_STABLE_SAMPLES) {
        return false;
    }

    switch (sequence.phase) {
    case TestPhase::WAIT_RELEASE:
        if (!pressed) {
            sequence.phase = TestPhase::WAIT_PRESS;
        }
        break;
    case TestPhase::WAIT_PRESS:
        if (pressed) {
            sequence.phase = TestPhase::WAIT_FINAL_RELEASE;
        }
        break;
    case TestPhase::WAIT_FINAL_RELEASE:
        if (!pressed) {
            sequence.phase = TestPhase::PASSED;
            return true;
        }
        break;
    case TestPhase::PASSED:
        break;
    }

    return false;
}

} // namespace

ButtonTest *ButtonTest::_instance = nullptr;

ButtonTest *ButtonTest::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new ButtonTest();
    }
    return _instance;
}

ButtonTest::ButtonTest()
    : App(APP_NAME, &img_app_button_test, true, false, false)
{
}

ButtonTest::~ButtonTest()
{
    stopWorker();
}

bool ButtonTest::init()
{
    // Hardware setup is intentionally deferred until run().  A broken input
    // must remain visible as READ ERROR instead of preventing Brookesia boot.
    return true;
}

bool ButtonTest::deinit()
{
    stopWorker();
    releaseUi();
    return true;
}

bool ButtonTest::run()
{
    ESP_UTILS_CHECK_FALSE_RETURN(_worker_task.load() == nullptr, false, "Worker is already running");

    _visual_area = getVisualArea();
    _visual_width = lv_area_get_width(&_visual_area);
    _visual_height = lv_area_get_height(&_visual_area);
    ESP_UTILS_CHECK_FALSE_RETURN(
        _visual_width >= 400 && _visual_height >= 400,
        false,
        "Visual area is too small"
    );

    ESP_UTILS_CHECK_FALSE_RETURN(createUi(), false, "Create Button Test UI failed");

    _pwr_level.store(-1);
    _boot_level.store(-1);
    _pwr_passed.store(false);
    _boot_passed.store(false);
    _pwr_configured = false;
    _boot_configured = false;
    updateUi();

    _paused.store(false);
    _running.store(true);

    TaskHandle_t task = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        workerTask, "button_test", 4096, this, 4, &task, 1
    );
    if (created != pdPASS) {
        _running.store(false);
        releaseUi();
        ESP_UTILS_LOGE("Create Button Test worker failed");
        return false;
    }
    _worker_task.store(task);
    return true;
}

bool ButtonTest::back()
{
    return notifyCoreClosed();
}

bool ButtonTest::close()
{
    stopWorker();
    releaseUi();
    return true;
}

bool ButtonTest::pause()
{
    _paused.store(true);
    if (_ui_timer != nullptr) {
        lv_timer_pause(_ui_timer);
    }
    return true;
}

bool ButtonTest::resume()
{
    _paused.store(false);
    if (_ui_timer != nullptr) {
        lv_timer_reset(_ui_timer);
        lv_timer_resume(_ui_timer);
    }
    const TaskHandle_t task = _worker_task.load();
    if (task != nullptr) {
        xTaskNotifyGive(task);
    }
    return true;
}

bool ButtonTest::configureBootInput()
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        ESP_UTILS_LOGE("Configure BOOT GPIO0 failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

bool ButtonTest::configurePwrInput()
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << PWR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        ESP_UTILS_LOGE("Configure PWR GPIO3 failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

bool ButtonTest::createUi()
{
    _screen = lv_screen_active();
    ESP_UTILS_CHECK_NULL_RETURN(_screen, false, "Get active screen failed");

    makePassive(_screen);
    lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(COLOR_BACKGROUND_TOP), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(_screen, lv_color_hex(COLOR_BACKGROUND_BOTTOM), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(_screen, LV_GRAD_DIR_VER, LV_PART_MAIN);

    _title_label = lv_label_create(_screen);
    makePassive(_title_label);
    lv_label_set_text(_title_label, "Button Test");
    lv_obj_set_style_text_font(_title_label, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_text_color(_title_label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);

    _subtitle_label = lv_label_create(_screen);
    makePassive(_subtitle_label);
    lv_label_set_text(_subtitle_label, "Check both physical keys");
    lv_obj_set_style_text_font(_subtitle_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(_subtitle_label, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);

    _pwr_widgets = createButtonCard(_screen, "PWR", "ESP32-S3 / GPIO3");
    _boot_widgets = createButtonCard(_screen, "BOOT", "ESP32-S3 / GPIO0");

    _result_label = lv_label_create(_screen);
    makePassive(_result_label);
    lv_obj_set_width(_result_label, _visual_width - 80);
    lv_obj_set_style_text_align(_result_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_result_label, &lv_font_montserrat_18, LV_PART_MAIN);

    _warning_label = lv_label_create(_screen);
    makePassive(_warning_label);
    lv_label_set_text(_warning_label, "Tap PWR briefly - do not hold");
    lv_obj_set_width(_warning_label, _visual_width - 120);
    lv_obj_set_style_text_align(_warning_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(_warning_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(_warning_label, lv_color_hex(COLOR_WARNING), LV_PART_MAIN);

    layoutUi();
    _ui_timer = lv_timer_create(uiTimerCallback, UI_PERIOD_MS, this);
    return _ui_timer != nullptr;
}

ButtonTest::ButtonWidgets ButtonTest::createButtonCard(
    lv_obj_t *parent,
    const char *name,
    const char *pin_name
)
{
    ButtonWidgets widgets = {};

    widgets.card = lv_obj_create(parent);
    makePassive(widgets.card);
    lv_obj_set_style_pad_all(widgets.card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(widgets.card, 22, LV_PART_MAIN);
    lv_obj_set_style_bg_color(widgets.card, lv_color_hex(COLOR_CARD), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widgets.card, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(widgets.card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(widgets.card, lv_color_hex(COLOR_CARD_BORDER), LV_PART_MAIN);
    lv_obj_set_style_shadow_color(widgets.card, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(widgets.card, 14, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(widgets.card, LV_OPA_30, LV_PART_MAIN);

    lv_obj_t *name_label = lv_label_create(widgets.card);
    makePassive(name_label);
    lv_label_set_text(name_label, name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);
    lv_obj_align(name_label, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *pin_label = lv_label_create(widgets.card);
    makePassive(pin_label);
    lv_label_set_text(pin_label, pin_name);
    lv_obj_set_style_text_font(pin_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(pin_label, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
    lv_obj_align(pin_label, LV_ALIGN_TOP_MID, 0, 45);

    widgets.state_panel = lv_obj_create(widgets.card);
    makePassive(widgets.state_panel);
    lv_obj_set_size(widgets.state_panel, 142, 54);
    lv_obj_align(widgets.state_panel, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_pad_all(widgets.state_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(widgets.state_panel, 15, LV_PART_MAIN);
    lv_obj_set_style_border_width(widgets.state_panel, 0, LV_PART_MAIN);

    widgets.state_label = lv_label_create(widgets.state_panel);
    makePassive(widgets.state_label);
    lv_obj_center(widgets.state_label);
    lv_obj_set_style_text_font(widgets.state_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(widgets.state_label, lv_color_hex(COLOR_TEXT), LV_PART_MAIN);

    widgets.raw_label = lv_label_create(widgets.card);
    makePassive(widgets.raw_label);
    lv_obj_set_style_text_font(widgets.raw_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(widgets.raw_label, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
    lv_obj_align(widgets.raw_label, LV_ALIGN_TOP_MID, 0, 134);

    widgets.pass_label = lv_label_create(widgets.card);
    makePassive(widgets.pass_label);
    lv_obj_set_style_text_font(widgets.pass_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(widgets.pass_label, LV_ALIGN_TOP_MID, 0, 160);

    return widgets;
}

void ButtonTest::layoutUi()
{
    const int shortest_side = std::min(_visual_width, _visual_height);
    const int card_gap = 12;
    const int card_width = std::min(184, (shortest_side - 68 - card_gap) / 2);
    const int card_height = 190;
    const int cards_width = (card_width * 2) + card_gap;
    const int cards_left = (_visual_width - cards_width) / 2;
    const int cards_top = 116;

    lv_obj_align(_title_label, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_align(_subtitle_label, LV_ALIGN_TOP_MID, 0, 69);

    lv_obj_set_size(_pwr_widgets.card, card_width, card_height);
    lv_obj_set_pos(_pwr_widgets.card, cards_left, cards_top);
    lv_obj_set_size(_boot_widgets.card, card_width, card_height);
    lv_obj_set_pos(_boot_widgets.card, cards_left + card_width + card_gap, cards_top);

    lv_obj_align(_result_label, LV_ALIGN_TOP_MID, 0, 330);
    lv_obj_align(_warning_label, LV_ALIGN_TOP_MID, 0, 365);
}

void ButtonTest::updateUi()
{
    const int pwr_level = _pwr_level.load();
    const int boot_level = _boot_level.load();
    const bool pwr_passed = _pwr_passed.load();
    const bool boot_passed = _boot_passed.load();

    // PWR is active high after the board's BSS138 conditioning circuit.
    updateButtonCard(_pwr_widgets, pwr_level, true, pwr_passed);
    // BOOT is directly pulled up and shorts GPIO0 to ground when pressed.
    updateButtonCard(_boot_widgets, boot_level, false, boot_passed);

    if (pwr_passed && boot_passed) {
        lv_label_set_text(_result_label, "ALL BUTTONS PASSED");
        lv_obj_set_style_text_color(_result_label, lv_color_hex(COLOR_PASS), LV_PART_MAIN);
    } else if (pwr_level < 0 || boot_level < 0) {
        lv_label_set_text(_result_label, "CHECK INPUT CONNECTIONS");
        lv_obj_set_style_text_color(_result_label, lv_color_hex(COLOR_ERROR), LV_PART_MAIN);
    } else {
        lv_label_set_text(_result_label, "SHORT-PRESS PWR AND BOOT");
        lv_obj_set_style_text_color(_result_label, lv_color_hex(COLOR_MUTED), LV_PART_MAIN);
    }
}

void ButtonTest::updateButtonCard(
    ButtonWidgets &widgets,
    int raw_level,
    bool active_high,
    bool passed
)
{
    if (raw_level < 0) {
        lv_label_set_text(widgets.state_label, "READ ERROR");
        lv_label_set_text(widgets.raw_label, "RAW --");
        lv_obj_set_style_bg_color(widgets.state_panel, lv_color_hex(COLOR_ERROR), LV_PART_MAIN);
    } else {
        const bool pressed = active_high ? raw_level == 1 : raw_level == 0;
        lv_label_set_text(widgets.state_label, pressed ? "PRESSED" : "RELEASED");
        lv_label_set_text(widgets.raw_label, raw_level == 1 ? "RAW HIGH" : "RAW LOW");
        lv_obj_set_style_bg_color(
            widgets.state_panel,
            lv_color_hex(pressed ? COLOR_PRESSED : COLOR_RELEASED),
            LV_PART_MAIN
        );
    }

    lv_label_set_text(widgets.pass_label, passed ? "PASS LATCHED" : "WAITING");
    lv_obj_set_style_text_color(
        widgets.pass_label,
        lv_color_hex(passed ? COLOR_PASS : COLOR_WAITING),
        LV_PART_MAIN
    );
}

void ButtonTest::uiTimerCallback(lv_timer_t *timer)
{
    auto *app = static_cast<ButtonTest *>(lv_timer_get_user_data(timer));
    if (app != nullptr) {
        app->updateUi();
    }
}

void ButtonTest::workerTask(void *arg)
{
    static_cast<ButtonTest *>(arg)->workerLoop();
}

void ButtonTest::workerLoop()
{
    int previous_pwr_level = -2;
    int previous_boot_level = -2;
    ButtonSequence pwr_sequence = {};
    ButtonSequence boot_sequence = {};
    TickType_t next_retry = xTaskGetTickCount();

    while (_running.load()) {
        if (_paused.load()) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100));
            continue;
        }

        const TickType_t now = xTaskGetTickCount();
        if ((!_pwr_configured || !_boot_configured) &&
                static_cast<int32_t>(now - next_retry) >= 0) {
            if (!_pwr_configured) {
                _pwr_configured = configurePwrInput();
            }
            if (!_boot_configured) {
                _boot_configured = configureBootInput();
            }
            next_retry = now + pdMS_TO_TICKS(INPUT_RETRY_PERIOD_MS);
        }

        const int pwr_level = _pwr_configured ? gpio_get_level(PWR_GPIO) : -1;
        const int boot_level = _boot_configured ? gpio_get_level(BOOT_GPIO) : -1;
        _pwr_level.store(pwr_level);
        _boot_level.store(boot_level);

        // A valid factory-test pass requires a stable released -> pressed ->
        // released sequence. This prevents a stuck active level from passing.
        if (updateButtonSequence(pwr_sequence, pwr_level, true)) {
            _pwr_passed.store(true);
        }
        if (updateButtonSequence(boot_sequence, boot_level, false)) {
            _boot_passed.store(true);
        }

        if (pwr_level != previous_pwr_level) {
            ESP_UTILS_LOGI("PWR GPIO3 level changed: %d", pwr_level);
            previous_pwr_level = pwr_level;
        }
        if (boot_level != previous_boot_level) {
            ESP_UTILS_LOGI("BOOT GPIO0 level changed: %d", boot_level);
            previous_boot_level = boot_level;
        }

        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }

    _worker_task.store(nullptr);
    vTaskDelete(nullptr);
}

void ButtonTest::releaseUi()
{
    if (_ui_timer != nullptr) {
        lv_timer_delete(_ui_timer);
        _ui_timer = nullptr;
    }

    // Brookesia owns and deletes the application screen and all child objects.
    _screen = nullptr;
    _title_label = nullptr;
    _subtitle_label = nullptr;
    _pwr_widgets = {};
    _boot_widgets = {};
    _result_label = nullptr;
    _warning_label = nullptr;
}

void ButtonTest::stopWorker()
{
    _running.store(false);
    TaskHandle_t task = _worker_task.load();
    if (task == nullptr) {
        return;
    }

    xTaskNotifyGive(task);
    for (int i = 0; i < 250 && _worker_task.load() != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (_worker_task.load() != nullptr) {
        // The worker never touches LVGL and owns its self-delete path. Leaving
        // it to finish avoids racing vTaskDelete() against natural task exit.
        ESP_UTILS_LOGW("Button Test worker is still leaving a bounded I2C call");
    }
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, ButtonTest, APP_NAME, []() {
    return std::shared_ptr<ButtonTest>(
        ButtonTest::requestInstance(), [](ButtonTest *) {}
    );
})

} // namespace esp_brookesia::apps
