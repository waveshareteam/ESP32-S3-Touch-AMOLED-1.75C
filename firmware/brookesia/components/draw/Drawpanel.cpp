/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "lvgl.h"
#include "esp_brookesia.hpp"
#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "BS:Drawpanel"
#include "esp_lib_utils.h"
#include "Drawpanel.hpp"
#include <list>

#define APP_NAME "DrawPanel"

using namespace std;
using namespace esp_brookesia::gui;
using namespace esp_brookesia::systems;

LV_IMG_DECLARE(img_app_drawpanel);

namespace esp_brookesia::apps {

static constexpr int DRAW_DOT_SIZE = 10;

static void configureFixedDrawObject(lv_obj_t *obj, bool clickable)
{
    if (!obj) {
        return;
    }

    lv_obj_clear_flag(
        obj,
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
        LV_OBJ_FLAG_SCROLL_ONE | LV_OBJ_FLAG_SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_ON_FOCUS |
        LV_OBJ_FLAG_SCROLL_WITH_ARROW | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE
    );
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    }
}

Drawpanel *Drawpanel::_instance = nullptr;

Drawpanel *Drawpanel::requestInstance(bool use_status_bar, bool use_navigation_bar)
{
    if (_instance == nullptr) {
        _instance = new Drawpanel(use_status_bar, use_navigation_bar);
    }
    return _instance;
}

Drawpanel::Drawpanel(bool use_status_bar, bool use_navigation_bar) :
    App(APP_NAME, &img_app_drawpanel, true, use_status_bar, use_navigation_bar),
    _panel_obj(nullptr),
    _max_points(1000)
{
}

Drawpanel::~Drawpanel()
{
    close();
}

bool Drawpanel::run(void)
{
    ESP_UTILS_LOGD("Run");

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    // The app screen already matches Brookesia's visual area. Using it directly
    // prevents children near an edge from expanding a separate scrollable panel.
    _panel_obj = screen;
    configureFixedDrawObject(_panel_obj, true);
    lv_obj_remove_event_cb_with_user_data(_panel_obj, touch_event_cb, this);
    lv_obj_add_event_cb(_panel_obj, touch_event_cb, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(_panel_obj, touch_event_cb, LV_EVENT_PRESSING, this);

    return true;
}

bool Drawpanel::back(void)
{
    ESP_UTILS_LOGD("Back");

    // If the app needs to exit, call notifyCoreClosed() to notify the core to close the app
    ESP_UTILS_CHECK_FALSE_RETURN(notifyCoreClosed(), false, "Notify core closed failed");

    return true;
}

bool Drawpanel::close(void)
{
    ESP_UTILS_LOGD("Close");

    clearAllPoints();
    if (_panel_obj) {
        lv_obj_remove_event_cb_with_user_data(_panel_obj, touch_event_cb, this);
        _panel_obj = nullptr;
    }

    return true;
}

bool Drawpanel::init()
{
    ESP_UTILS_LOGD("Init");

    return true;
}

bool Drawpanel::deinit()
{
    ESP_UTILS_LOGD("Deinit");

    return true;
}

bool Drawpanel::pause()
{
    ESP_UTILS_LOGD("Pause");

    return true;
}

bool Drawpanel::resume()
{
    ESP_UTILS_LOGD("Resume");

    return true;
}

void Drawpanel::clearAllPoints()
{
    for (lv_obj_t* dot : _dots) {
        if (dot) {
            lv_obj_del(dot);
        }
    }
    _dots.clear();
}

void Drawpanel::touch_event_cb(lv_event_t *e)
{
    Drawpanel *app = (Drawpanel *)lv_event_get_user_data(e);
    if (!app || !app->_panel_obj) {
        ESP_UTILS_LOGE("Get drawpanel instance failed");
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING) {
        return;
    }

    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    lv_area_t panel_area;
    lv_obj_get_coords(app->_panel_obj, &panel_area);
    if (point.x < panel_area.x1 || point.x > panel_area.x2 ||
            point.y < panel_area.y1 || point.y > panel_area.y2) {
        return;
    }

    const int panel_width = lv_area_get_width(&panel_area);
    const int panel_height = lv_area_get_height(&panel_area);
    const int dimension_delta = panel_width - panel_height;
    if (dimension_delta >= -8 && dimension_delta <= panel_width / 4 &&
            panel_width >= 300 && panel_height >= 300) {
        // The touch controller still reports the square bounding box of the
        // round AMOLED. Ignore the four physically invisible corners instead
        // of storing dots which the user cannot see or erase deliberately.
        const int local_x = point.x - panel_area.x1;
        const int local_y = point.y - panel_area.y1;
        const int center_x = (panel_width - 1) / 2;
        // A fixed status bar crops the top of the app-local screen. Shift the
        // physical circle center upward by exactly that crop.
        const int top_crop = (panel_width > panel_height) ? panel_width - panel_height : 0;
        const int center_y = center_x - top_crop;
        const int diameter = panel_width;
        const int radius = (diameter - DRAW_DOT_SIZE) / 2;
        const int dx = local_x - center_x;
        const int dy = local_y - center_y;

        if (dx * dx + dy * dy > radius * radius) {
            return;
        }
    }

    int dot_x = point.x - panel_area.x1 - DRAW_DOT_SIZE / 2;
    int dot_y = point.y - panel_area.y1 - DRAW_DOT_SIZE / 2;
    int max_x = panel_width - DRAW_DOT_SIZE;
    int max_y = panel_height - DRAW_DOT_SIZE;
    max_x = (max_x > 0) ? max_x : 0;
    max_y = (max_y > 0) ? max_y : 0;
    dot_x = (dot_x < 0) ? 0 : ((dot_x > max_x) ? max_x : dot_x);
    dot_y = (dot_y < 0) ? 0 : ((dot_y > max_y) ? max_y : dot_y);

    lv_obj_t *dot = lv_obj_create(app->_panel_obj);
    configureFixedDrawObject(dot, false);
    lv_obj_set_size(dot, DRAW_DOT_SIZE, DRAW_DOT_SIZE);
    lv_obj_set_pos(dot, dot_x, dot_y);
    lv_obj_set_style_bg_color(dot, lv_color_make(255, 0, 0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dot, DRAW_DOT_SIZE / 2, LV_PART_MAIN);

    app->_dots.push_back(dot);

    if (app->_dots.size() > app->_max_points) {
        lv_obj_t* oldest_dot = app->_dots.front();
        if (oldest_dot) {
            lv_obj_del(oldest_dot);
        }
        app->_dots.pop_front();
    }
}

} // namespace esp_brookesia::apps
