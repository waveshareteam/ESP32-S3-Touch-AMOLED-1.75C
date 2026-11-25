#pragma once

#include <array>

#include "display_power.hpp"
#include "esp_brookesia.hpp"
#include "lvgl.h"
namespace esp_brookesia::apps {
    class Settings;
}
namespace esp_brookesia::apps {

class DisplayPage : public systems::phone::App {
public:
    static DisplayPage *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    DisplayPage(bool use_status_bar, bool use_navigation_bar);
    virtual ~DisplayPage();

    bool run() override;
    bool back() override;
    bool close() override;

private:
    static void event_handler_cb(lv_event_t *e);
    static void timeout_event_cb(lv_event_t *e);
    void show_timeout_check();
    static DisplayPage *_instance;
    lv_obj_t *page_root;
    lv_obj_t *label;
    lv_obj_t *list1;
    std::array<lv_obj_t *, display_power::TIMEOUT_OPTIONS.size()> timeout_checks{};
    lv_style_t style_list;
    lv_style_t style_list_btn;
    lv_style_t style_list_text;
    lv_style_t style_list_btn_pressed;
};

} // namespace esp_brookesia::apps
