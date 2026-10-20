# 03_LVGL_AXP2101_ADC_Data

[English](README.md) | [简体中文](README_ZH.md)

## 功能

在 LVGL 文本标签中显示 AXP2101 的电源数据。草图还会开启热点 `ESP32-S3-Touch-AMOLED-1.75C-AP`，并可选连接你的 Wi-Fi 网络。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板
- 2.4 GHz Wi-Fi 网络（可选）
- 锂电池（可选）

## 怎么运行

1. 安装 `esp32` 开发板包，并把 [`examples/arduino/libraries/`](../../libraries/) 作为项目文件夹下的
   `libraries` 目录使用。
2. 开发板选 **ESP32S3 Dev Module**，Flash Size 选 **32MB**，Partition Scheme 选
   **32M Flash (4.8MB APP/22MB FATFS)**，PSRAM 选 **OPI PSRAM**，USB CDC On Boot 选 **Enabled**。
3. 打开 `05_LVGL_AXP2101_ADC_Data.ino` 并上传。

## 预期现象

屏幕显示持续刷新的充电状态以及电池、VBUS 和系统电压。

## 常见问题

- 要连接你的 Wi-Fi，请在草图开头填写 `ssid_sta` 和 `password_sta`。10 秒内连不上会跳过，继续运行。
