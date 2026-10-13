# 03_esp-brookesia

[English](README.md) | [简体中文](README_ZH.md)

## 功能

运行 ESP-Brookesia 手机风格界面，以及随示例提供的 SquareLine 演示应用。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板

## 怎么运行

```bash
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

屏幕显示带状态栏时钟和应用图标的主屏，可通过触摸切换界面、打开演示应用。

## 常见问题

- LVGL 任务使用 40 KB 的 PSRAM 栈（`main/main.cpp` 的 `LVGL_TASK_STACK_SIZE`）。除非已在全部受支持的 ESP-IDF 版本上重新做过硬件验证，否则不要改动它和 `sdkconfig.defaults` 里的内存相关选项。
- 更新 LVGL、显示、触摸、音频或 Brookesia 依赖前，先确认本示例在全部受支持的 ESP-IDF 版本上仍能构建。如果新的 Brookesia 版本改变了支持的 ESP-IDF 范围，请同步更新示例清单和本文档。
- 构建通过不代表显示、触摸、PMU 和传感器初始化正常，这些仍需在开发板上检查。
