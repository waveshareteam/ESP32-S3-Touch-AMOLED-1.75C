# 02_GFX_AsciiTable

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Draws the built-in Arduino_GFX font as a character table.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board

## How to Run

1. Install the `esp32` board package and make [`examples/arduino/libraries/`](../../libraries/)
   available as your sketchbook `libraries` folder.
2. Select **ESP32S3 Dev Module** and set Flash Size **16MB**, Partition Scheme
   **16M Flash (3MB APP/9.9MB FATFS)**, PSRAM **OPI PSRAM**, USB CDC On Boot **Enabled**.
3. Open `03_GFX_AsciiTable.ino` and upload.

## Expected Result

The display shows a static table of white characters with green column numbers and blue row numbers.

## Troubleshooting

None yet.
