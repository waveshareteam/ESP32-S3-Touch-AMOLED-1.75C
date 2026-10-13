# 03_esp-brookesia

[English](README.md) | [简体中文](README_ZH.md)

## What It Does

Runs the ESP-Brookesia phone-style UI on the AMOLED display: a home screen with app icons, a
status bar with a clock, and the bundled SquareLine demo app.

## Hardware Required

- ESP32-S3-Touch-AMOLED-1.75C development board
- USB cable for flashing and monitoring

## How to Run

```bash
idf.py set-target esp32s3 build flash monitor
```

Dependencies are declared in [`main/idf_component.yml`](main/idf_component.yml) and downloaded
on the first build. `brookesia_core` and `brookesia_app_squareline_demo` are local components
under [`components/`](components/).

## Expected Result

The display shows the Brookesia home screen with a status bar and app icons. Swiping and
tapping navigate between screens, and tapping the SquareLine demo icon opens the demo app.
The status bar clock updates once per second. The serial monitor prints
`Display ESP-Brookesia phone demo` at startup. (pending hardware confirmation)

## Troubleshooting

- **Stack overflow or allocation failure after changing the UI.** The example gives the LVGL
  task a 40 KB stack placed in PSRAM (`LVGL_TASK_STACK_SIZE` in `main/main.cpp`) because
  Brookesia screen creation exceeds the adapter default. Keep the stack size and the memory
  options in `sdkconfig.defaults` unless the change is re-tested on hardware with every
  supported ESP-IDF version.
- **Updating LVGL, display, touch, audio or Brookesia dependencies.** This is the largest UI
  example in the repository, so check that it still builds with every supported ESP-IDF version
  before updating shared dependencies. If a newer upstream Brookesia release changes the
  supported ESP-IDF range, update the example manifests and this README together.
- **The build passes but the board misbehaves.** A successful build only covers compilation
  and packaging. Display, touch, PMU and sensor initialization still need to be checked on the
  board.
