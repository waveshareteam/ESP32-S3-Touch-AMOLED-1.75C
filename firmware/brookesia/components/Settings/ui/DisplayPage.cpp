#include "DisplayPage.hpp"
#include "SettingsUI.hpp"
#include "display_power.hpp"
#include "esp_log.h"
#include "../Settings.hpp"

#define ESP_UTILS_LOG_TAG "BS:DisplayPage"
#include "esp_lib_utils.h"

namespace esp_brookesia::apps
{
    DisplayPage *DisplayPage::_instance = nullptr;

    DisplayPage *DisplayPage::requestInstance(bool use_status_bar, bool use_navigation_bar)
    {
        if (_instance == nullptr)
        {
            _instance = new DisplayPage(use_status_bar, use_navigation_bar);
        }
        return _instance;
    }

    DisplayPage::DisplayPage(bool use_status_bar, bool use_navigation_bar)
        : App("Display", nullptr, true, use_status_bar, use_navigation_bar),
          page_root(nullptr), label(nullptr), list1(nullptr)
    {
    }

    DisplayPage::~DisplayPage()
    {
    }

    bool DisplayPage::run()
    {
        ESP_UTILS_LOGD("DisplayPage Run");
        lv_obj_clean(lv_scr_act());

        page_root = settings_ui::create_page(lv_scr_act());
        settings_ui::create_header(page_root, "Display", [](lv_event_t *e) {
            (void)e;
            lv_async_call([](void *param) {
                (void)param;
                Settings::requestInstance()->showRootPage();
            }, nullptr);
        });

        settings_ui::init_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        list1 = settings_ui::create_content_list(page_root);
        lv_obj_add_style(list1, &style_list, LV_PART_MAIN);

        settings_ui::add_section(list1, "Screen", style_list_text);

        lv_obj_t *slider_container = lv_obj_create(list1);
        lv_obj_add_style(slider_container, &style_list_btn, LV_PART_MAIN);
        lv_obj_set_size(slider_container, lv_pct(100), settings_ui::CONTROL_PANEL_HEIGHT);
        lv_obj_set_style_pad_all(slider_container, 20, LV_PART_MAIN);
        lv_obj_clear_flag(slider_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(slider_container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(slider_container);
        lv_label_set_text(title, "Brightness");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
        lv_obj_set_style_text_color(title, lv_color_hex(settings_ui::COLOR_PRIMARY_TEXT), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

        const int32_t value = display_power::brightness();

        label = lv_label_create(slider_container);
        lv_label_set_text_fmt(label, "%ld%%", static_cast<long>(value));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(settings_ui::COLOR_ACCENT), LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_TOP_RIGHT, 0, 1);

        lv_obj_t *brightness_slider = lv_slider_create(slider_container);
        lv_obj_set_width(brightness_slider, lv_pct(100));
        lv_obj_set_height(brightness_slider, 14);
        lv_obj_align(brightness_slider, LV_ALIGN_BOTTOM_MID, 0, -5);
        lv_slider_set_range(brightness_slider, display_power::BRIGHTNESS_MIN, display_power::BRIGHTNESS_MAX);
        lv_slider_set_value(brightness_slider, value, LV_ANIM_OFF);

        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(0x505050), LV_PART_MAIN);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_hex(settings_ui::COLOR_ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(brightness_slider, lv_color_white(), LV_PART_KNOB);
        lv_obj_set_style_pad_all(brightness_slider, 7, LV_PART_KNOB);
        lv_obj_set_style_radius(brightness_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_add_event_cb(brightness_slider, event_handler_cb, LV_EVENT_VALUE_CHANGED, this);
        lv_obj_add_event_cb(brightness_slider, event_handler_cb, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(brightness_slider, event_handler_cb, LV_EVENT_PRESS_LOST, this);

        settings_ui::add_section(list1, "Screen timeout", style_list_text);
        for (size_t i = 0; i < display_power::TIMEOUT_OPTIONS.size(); ++i) {
            lv_obj_t *row = lv_list_add_button(list1, nullptr, display_power::TIMEOUT_OPTIONS[i].label);
            lv_obj_add_style(row, &style_list_btn, LV_PART_MAIN);
            lv_obj_add_style(row, &style_list_btn_pressed, LV_STATE_PRESSED);
            settings_ui::use_ellipsis_for_button_label(row);
            lv_obj_add_event_cb(row, timeout_event_cb, LV_EVENT_CLICKED, reinterpret_cast<void *>(i));

            timeout_checks[i] = lv_label_create(row);
            lv_label_set_text(timeout_checks[i], LV_SYMBOL_OK);
            lv_obj_set_style_text_color(timeout_checks[i], lv_color_hex(settings_ui::COLOR_ACCENT), LV_PART_MAIN);
        }
        show_timeout_check();
        return true;
    }

    void DisplayPage::show_timeout_check()
    {
        const uint32_t selected = display_power::timeout_s();
        for (size_t i = 0; i < timeout_checks.size(); ++i) {
            if (display_power::TIMEOUT_OPTIONS[i].seconds == selected) {
                lv_obj_remove_flag(timeout_checks[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(timeout_checks[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    void DisplayPage::timeout_event_cb(lv_event_t *e)
    {
        const size_t index = reinterpret_cast<size_t>(lv_event_get_user_data(e));
        display_power::set_timeout_s(display_power::TIMEOUT_OPTIONS[index].seconds);
        _instance->show_timeout_check();
    }

    void DisplayPage::event_handler_cb(lv_event_t *e)
    {
        DisplayPage *instance = static_cast<DisplayPage *>(lv_event_get_user_data(e));
        lv_obj_t *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
        if (instance == nullptr || slider == nullptr || !lv_obj_has_class(slider, &lv_slider_class)) {
            return;
        }

        if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
            display_power::save_brightness();
            return;
        }
        const int32_t value = lv_slider_get_value(slider);
        if (instance->label != nullptr) {
            lv_label_set_text_fmt(instance->label, "%ld%%", static_cast<long>(value));
        }
        display_power::set_brightness(value);
    }

    bool DisplayPage::back()
    {
        ESP_UTILS_LOGD("DisplayPage Back");
        Settings::requestInstance()->showRootPage();
        return true;
    }

    bool DisplayPage::close()
    {
        ESP_UTILS_LOGD("DisplayPage Close");
        if (page_root != nullptr)
        {
            lv_obj_del(page_root);
            page_root = nullptr;
            label = nullptr;
            list1 = nullptr;
            timeout_checks.fill(nullptr);
            settings_ui::reset_list_styles(style_list, style_list_btn, style_list_text, style_list_btn_pressed);
        }
        return true;
    }

} // namespace esp_brookesia::apps
