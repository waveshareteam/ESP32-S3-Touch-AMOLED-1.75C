# 05_LVGL_Widgets

[English](README.md) | [简体中文](README_ZH.md)

## 功能

运行带触摸输入的 LVGL 音乐播放器演示。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板

## 怎么运行

1. 安装 `esp32` 开发板包，并把 [`examples/arduino/libraries/`](../../libraries/) 作为项目文件夹下的
   `libraries` 目录使用。
2. 开发板选 **ESP32S3 Dev Module**，Flash Size 选 **16MB**，Partition Scheme 选
   **16M Flash (3MB APP/9.9MB FATFS)**，PSRAM 选 **OPI PSRAM**，USB CDC On Boot 选 **Enabled**。
3. 打开 `13_LVGL_Widgets.ino` 并上传。

## 预期现象

屏幕显示 LVGL 音乐播放器，可以点击和滑动操作。

## 常见问题

- 想运行其他演示，在草图中启用 `lv_demo_widgets()`、`lv_demo_benchmark()` 或 `lv_demo_stress()`。
