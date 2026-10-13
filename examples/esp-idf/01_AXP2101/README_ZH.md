# 01_AXP2101

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 I2C 读取 AXP2101 电源管理芯片，在日志中输出各路电源、电池和充电数据。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板
- 锂电池（可选）

## 怎么运行

```bash
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

串口监视器先打印一次各路电源的开关状态和电压，之后每秒打印充电状态以及电池、VBUS 和系统电压。屏幕不亮。

## 常见问题

- `PMU READ FAILED!`：检查 `idf.py menuconfig` 中 **AXP2101 PMU Configuration** 的 I2C 引脚（SDA GPIO15、SCL GPIO14）。
