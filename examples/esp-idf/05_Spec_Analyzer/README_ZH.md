# 05_Spec_Analyzer

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 ES7210 采集板载麦克风，在屏幕上实时绘制音频频谱。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板

## 怎么运行

```bash
idf.py set-target esp32s3 build flash monitor
```

## 预期现象

屏幕显示 64 根彩色竖条，随开发板附近的声音起伏。

## 常见问题

暂无。
