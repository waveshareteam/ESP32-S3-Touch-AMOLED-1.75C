# 02_lvgl_demo_v9

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs the LVGL 9 benchmark demo on the display through the managed Waveshare BSP.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board

## How to Run

```bash
idf.py set-target esp32s3 build flash monitor
```

## Expected Result

The display plays the benchmark scenes in sequence and then shows a frame-rate summary.

## Troubleshooting

- To run another demo, enable `lv_demo_music()` or `lv_demo_widgets()` in `main/main.c`.
