/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "systems/phone/esp_brookesia_phone_app.hpp"

namespace esp_brookesia::apps {

/**
 * @brief Drawpanel application for touch-based drawing on the device screen
 *
 */
class Drawpanel: public systems::phone::App {
public:
    /**
     * @brief Get the singleton instance of Drawpanel
     *
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     * @return Drawpanel* Singleton instance pointer
     */
    static Drawpanel *requestInstance(bool use_status_bar = false, bool use_navigation_bar = false);

    /**
     * @brief Destroy the Drawpanel object
     *
     */
    ~Drawpanel();

    using systems::phone::App::startRecordResource;
    using systems::phone::App::endRecordResource;

protected:
    /**
     * @brief Construct a new Drawpanel object (private to enforce singleton)
     *
     * @param use_status_bar Show status bar
     * @param use_navigation_bar Show navigation bar
     */
    Drawpanel(bool use_status_bar, bool use_navigation_bar);

    bool run(void) override;
    bool back(void) override;

    bool close(void) override;
    bool init(void) override;
    bool deinit(void) override;
    bool pause(void) override;
    bool resume(void) override;
    // bool cleanResource(void) override;

private:
    static Drawpanel *_instance;

    /**
     * @brief Touch event callback for drawing dots on screen
     *
     * @param e LVGL event data
     */

    lv_obj_t *_panel_obj;
    std::list<lv_obj_t *> _dots;
    const int _max_points;
    void clearAllPoints();
    static void touch_event_cb(lv_event_t *e);

    lv_point_t prev_point;
};

} // namespace esp_brookesia::apps