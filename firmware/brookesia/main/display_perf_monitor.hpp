/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"
#include "lvgl.h"

/**
 * @brief Start the optional, low-overhead display performance logger.
 *
 * The implementation is compiled to a no-op when
 * CONFIG_BROOKESIA_DISPLAY_PERF_LOG is disabled.
 */
esp_err_t display_perf_monitor_start(lv_display_t *display);
