/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <cstdint>

#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class ButtonTest final : public systems::phone::App {
public:
    static ButtonTest *requestInstance();
    ~ButtonTest() override;

    using systems::phone::App::endRecordResource;
    using systems::phone::App::startRecordResource;

protected:
    ButtonTest();

    bool init() override;
    bool deinit() override;
    bool run() override;
    bool back() override;
    bool close() override;
    bool pause() override;
    bool resume() override;

private:
    struct ButtonWidgets {
        lv_obj_t *card = nullptr;
        lv_obj_t *state_panel = nullptr;
        lv_obj_t *state_label = nullptr;
        lv_obj_t *raw_label = nullptr;
        lv_obj_t *pass_label = nullptr;
    };

    static ButtonTest *_instance;

    static void workerTask(void *arg);
    static void uiTimerCallback(lv_timer_t *timer);

    bool configureBootInput();
    bool configurePwrInput();
    bool createUi();
    ButtonWidgets createButtonCard(
        lv_obj_t *parent,
        const char *name,
        const char *pin_name
    );
    void layoutUi();
    void updateUi();
    void updateButtonCard(
        ButtonWidgets &widgets,
        int raw_level,
        bool active_high,
        bool passed
    );
    void releaseUi();
    void workerLoop();
    void stopWorker();

    bool _pwr_configured = false;
    bool _boot_configured = false;

    std::atomic<TaskHandle_t> _worker_task = nullptr;
    std::atomic<bool> _running = false;
    std::atomic<bool> _paused = false;
    std::atomic<int> _pwr_level = -1;
    std::atomic<int> _boot_level = -1;
    std::atomic<bool> _pwr_passed = false;
    std::atomic<bool> _boot_passed = false;

    lv_area_t _visual_area = {};
    int _visual_width = 0;
    int _visual_height = 0;
    lv_obj_t *_screen = nullptr;
    lv_obj_t *_title_label = nullptr;
    lv_obj_t *_subtitle_label = nullptr;
    ButtonWidgets _pwr_widgets = {};
    ButtonWidgets _boot_widgets = {};
    lv_obj_t *_result_label = nullptr;
    lv_obj_t *_warning_label = nullptr;
    lv_timer_t *_ui_timer = nullptr;

    static constexpr gpio_num_t BOOT_GPIO = BSP_BOOT_KEY_IO;
    static constexpr gpio_num_t PWR_GPIO = BSP_PWR_KEY_IO;
    static constexpr int SAMPLE_PERIOD_MS = 50;
    static constexpr int UI_PERIOD_MS = 100;
    static constexpr int INPUT_RETRY_PERIOD_MS = 1000;
};

} // namespace esp_brookesia::apps
