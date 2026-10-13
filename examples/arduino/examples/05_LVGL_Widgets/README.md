# 05_LVGL_Widgets

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs the LVGL music player demo with touch input.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board

## How to Run

1. Install the `esp32` board package and make [`examples/arduino/libraries/`](../../libraries/)
   available as your sketchbook `libraries` folder.
2. Select **ESP32S3 Dev Module** and set Flash Size **16MB**, Partition Scheme
   **16M Flash (3MB APP/9.9MB FATFS)**, PSRAM **OPI PSRAM**, USB CDC On Boot **Enabled**.
3. Open `13_LVGL_Widgets.ino` and upload.

## Expected Result

The display shows the LVGL music player, which responds to taps and swipes.

## Troubleshooting

- To run another demo, enable `lv_demo_widgets()`, `lv_demo_benchmark()` or `lv_demo_stress()` in the sketch.
