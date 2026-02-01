# GitHub Copilot / AI agent instructions for this repo ✅

Purpose: give an AI coding agent the minimal, actionable knowledge to be productive working on Waveshare's "ESP32-S3-Touch-AMOLED-1.75C" firmware & examples.

---

## Quick architecture overview 🔧

- This repo is a board-support + example collection for the Waveshare ESP32‑S3 1.75" AMOLED (466×466) device.
- Major pieces:
  - BSP component: `components/esp32_s3_touch_amoled_1_75c` (exposes `bsp_...` APIs for display, touch, power, audio).
  - ESP-IDF examples: `examples/ESP-IDF-v5.5/*` (LVGL demo, spec analyzer, audio demos, etc.).
  - Arduino examples: `examples/Arduino-v3.3.5/examples/*` (.ino sketches using the libraries folder)
  - Libraries: `XPowersLib`, `SensorLib`, and `lvgl` (LVGL GUI library integrated as component/library).
- Integration points: BSP uses `esp_lcd` and touch APIs, LVGL is configured through Kconfig (sdkconfig.defaults), audio uses I2S + a power amp pin (`BSP_POWER_AMP_IO`).

---

## Developer workflows (explicit commands) 🛠️

- ESP-IDF (recommended matching examples folder: v5.5):
  1. Install ESP-IDF v5.x via Espressif docs and set up environment.
  2. cd into an example, e.g. `examples/ESP-IDF-v5.5/02_lvgl_demo_v9`
  3. (optional) `idf.py set-target esp32s3`
  4. Edit configuration: `idf.py menuconfig` (many LVGL and BSP options provided in `sdkconfig.defaults`)
  5. Build: `idf.py build`
  6. Flash + monitor: `idf.py -p <PORT> flash monitor`
- Tests: some examples include `pytest_*.py` (use the ESP-IDF pytest harness where present).
- Arduino examples: open `.ino` under `examples/Arduino-v3.3.5/examples/` in Arduino IDE or PlatformIO using Arduino-ESP32 S3 core (v3.3.5 folder indicates tested core version).

---

## Project-specific conventions & patterns 📐

- BSP API (single place to learn): `components/esp32_s3_touch_amoled_1_75c/include/bsp/esp32_s3_touch_amoled_1_75c.h`.
  - Key APIs: `bsp_display_start()`, `bsp_display_start_with_config()`, `bsp_display_backlight_on()`, `bsp_display_brightness_set()`, `bsp_display_lock(timeout_ms)`, `bsp_display_unlock()`, `bsp_display_get_input_dev()`.
  - If you need the display without LVGL, look for BSP variants with the `noglib` suffix (header notes mention this).
- LVGL rules:
  - LVGL is configured by Kconfig (`sdkconfig.defaults` under each example), e.g., `CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT`.
  - The BSP initializes LVGL input device in `bsp_display_start()`.
  - **Always** use `bsp_display_lock(timeout_ms)` before calling LVGL APIs from non-LVGL tasks, then `bsp_display_unlock()` when done. Example:

```c
lv_display_t *disp = bsp_display_start();
if (bsp_display_lock(1000)) {
  // Safe to call lv_...()
  lv_label_set_text(lv_label_create(lv_scr_act()), "Hello");
  bsp_display_unlock();
}
```

- Display constants live in `components/.../include/bsp/display.h` (e.g. `BSP_LCD_H_RES`, `BSP_LCD_V_RES`, color format macros).
- Hardware and pin constants are defined in component headers (e.g. `BSP_POWER_AMP_IO` for audio amplifier).

---

## Where to look for examples and canonical patterns 🔎

- LVGL-centric example: `examples/ESP-IDF-v5.5/02_lvgl_demo_v9/main/` (shows LVGL init and demo setup).
- Display + audio integration: `examples/ESP-IDF-v5.5/05_Spec_Analyzer/main/main.c` (shows using `bsp_display_start()` and audio processing).
- BSP implementation & configuration: `examples/*/components/esp32_s3_touch_amoled_1_75c/` (C file + headers + `bsp/display.h`).
- Arduino usage: inspect `examples/Arduino-v3.3.5/examples/` for simple `.ino` sketches.

---

## Common gotchas & quick checks ⚠️

- Display resolution is 466×466 and default color format is RGB565 (16 bpp) — use those constants; incorrect assumptions break LVGL scaling and drivers.
- Many runtime behaviors are governed by Kconfig (`sdkconfig.defaults`) — changing LVGL/imemory options requires re-running `idf.py build` and may need `menuconfig` adjustments.
- If adding the BSP to other projects, either place the component under `components/` or use `EXTRA_COMPONENT_DIRS` (CMakeLists shows commented example).

---

## How to update this file 📝

- Keep it short and actionable (20–50 lines). Reference exact file paths and one-line example commands.
- If you (human) see missing specifics (e.g., preferred ESP-IDF patch version), tell the bot which files to inspect and what assumption to change.

---

Feedback? Please point out any missing developer flows or examples you want prioritized and I will iterate. 💡
