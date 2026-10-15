# Immersive Block

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Renders movable LVGL shapes on the AMOLED display and uses QMI8658 acceleration data to move them as the board tilts.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board

## How to Run

```bash
idf.py set-target esp32s3 build flash monitor
```

## Expected Result

After a short level calibration, shapes appear on the display and slide toward the lower side when the board is tilted.

## Troubleshooting

- Keep the board stationary on a level surface while the initial calibration runs. Press the BOOT button to recalibrate.
- If flashing or monitoring fails, verify the USB connection and select the correct serial port.
