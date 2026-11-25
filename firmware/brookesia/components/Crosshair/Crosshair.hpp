/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstddef>

#include "lvgl.h"
#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

class Crosshair final : public systems::phone::App {
public:
    static Crosshair *requestInstance();
    ~Crosshair() override = default;

    using systems::phone::App::endRecordResource;
    using systems::phone::App::startRecordResource;

protected:
    Crosshair();

    bool init() override;
    bool deinit() override;
    bool run() override;
    bool back() override;
    bool close() override;
    bool pause() override;
    bool resume() override;

private:
    static constexpr std::size_t TICK_COUNT = 24;
    static constexpr std::size_t TARGET_RING_COUNT = 3;
    static constexpr std::size_t ANGLE_LABEL_COUNT = 4;

    static Crosshair *_instance;

    static void screenEventCallback(lv_event_t *event);
    static void hintTimerCallback(lv_timer_t *timer);

    bool createUi();
    void updatePalette();
    void showContrastHint();
    void stopHintTimer();
    void clearUiReferences();

    lv_area_t _visual_area = {};
    int _visual_width = 0;
    int _visual_height = 0;
    int _circle_diameter = 0;
    bool _high_contrast = false;

    lv_obj_t *_screen = nullptr;
    lv_obj_t *_outer_ring = nullptr;
    lv_obj_t *_horizontal_axis = nullptr;
    lv_obj_t *_vertical_axis = nullptr;
    std::array<lv_obj_t *, TICK_COUNT> _ticks = {};
    std::array<std::array<lv_point_precise_t, 2>, TICK_COUNT> _tick_points = {};
    std::array<lv_obj_t *, TARGET_RING_COUNT> _target_rings = {};
    lv_obj_t *_center_dot = nullptr;
    std::array<lv_obj_t *, ANGLE_LABEL_COUNT> _angle_labels = {};
    lv_obj_t *_hint_label = nullptr;
    lv_timer_t *_hint_timer = nullptr;
};

} // namespace esp_brookesia::apps
