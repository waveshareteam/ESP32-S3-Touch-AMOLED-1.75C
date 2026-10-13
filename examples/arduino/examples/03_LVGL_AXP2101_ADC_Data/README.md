# 03_LVGL_AXP2101_ADC_Data

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Shows AXP2101 power data in an LVGL label. The sketch also joins a Wi-Fi network and starts an access point.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board
- 2.4 GHz Wi-Fi network
- Lithium battery (optional)

## How to Run

1. Install the `esp32` board package and make [`examples/arduino/libraries/`](../../libraries/)
   available as your sketchbook `libraries` folder.
2. Select **ESP32S3 Dev Module** and set Flash Size **16MB**, Partition Scheme
   **16M Flash (3MB APP/9.9MB FATFS)**, PSRAM **OPI PSRAM**, USB CDC On Boot **Enabled**.
3. Open `05_LVGL_AXP2101_ADC_Data.ino` and upload.

## Expected Result

Once Wi-Fi is connected, the display shows continuously refreshed charging state and battery, VBUS and system voltages.

## Troubleshooting

- The sketch waits until Wi-Fi is connected. Set `ssid_sta` and `password_sta` at the top of the sketch to your own network before uploading.
