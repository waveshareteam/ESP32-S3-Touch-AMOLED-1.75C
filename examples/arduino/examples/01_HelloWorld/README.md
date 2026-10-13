# 01_HelloWorld

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Initializes the AMOLED panel with Arduino_GFX and prints `Hello World!`.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board

## How to Run

1. Install the `esp32` board package and make [`examples/arduino/libraries/`](../../libraries/)
   available as your sketchbook `libraries` folder.
2. Select **ESP32S3 Dev Module** and set Flash Size **16MB**, Partition Scheme
   **16M Flash (3MB APP/9.9MB FATFS)**, PSRAM **OPI PSRAM**, USB CDC On Boot **Enabled**.
3. Open `01_HelloWorld.ino` and upload.

## Expected Result

A red `Hello World!` appears for five seconds, then the text is repeated at random positions, colors and sizes.

## Troubleshooting

None yet.
