# 07_ES8311

[English](README.md) | [简体中文](README_ZH.md)

## 功能

通过 ES8311 播放一段内置音频，随后把麦克风的声音回放到喇叭。

## 硬件需求

- ESP32-S3-Touch-AMOLED-1.75C 开发板
- 喇叭

## 怎么运行

1. 安装 `esp32` 开发板包，并把 [`examples/arduino/libraries/`](../../libraries/) 作为项目文件夹下的
   `libraries` 目录使用。
2. 开发板选 **ESP32S3 Dev Module**，Flash Size 选 **32MB**，Partition Scheme 选
   **32M Flash (4.8MB APP/22MB FATFS)**，PSRAM 选 **OPI PSRAM**，USB CDC On Boot 选 **Enabled**。
3. 打开 `15_ES8311.ino` 并上传。

## 预期现象

复位后喇叭播放一遍内置音频，之后回放麦克风采集到的声音。屏幕不亮。

## 常见问题

- 如果喇叭啸叫，请调低草图开头的 `EXAMPLE_VOICE_VOLUME`。
