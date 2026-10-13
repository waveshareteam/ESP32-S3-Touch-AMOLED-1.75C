# 02_lvgl_demo_v9

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 Waveshare 托管 BSP 在屏幕上运行 LVGL 9 的 benchmark 演示。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板

## 怎么运行

```bash
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

屏幕依次播放 benchmark 各个场景，结束后显示帧率汇总。

## 常见问题

- 想运行其他演示，在 `main/main.c` 中启用 `lv_demo_music()` 或 `lv_demo_widgets()`。
