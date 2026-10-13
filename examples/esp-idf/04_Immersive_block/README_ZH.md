# 沉浸式方块

[English](README.md) | [简体中文](README_ZH.md)

该 ESP-IDF 示例在 AMOLED 显示屏上绘制可移动的 LVGL 图形，并根据 QMI8658
加速度数据在开发板倾斜时移动这些图形。

## 要求

- ESP32-S3-Touch-AMOLED-1.75C
- ESP-IDF `v5.5.5` 或 `v6.0.2`
- 用于烧录和监视的 USB 连接

工程使用 [`main/idf_component.yml`](main/idf_component.yml) 中声明的 Waveshare 托管 BSP、
QMI8658 托管组件和 LVGL 9.5.0。

## 构建入口

维护中的应用源码为 [`main/main.c`](main/main.c)。在 ESP-IDF 环境中，可通过以下命令
设置目标并构建本工程：

```text
idf.py set-target esp32s3
idf.py build
```

仓库 CI 会在两个受支持的 ESP-IDF 版本系列上构建本示例。受支持的版本见
[仓库 README](../../../README_ZH.md)。

## 运行说明

- 初始加速度计校准期间请保持开发板静止。
- 需要时可点击屏幕控制项请求重新校准。
- CI 验证编译和打包；显示、触摸和运动行为仍需在实体开发板上验证。

## 故障排查

如果烧录或监视失败，请检查 USB 连接、选择正确串口，并使用同一次 CI 构建产生的完整
固件包重试。
