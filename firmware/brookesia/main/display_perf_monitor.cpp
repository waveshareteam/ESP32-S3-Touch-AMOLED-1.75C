/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "display_perf_monitor.hpp"

#if CONFIG_BROOKESIA_DISPLAY_PERF_LOG

#include <algorithm>
#include <cinttypes>
#include <cstdint>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char *TAG = "DisplayPerf";
constexpr int64_t FLUSH_WAIT_BLOCKED_THRESHOLD_US = 50;

struct DisplayPerfContext {
    lv_display_t *display = nullptr;
    lv_timer_t *report_timer = nullptr;
    int64_t window_start_us = 0;
    int64_t refresh_start_us = 0;
    int64_t render_start_us = 0;
    int64_t flush_submit_start_us = 0;
    int64_t flush_wait_start_us = 0;
    uint64_t refresh_total_us = 0;
    uint64_t render_total_us = 0;
    uint64_t flush_submit_total_us = 0;
    uint64_t flush_wait_total_us = 0;
    uint64_t flushed_pixels = 0;
    uint32_t refresh_count = 0;
    uint32_t redraw_count = 0;
    uint32_t flush_count = 0;
    uint32_t flush_wait_count = 0;
    uint32_t flush_wait_blocked_count = 0;
    uint32_t refresh_max_us = 0;
    uint32_t render_max_us = 0;
    uint32_t flush_submit_max_us = 0;
    uint32_t flush_wait_max_us = 0;
    bool started = false;
};

DisplayPerfContext perf;

uint32_t saturating_us(int64_t elapsed_us)
{
    if (elapsed_us <= 0) {
        return 0;
    }
    return static_cast<uint32_t>(std::min<int64_t>(elapsed_us, UINT32_MAX));
}

uint32_t average_us(uint64_t total_us, uint32_t count)
{
    if (count == 0) {
        return 0;
    }
    return static_cast<uint32_t>(std::min<uint64_t>(total_us / count, UINT32_MAX));
}

void reset_window(DisplayPerfContext &context, int64_t now_us)
{
    context.window_start_us = now_us;
    context.refresh_total_us = 0;
    context.render_total_us = 0;
    context.flush_submit_total_us = 0;
    context.flush_wait_total_us = 0;
    context.flushed_pixels = 0;
    context.refresh_count = 0;
    context.redraw_count = 0;
    context.flush_count = 0;
    context.flush_wait_count = 0;
    context.flush_wait_blocked_count = 0;
    context.refresh_max_us = 0;
    context.render_max_us = 0;
    context.flush_submit_max_us = 0;
    context.flush_wait_max_us = 0;
}

void display_event_callback(lv_event_t *event)
{
    auto *context = static_cast<DisplayPerfContext *>(lv_event_get_user_data(event));
    if (context == nullptr) {
        return;
    }

    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        context->refresh_start_us = esp_timer_get_time();
        break;

    case LV_EVENT_REFR_READY: {
        const int64_t now_us = esp_timer_get_time();
        const uint32_t elapsed_us = saturating_us(now_us - context->refresh_start_us);
        context->refresh_total_us += elapsed_us;
        context->refresh_max_us = std::max(context->refresh_max_us, elapsed_us);
        ++context->refresh_count;
        break;
    }

    case LV_EVENT_RENDER_START:
        context->render_start_us = esp_timer_get_time();
        break;

    case LV_EVENT_RENDER_READY: {
        const int64_t now_us = esp_timer_get_time();
        const uint32_t elapsed_us = saturating_us(now_us - context->render_start_us);
        context->render_total_us += elapsed_us;
        context->render_max_us = std::max(context->render_max_us, elapsed_us);
        ++context->redraw_count;
        break;
    }

    case LV_EVENT_FLUSH_START: {
        context->flush_submit_start_us = esp_timer_get_time();
        const auto *area = static_cast<const lv_area_t *>(lv_event_get_param(event));
        if (area != nullptr) {
            context->flushed_pixels += static_cast<uint64_t>(lv_area_get_size(area));
        }
        ++context->flush_count;
        break;
    }

    case LV_EVENT_FLUSH_FINISH: {
        const int64_t now_us = esp_timer_get_time();
        const uint32_t elapsed_us = saturating_us(now_us - context->flush_submit_start_us);
        context->flush_submit_total_us += elapsed_us;
        context->flush_submit_max_us = std::max(context->flush_submit_max_us, elapsed_us);
        break;
    }

    case LV_EVENT_FLUSH_WAIT_START:
        context->flush_wait_start_us = esp_timer_get_time();
        break;

    case LV_EVENT_FLUSH_WAIT_FINISH: {
        const int64_t now_us = esp_timer_get_time();
        const uint32_t elapsed_us = saturating_us(now_us - context->flush_wait_start_us);
        context->flush_wait_total_us += elapsed_us;
        context->flush_wait_max_us = std::max(context->flush_wait_max_us, elapsed_us);
        ++context->flush_wait_count;
        if (elapsed_us >= FLUSH_WAIT_BLOCKED_THRESHOLD_US) {
            ++context->flush_wait_blocked_count;
        }
        break;
    }

    default:
        break;
    }
}

void report_timer_callback(lv_timer_t *timer)
{
    auto *context = static_cast<DisplayPerfContext *>(lv_timer_get_user_data(timer));
    if (context == nullptr || context->display == nullptr) {
        return;
    }

    const int64_t now_us = esp_timer_get_time();
    const uint64_t window_us = static_cast<uint64_t>(std::max<int64_t>(now_us - context->window_start_us, 1));
    uint32_t fps = 0;
    if (esp_lv_adapter_get_fps(context->display, &fps) != ESP_OK) {
        fps = 0;
    }

    const uint64_t kilo_pixels_per_second = context->flushed_pixels * 1000ULL / window_us;
    const uint32_t strips_x10_per_redraw = context->redraw_count
        ? static_cast<uint32_t>(static_cast<uint64_t>(context->flush_count) * 10ULL / context->redraw_count)
        : 0;

    ESP_LOGI(
        TAG,
        "fps=%" PRIu32 " refresh=%" PRIu32 " redraw=%" PRIu32
        " flush=%" PRIu32 " strips=%" PRIu32 ".%" PRIu32 " kpix/s=%" PRIu64
        " us refresh=%" PRIu32 "/%" PRIu32 " render=%" PRIu32 "/%" PRIu32
        " submit=%" PRIu32 "/%" PRIu32 " wait=%" PRIu32 "/%" PRIu32
        " blocked=%" PRIu32 "/%" PRIu32 " core=%d stack=%u internal=%u/%u psram=%u/%u",
        fps,
        context->refresh_count,
        context->redraw_count,
        context->flush_count,
        strips_x10_per_redraw / 10,
        strips_x10_per_redraw % 10,
        kilo_pixels_per_second,
        average_us(context->refresh_total_us, context->refresh_count),
        context->refresh_max_us,
        average_us(context->render_total_us, context->redraw_count),
        context->render_max_us,
        average_us(context->flush_submit_total_us, context->flush_count),
        context->flush_submit_max_us,
        average_us(context->flush_wait_total_us, context->flush_wait_count),
        context->flush_wait_max_us,
        context->flush_wait_blocked_count,
        context->flush_wait_count,
        xPortGetCoreID(),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT))
    );

    reset_window(*context, now_us);
}

}  // namespace

esp_err_t display_perf_monitor_start(lv_display_t *display)
{
    if (display == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (perf.started) {
        return perf.display == display ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = esp_lv_adapter_fps_stats_enable(display, true);
    if (result != ESP_OK) {
        return result;
    }

    result = esp_lv_adapter_lock(-1);
    if (result != ESP_OK) {
        (void)esp_lv_adapter_fps_stats_enable(display, false);
        return result;
    }

    perf.display = display;
    reset_window(perf, esp_timer_get_time());
    lv_display_add_event_cb(display, display_event_callback, LV_EVENT_ALL, &perf);
    perf.report_timer = lv_timer_create(
        report_timer_callback,
        CONFIG_BROOKESIA_DISPLAY_PERF_LOG_PERIOD_MS,
        &perf
    );

    if (perf.report_timer == nullptr) {
        (void)lv_display_remove_event_cb_with_user_data(display, display_event_callback, &perf);
        perf.display = nullptr;
        esp_lv_adapter_unlock();
        (void)esp_lv_adapter_fps_stats_enable(display, false);
        return ESP_ERR_NO_MEM;
    }

    perf.started = true;
    esp_lv_adapter_unlock();

    ESP_LOGI(
        TAG,
        "Enabled %d ms display profiling (completed-frame FPS + LVGL render/flush events)",
        CONFIG_BROOKESIA_DISPLAY_PERF_LOG_PERIOD_MS
    );
    return ESP_OK;
}

#else

esp_err_t display_perf_monitor_start(lv_display_t *display)
{
    return display == nullptr ? ESP_ERR_INVALID_ARG : ESP_ERR_NOT_SUPPORTED;
}

#endif
