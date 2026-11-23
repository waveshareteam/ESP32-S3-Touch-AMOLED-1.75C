/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Crosshair.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include "esp_brookesia.hpp"
#include "esp_lib_utils.h"

#ifdef ESP_UTILS_LOG_TAG
#undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Crosshair"

LV_IMG_DECLARE(img_app_crosshair);

namespace esp_brookesia::apps {

namespace {

constexpr char APP_NAME[] = "Crosshair";
constexpr double PI = 3.14159265358979323846;
constexpr uint32_t HINT_PERIOD_MS = 1600;

constexpr uint32_t COLOR_BACKGROUND = 0x000000;
constexpr uint32_t COLOR_STANDARD_OUTER = 0x29B6F6;
constexpr uint32_t COLOR_STANDARD_AXIS = 0xB0BEC5;
constexpr uint32_t COLOR_STANDARD_MINOR = 0x546E7A;
constexpr uint32_t COLOR_STANDARD_MAJOR = 0xFFCA28;
constexpr uint32_t COLOR_STANDARD_TARGET = 0xFF5252;
constexpr uint32_t COLOR_STANDARD_TEXT = 0xECEFF1;

constexpr uint32_t COLOR_HIGH_OUTER = 0xFFFFFF;
constexpr uint32_t COLOR_HIGH_AXIS = 0xFFFF00;
constexpr uint32_t COLOR_HIGH_MINOR = 0xFFFFFF;
constexpr uint32_t COLOR_HIGH_MAJOR = 0xFF3D00;
constexpr uint32_t COLOR_HIGH_TARGET = 0x00FF66;
constexpr uint32_t COLOR_HIGH_TEXT = 0xFFFFFF;

void makePassive(lv_obj_t *object)
{
    if (object == nullptr) {
        return;
    }
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void clearObjectStyle(lv_obj_t *object)
{
    if (object == nullptr) {
        return;
    }
    makePassive(object);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(object, 0, LV_PART_MAIN);
}

int matchParity(int value, int reference)
{
    if ((value & 1) != (reference & 1)) {
        ++value;
    }
    return value;
}

} // namespace

Crosshair *Crosshair::_instance = nullptr;

Crosshair *Crosshair::requestInstance()
{
    if (_instance == nullptr) {
        _instance = new Crosshair();
    }
    return _instance;
}

Crosshair::Crosshair()
    : App(APP_NAME, &img_app_crosshair, true, false, false)
{
}

bool Crosshair::init()
{
    return true;
}

bool Crosshair::deinit()
{
    stopHintTimer();
    clearUiReferences();
    return true;
}

bool Crosshair::run()
{
    _visual_area = getVisualArea();
    _visual_width = lv_area_get_width(&_visual_area);
    _visual_height = lv_area_get_height(&_visual_area);
    ESP_UTILS_CHECK_FALSE_RETURN(
        _visual_width >= 96 && _visual_height >= 96,
        false,
        "Visual area is too small"
    );

    _high_contrast = false;
    return createUi();
}

bool Crosshair::back()
{
    return notifyCoreClosed();
}

bool Crosshair::close()
{
    stopHintTimer();
    // Brookesia owns the application screen and deletes all child objects.
    clearUiReferences();
    return true;
}

bool Crosshair::pause()
{
    if (_hint_timer != nullptr) {
        lv_timer_pause(_hint_timer);
    }
    return true;
}

bool Crosshair::resume()
{
    if (_hint_timer != nullptr && _hint_label != nullptr &&
            !lv_obj_has_flag(_hint_label, LV_OBJ_FLAG_HIDDEN)) {
        lv_timer_reset(_hint_timer);
        lv_timer_resume(_hint_timer);
    }
    return true;
}

bool Crosshair::createUi()
{
    _screen = lv_screen_active();
    ESP_UTILS_CHECK_NULL_RETURN(_screen, false, "Get active screen failed");

    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(_screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        _screen, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN
    );
    lv_obj_add_event_cb(
        _screen, screenEventCallback, LV_EVENT_CLICKED, this
    );

    const int shortest_side = std::min(_visual_width, _visual_height);
    const int margin = std::max(3, shortest_side / 80);
    _circle_diameter = shortest_side - (margin * 2);
    ESP_UTILS_CHECK_FALSE_RETURN(
        _circle_diameter >= 80, false, "Crosshair circle is too small"
    );

    const int circle_left =
        _visual_area.x1 + (_visual_width - _circle_diameter) / 2;
    const int circle_top =
        _visual_area.y1 + (_visual_height - _circle_diameter) / 2;
    const int horizontal_thickness = (_circle_diameter & 1) ? 1 : 2;
    const int vertical_thickness = horizontal_thickness;
    const int axis_inset = std::max(3, _circle_diameter / 100);

    _horizontal_axis = lv_obj_create(_screen);
    _vertical_axis = lv_obj_create(_screen);
    ESP_UTILS_CHECK_FALSE_RETURN(
        _horizontal_axis != nullptr && _vertical_axis != nullptr,
        false,
        "Create center axes failed"
    );
    clearObjectStyle(_horizontal_axis);
    clearObjectStyle(_vertical_axis);
    lv_obj_set_size(
        _horizontal_axis,
        _circle_diameter - (axis_inset * 2),
        horizontal_thickness
    );
    lv_obj_set_pos(
        _horizontal_axis,
        circle_left + axis_inset,
        circle_top + (_circle_diameter - horizontal_thickness) / 2
    );
    lv_obj_set_size(
        _vertical_axis,
        vertical_thickness,
        _circle_diameter - (axis_inset * 2)
    );
    lv_obj_set_pos(
        _vertical_axis,
        circle_left + (_circle_diameter - vertical_thickness) / 2,
        circle_top + axis_inset
    );

    const double center = static_cast<double>(_circle_diameter - 1) / 2.0;
    const int center_twice = _circle_diameter - 1;
    const double tick_outer_radius = center - std::max(4, _circle_diameter / 100);
    const int major_tick_length = std::max(12, _circle_diameter / 10);
    const int minor_tick_length = std::max(7, _circle_diameter / 23);

    // Calculate one semicircle and reflect it exactly through the geometric
    // center. This keeps every minor tick paired even on even-sized displays,
    // whose true center lies between two pixels.
    for (std::size_t index = 0; index < TICK_COUNT / 2; ++index) {
        const double angle =
            (-PI / 2.0) +
            static_cast<double>(index) * (2.0 * PI / TICK_COUNT);
        const bool major = (index % (TICK_COUNT / 4)) == 0;
        const double inner_radius =
            tick_outer_radius - (major ? major_tick_length : minor_tick_length);

        _tick_points[index][0] = {
            static_cast<lv_value_precise_t>(
                std::lround(center + std::cos(angle) * tick_outer_radius)
            ),
            static_cast<lv_value_precise_t>(
                std::lround(center + std::sin(angle) * tick_outer_radius)
            ),
        };
        _tick_points[index][1] = {
            static_cast<lv_value_precise_t>(
                std::lround(center + std::cos(angle) * inner_radius)
            ),
            static_cast<lv_value_precise_t>(
                std::lround(center + std::sin(angle) * inner_radius)
            ),
        };

        const std::size_t opposite = index + TICK_COUNT / 2;
        for (std::size_t point = 0; point < 2; ++point) {
            _tick_points[opposite][point] = {
                static_cast<lv_value_precise_t>(
                    center_twice - _tick_points[index][point].x
                ),
                static_cast<lv_value_precise_t>(
                    center_twice - _tick_points[index][point].y
                ),
            };
        }
    }

    for (std::size_t index = 0; index < TICK_COUNT; ++index) {
        _ticks[index] = lv_line_create(_screen);
        ESP_UTILS_CHECK_NULL_RETURN(
            _ticks[index], false, "Create angle tick failed"
        );
        makePassive(_ticks[index]);
        lv_obj_set_pos(_ticks[index], circle_left, circle_top);
        lv_line_set_points(
            _ticks[index], _tick_points[index].data(), 2
        );
        const bool major = (index % (TICK_COUNT / 4)) == 0;
        lv_obj_set_style_line_width(
            _ticks[index], major ? 3 : 2, LV_PART_MAIN
        );
        lv_obj_set_style_line_rounded(_ticks[index], true, LV_PART_MAIN);
    }

    _outer_ring = lv_obj_create(_screen);
    ESP_UTILS_CHECK_NULL_RETURN(
        _outer_ring, false, "Create outer reference circle failed"
    );
    clearObjectStyle(_outer_ring);
    lv_obj_set_size(_outer_ring, _circle_diameter, _circle_diameter);
    lv_obj_set_pos(_outer_ring, circle_left, circle_top);
    lv_obj_set_style_radius(_outer_ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_outer_ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(_outer_ring, 3, LV_PART_MAIN);

    const std::array<int, TARGET_RING_COUNT> ring_divisors = {4, 7, 12};
    for (std::size_t index = 0; index < TARGET_RING_COUNT; ++index) {
        int ring_size = std::max(12, _circle_diameter / ring_divisors[index]);
        ring_size = matchParity(ring_size, _circle_diameter);
        ring_size = std::min(ring_size, _circle_diameter - 12);

        _target_rings[index] = lv_obj_create(_screen);
        ESP_UTILS_CHECK_NULL_RETURN(
            _target_rings[index], false, "Create center target ring failed"
        );
        clearObjectStyle(_target_rings[index]);
        lv_obj_set_size(_target_rings[index], ring_size, ring_size);
        lv_obj_set_pos(
            _target_rings[index],
            circle_left + (_circle_diameter - ring_size) / 2,
            circle_top + (_circle_diameter - ring_size) / 2
        );
        lv_obj_set_style_radius(
            _target_rings[index], LV_RADIUS_CIRCLE, LV_PART_MAIN
        );
        lv_obj_set_style_bg_opa(
            _target_rings[index], LV_OPA_TRANSP, LV_PART_MAIN
        );
        lv_obj_set_style_border_width(
            _target_rings[index], index == 0 ? 2 : 1, LV_PART_MAIN
        );
    }

    int dot_size = matchParity(
        std::max(6, _circle_diameter / 70), _circle_diameter
    );
    _center_dot = lv_obj_create(_screen);
    ESP_UTILS_CHECK_NULL_RETURN(
        _center_dot, false, "Create center reference point failed"
    );
    clearObjectStyle(_center_dot);
    lv_obj_set_size(_center_dot, dot_size, dot_size);
    lv_obj_set_pos(
        _center_dot,
        circle_left + (_circle_diameter - dot_size) / 2,
        circle_top + (_circle_diameter - dot_size) / 2
    );
    lv_obj_set_style_radius(_center_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_center_dot, LV_OPA_COVER, LV_PART_MAIN);

    const std::array<const char *, ANGLE_LABEL_COUNT> label_text = {
        "0\xC2\xB0", "90\xC2\xB0", "180\xC2\xB0", "270\xC2\xB0"
    };
    const int label_width = std::max(42, _circle_diameter / 8);
    const int label_height = std::max(20, _circle_diameter / 24);
    const double label_radius = center * 0.60;
    const double label_offset = center * 0.12;
    const std::array<double, ANGLE_LABEL_COUNT> label_dx = {
        label_offset, label_radius, -label_offset, -label_radius
    };
    const std::array<double, ANGLE_LABEL_COUNT> label_dy = {
        -label_radius, label_offset, label_radius, -label_offset
    };

    const double absolute_center_x =
        static_cast<double>(circle_left) + center;
    const double absolute_center_y =
        static_cast<double>(circle_top) + center;
    for (std::size_t index = 0; index < ANGLE_LABEL_COUNT; ++index) {
        _angle_labels[index] = lv_label_create(_screen);
        ESP_UTILS_CHECK_NULL_RETURN(
            _angle_labels[index], false, "Create angle label failed"
        );
        makePassive(_angle_labels[index]);
        lv_label_set_text(_angle_labels[index], label_text[index]);
        lv_obj_set_size(_angle_labels[index], label_width, label_height);
        lv_obj_set_style_text_align(
            _angle_labels[index], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN
        );
        lv_obj_set_pos(
            _angle_labels[index],
            static_cast<int>(std::lround(
                absolute_center_x + label_dx[index] - label_width / 2.0
            )),
            static_cast<int>(std::lround(
                absolute_center_y + label_dy[index] - label_height / 2.0
            ))
        );
    }

    const int hint_width = std::min(280, _circle_diameter / 2);
    const int hint_height = std::max(30, _circle_diameter / 14);
    _hint_label = lv_label_create(_screen);
    ESP_UTILS_CHECK_NULL_RETURN(
        _hint_label, false, "Create contrast hint failed"
    );
    makePassive(_hint_label);
    lv_label_set_text(_hint_label, "Tap to switch contrast");
    lv_obj_set_size(_hint_label, hint_width, hint_height);
    lv_obj_set_style_text_align(
        _hint_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN
    );
    lv_obj_set_style_bg_opa(_hint_label, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        _hint_label, lv_color_hex(COLOR_BACKGROUND), LV_PART_MAIN
    );
    lv_obj_set_style_radius(
        _hint_label, std::max(14, hint_height / 2), LV_PART_MAIN
    );
    lv_obj_set_style_pad_all(_hint_label, 5, LV_PART_MAIN);
    lv_obj_set_pos(
        _hint_label,
        static_cast<int>(std::lround(absolute_center_x - hint_width / 2.0)),
        static_cast<int>(std::lround(
            absolute_center_y + center * 0.42 - hint_height / 2.0
        ))
    );

    updatePalette();
    _hint_timer = lv_timer_create(hintTimerCallback, HINT_PERIOD_MS, this);
    return true;
}

void Crosshair::screenEventCallback(lv_event_t *event)
{
    auto *app = static_cast<Crosshair *>(lv_event_get_user_data(event));
    if (app == nullptr) {
        return;
    }

    app->_high_contrast = !app->_high_contrast;
    app->updatePalette();
    app->showContrastHint();
}

void Crosshair::hintTimerCallback(lv_timer_t *timer)
{
    auto *app = static_cast<Crosshair *>(lv_timer_get_user_data(timer));
    if (app == nullptr) {
        return;
    }

    if (app->_hint_label != nullptr) {
        lv_obj_add_flag(app->_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_pause(timer);
}

void Crosshair::updatePalette()
{
    const uint32_t outer =
        _high_contrast ? COLOR_HIGH_OUTER : COLOR_STANDARD_OUTER;
    const uint32_t axis =
        _high_contrast ? COLOR_HIGH_AXIS : COLOR_STANDARD_AXIS;
    const uint32_t minor =
        _high_contrast ? COLOR_HIGH_MINOR : COLOR_STANDARD_MINOR;
    const uint32_t major =
        _high_contrast ? COLOR_HIGH_MAJOR : COLOR_STANDARD_MAJOR;
    const uint32_t target =
        _high_contrast ? COLOR_HIGH_TARGET : COLOR_STANDARD_TARGET;
    const uint32_t text =
        _high_contrast ? COLOR_HIGH_TEXT : COLOR_STANDARD_TEXT;

    if (_outer_ring != nullptr) {
        lv_obj_set_style_border_color(
            _outer_ring, lv_color_hex(outer), LV_PART_MAIN
        );
    }
    if (_horizontal_axis != nullptr) {
        lv_obj_set_style_bg_color(
            _horizontal_axis, lv_color_hex(axis), LV_PART_MAIN
        );
        lv_obj_set_style_bg_opa(
            _horizontal_axis, LV_OPA_COVER, LV_PART_MAIN
        );
    }
    if (_vertical_axis != nullptr) {
        lv_obj_set_style_bg_color(
            _vertical_axis, lv_color_hex(axis), LV_PART_MAIN
        );
        lv_obj_set_style_bg_opa(
            _vertical_axis, LV_OPA_COVER, LV_PART_MAIN
        );
    }

    for (std::size_t index = 0; index < TICK_COUNT; ++index) {
        if (_ticks[index] == nullptr) {
            continue;
        }
        const bool is_major = (index % (TICK_COUNT / 4)) == 0;
        lv_obj_set_style_line_color(
            _ticks[index],
            lv_color_hex(is_major ? major : minor),
            LV_PART_MAIN
        );
    }
    for (lv_obj_t *ring : _target_rings) {
        if (ring != nullptr) {
            lv_obj_set_style_border_color(
                ring, lv_color_hex(target), LV_PART_MAIN
            );
        }
    }
    if (_center_dot != nullptr) {
        lv_obj_set_style_bg_color(
            _center_dot, lv_color_hex(target), LV_PART_MAIN
        );
    }
    for (lv_obj_t *label : _angle_labels) {
        if (label != nullptr) {
            lv_obj_set_style_text_color(
                label, lv_color_hex(text), LV_PART_MAIN
            );
        }
    }
    if (_hint_label != nullptr) {
        lv_obj_set_style_text_color(
            _hint_label, lv_color_hex(text), LV_PART_MAIN
        );
        lv_obj_set_style_border_width(_hint_label, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(
            _hint_label, lv_color_hex(outer), LV_PART_MAIN
        );
    }
}

void Crosshair::showContrastHint()
{
    if (_hint_label == nullptr) {
        return;
    }

    lv_label_set_text(
        _hint_label,
        _high_contrast ? "High contrast: ON" : "High contrast: OFF"
    );
    lv_obj_clear_flag(_hint_label, LV_OBJ_FLAG_HIDDEN);
    if (_hint_timer != nullptr) {
        lv_timer_set_period(_hint_timer, HINT_PERIOD_MS);
        lv_timer_reset(_hint_timer);
        lv_timer_resume(_hint_timer);
    }
}

void Crosshair::stopHintTimer()
{
    if (_hint_timer != nullptr) {
        lv_timer_delete(_hint_timer);
        _hint_timer = nullptr;
    }
}

void Crosshair::clearUiReferences()
{
    _screen = nullptr;
    _outer_ring = nullptr;
    _horizontal_axis = nullptr;
    _vertical_axis = nullptr;
    _ticks.fill(nullptr);
    _target_rings.fill(nullptr);
    _center_dot = nullptr;
    _angle_labels.fill(nullptr);
    _hint_label = nullptr;
    _circle_diameter = 0;
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(
    systems::base::App, Crosshair, APP_NAME, []() {
        return std::shared_ptr<Crosshair>(
            Crosshair::requestInstance(), [](Crosshair *) {}
        );
    }
)

} // namespace esp_brookesia::apps
