# Immersive Block

[English](README.md) | [简体中文](README_ZH.md)

This ESP-IDF example renders movable LVGL shapes on the AMOLED display and uses
QMI8658 acceleration data to move them as the board tilts.

## Requirements

- ESP32-S3-Touch-AMOLED-1.75C
- ESP-IDF `v5.5.5` or `v6.0.2`
- USB connection for flashing and monitoring

The project uses the managed Waveshare BSP, the managed QMI8658 component, and
LVGL 9.5.0 as declared in [`main/idf_component.yml`](main/idf_component.yml).

## Build Entry Point

The maintained application source is [`main/main.c`](main/main.c). From an
ESP-IDF environment, configure the target and build this project with:

```text
idf.py set-target esp32s3
idf.py build
```

Repository CI builds this example on both supported ESP-IDF release lines. See
the [repository README](../../../README.md) for the supported versions.

## Runtime Notes

- Keep the board stationary while the initial accelerometer calibration runs.
- Tap the on-screen control to request recalibration when needed.
- CI validates compilation and packaging; display, touch, and motion behavior
  still require validation on the physical board.

## Troubleshooting

If flashing or monitoring fails, verify the USB connection, select the correct
serial port, and retry with the complete firmware package produced by the same
CI build.
