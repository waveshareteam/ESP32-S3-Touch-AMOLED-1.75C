<div align="center">
  <h1>ESP32-S3-Touch-AMOLED-1.75C</h1>
  <p><strong>ESP32-S3 1.75-inch 466 x 466 QSPI AMOLED touch development board</strong></p>
  <p><strong>English</strong> | <a href="README_ZH.md">简体中文</a></p>
  <p>
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/actions/workflows/build.yml"><img alt="Build" src="https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/actions/workflows/build.yml/badge.svg"></a>
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/releases/latest"><img alt="Latest Release" src="https://img.shields.io/github/v/release/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C"></a>
    <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C"></a>
  </p>
  <p>
    <a href="https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm">🌐 Product</a> &middot;
    <a href="https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/releases/latest">📦 Firmware</a> &middot;
    <a href="examples/esp-idf/">🧩 ESP-IDF</a> &middot;
    <a href="examples/arduino/examples/">🔧 Arduino</a> &middot;
    <a href="hardware/">📚 Documentation</a>
  </p>
  <p><img src="docs/images/esp32-s3-touch-amoled-1.75c.jpg" alt="ESP32-S3-Touch-AMOLED-1.75C development board" width="500"></p>
</div>

---

## ✨ Overview

The ESP32-S3-Touch-AMOLED-1.75C is a watch-style development board that combines an ESP32-S3
with a square AMOLED touch display, a six-axis motion sensor, power management, and audio input
and output. This repository provides its ESP-IDF and Arduino examples, ready-to-flash example
firmware, and the schematic.

| Feature | Device / interface |
| --- | --- |
| MCU | ESP32-S3 |
| Memory | 32 MB Flash, 8 MB PSRAM |
| Display | 1.75-inch 466 x 466 QSPI AMOLED using CO5300 |
| Touch | CST9217 capacitive touch controller over I2C |
| Power management | AXP2101 |
| Motion sensor | QMI8658 six-axis IMU |
| Audio | Dual digital microphones via ES7210 ADC; ES8311 audio codec |
| Board support | Managed component: `waveshare/esp32_s3_touch_amoled_1_75c` |

## 📦 Quick Start

The fastest way to try an example is a ready-to-flash package from the
[latest release](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/releases/latest).

1. Download the `*-combined.zip` package for the example and framework version you need.
2. Extract the archive and install esptool with `python -m pip install esptool`.
3. Connect the board over USB.
4. Run `flash_combined.bat COMx` on Windows or `./flash_combined.sh /dev/ttyACM0` on Linux.
5. Reset the board if it does not restart automatically.

Factory firmware (restores the out-of-box demo): download the file whose name contains `FactoryOnly` from the [`firmware`](firmware/) folder.

## 🧪 Examples

### ESP-IDF

| Example | Main feature |
| --- | --- |
| [01_AXP2101](examples/esp-idf/01_AXP2101/) | Power management and battery telemetry |
| [02_lvgl_demo_v9](examples/esp-idf/02_lvgl_demo_v9/) | LVGL 9 display demo |
| [03_esp-brookesia](examples/esp-idf/03_esp-brookesia/) | ESP-Brookesia application UI |
| [04_Immersive_block](examples/esp-idf/04_Immersive_block/) | Motion-driven LVGL graphics demo |
| [05_Spec_Analyzer](examples/esp-idf/05_Spec_Analyzer/) | Microphone spectrum analyzer |

### Arduino

| Example | Main feature |
| --- | --- |
| [01_HelloWorld](examples/arduino/examples/01_HelloWorld/) | Display initialization |
| [02_GFX_AsciiTable](examples/arduino/examples/02_GFX_AsciiTable/) | GFX text and glyph rendering |
| [03_LVGL_AXP2101_ADC_Data](examples/arduino/examples/03_LVGL_AXP2101_ADC_Data/) | LVGL power telemetry UI |
| [04_LVGL_QMI8658_ui](examples/arduino/examples/04_LVGL_QMI8658_ui/) | LVGL IMU data UI |
| [05_LVGL_Widgets](examples/arduino/examples/05_LVGL_Widgets/) | LVGL widgets, touch input, and display interaction |
| [06_ES7210](examples/arduino/examples/06_ES7210/) | ES7210 microphone input |
| [07_ES8311](examples/arduino/examples/07_ES8311/) | ES8311 audio output |

Libraries used by the sketches are bundled in [`examples/arduino/libraries`](examples/arduino/libraries/).

## 🛠️ Development Environment

| Framework | Version | Examples |
| --- | --- | ---: |
| ESP-IDF | `v5.5.5` | 5 |
| ESP-IDF | `v6.0.2` | 5 |
| Arduino-ESP32 | `3.3.11` | 7 |

## 📚 Resources

- [Schematic](hardware/schematic/)
- [Product page](https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm)
- [Wiki](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75C)

## 🤝 Support

- Example or repository problem: [open an issue](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75C/issues/new/choose).
- Product usage, hardware or order questions: [Waveshare support](https://service.waveshare.com/).

## 📄 License

Apache License 2.0. See [LICENSE](LICENSE).
