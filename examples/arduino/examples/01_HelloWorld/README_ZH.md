# 01_HelloWorld

[English](README.md) | [简体中文](README_ZH.md)

## 功能

用 Arduino_GFX 初始化 AMOLED 屏并打印 `Hello World!`。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板

## 怎么运行

1. 安装 `esp32` 开发板包，并把 [`examples/arduino/libraries/`](../../libraries/) 作为项目文件夹下的
   `libraries` 目录使用。
2. 开发板选 **ESP32S3 Dev Module**，Flash Size 选 **32MB**，Partition Scheme 选
   **32M Flash (4.8MB APP/22MB FATFS)**，PSRAM 选 **OPI PSRAM**，USB CDC On Boot 选 **Enabled**。
3. 打开 `01_HelloWorld.ino` 并上传。

## 预期现象

屏幕先显示红色的 `Hello World!` 5 秒，之后以随机位置、颜色和字号不断重复打印。

## 常见问题

暂无。
