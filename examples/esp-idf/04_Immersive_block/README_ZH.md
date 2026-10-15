# 沉浸式方块

[English](README.md) | [简体中文](README_ZH.md)

## 功能

在 AMOLED 显示屏上绘制可移动的 LVGL 图形，并根据 QMI8658 加速度数据在开发板倾斜时移动这些图形。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板

## 怎么运行

```bash
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

完成一次简短的水平校准后，屏幕上出现若干图形，开发板倾斜时图形滑向较低的一侧。

## 常见问题

- 初始校准期间请把开发板静止放在水平面上。按 BOOT 键可以重新校准。
- 如果烧录或监视失败，请检查 USB 连接并选择正确的串口。
