# Brookesia firmware for the 1.75C

[简体中文](README_ZH.md)

This project is the Brookesia phone-style firmware for the
ESP32-S3-Touch-AMOLED-1.75C, with a screen timeout. It is a port of
[`firmware/brookesia`](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/tree/5481619/firmware/brookesia)
from the ESP32-S3-Touch-AMOLED-1.75 repository (commit 5481619).

It is not the 1.75C factory firmware. The factory image and its flash
instructions are in [`../README.md`](../README.md).

## Hardware

The firmware uses the board's 466 x 466 CO5300 QSPI AMOLED, the CST9217 touch
controller, the ES8311 speaker codec, the ES7210 microphone ADC, the AXP2101
power chip, the QMI8658 motion sensor, and the ESP32-S3 Wi-Fi.

The 1.75C differs from the 1.75 in these ways:

| Item | 1.75 | 1.75C |
| --- | --- | --- |
| I2S MCLK | GPIO42 | GPIO16 |
| LCD reset | GPIO39 | GPIO1 |
| Touch reset | GPIO40 | GPIO2 |
| PWR key input | TCA9554 EXIO4 | GPIO3, active high |
| microSD slot | Yes | No |

The board support package in
[`components/waveshare__esp32_s3_touch_amoled_1_75/`](components/waveshare__esp32_s3_touch_amoled_1_75/)
keeps its 1.75 component name and uses the 1.75C pins. Its SD functions return
`ESP_ERR_NOT_SUPPORTED`.

## Applications

The launcher has 10 applications:

- SquareLine demo.
- Gravitysphere, a ball that follows the QMI8658 accelerometer inside the round
  screen edge.
- Crosshair, a full-screen circular alignment target.
- Button Test, which shows the raw levels of the PWR key (GPIO3) and the BOOT
  key (GPIO0).
- Calculator.
- DrawPanel.
- SpecAnalyzer, a live spectrum of the ES7210 microphones.
- MusicPlayer, for MP3 and WAV files in `/spiffs/music`.
- Settings, with **WLAN**, **Sound**, **Display**, **Power**, and **About**
  pages.
- AIChats, the Xiaozhi voice assistant with WakeNet wake word, VAD, and Opus
  audio.

The 1.75 firmware also has Gallery, Recorder, VideoPlayer, a Settings storage
page, and AIChats chat history. All of them need a microSD card, so this port
removes them.

The SPIFFS image contains no music. To give MusicPlayer tracks, put MP3 or WAV
files in `spiffs/music/` before you build. The 6 MB `storage` partition also
holds the AIChats font.

## Screen timeout

The screen dims, and then turns off, when nobody touches it. The
[`display_power`](components/display_power/) component owns this behavior.

| Event | Result |
| --- | --- |
| No touch for the timeout minus the dim lead | The screen dims to 10% brightness. |
| No touch for the full timeout | The screen turns off. |
| A touch on a dim or dark screen | The screen wakes. Apps and gestures do not get that touch. |
| A PWR press of 50 ms to 1.5 s | The screen turns off, or wakes if it is dim or off. |
| A PWR hold of 4 s | The AXP2101 powers the board off. |

The dim lead is 10 s, or one third of the timeout if that is shorter. A 15 s
timeout dims at 10 s and turns off at 15 s. A 60 s timeout dims at 50 s.

Set the timeout in **Settings > Display > Screen timeout**. The choices are 15
seconds, 30 seconds, 1 minute, 2 minutes, 5 minutes, 10 minutes, and Never. The
brightness slider on the same page goes from 20% to 100%.

The firmware stores both values in the NVS namespace `display_power`:

| Key | Type | Default |
| --- | --- | --- |
| `brightness` | `u8`, percent | `100` |
| `timeout_s` | `u32`, seconds, `0` is Never | `60` |

While the screen is off, LVGL does not draw frames. At wake it redraws the
whole screen once.

## Known limits

- A PWR press in Button Test also turns the screen off. Press PWR again to wake
  it. Button Test still records the press.
- The boot screen shows 100% brightness. The saved brightness applies when the
  launcher is ready, about 7 s after reset.
- The partition table uses 16 MB of the 32 MB flash.

## Build and flash

Use ESP-IDF v5.5.4. The project uses the Brookesia core and the SquareLine
component in `examples/esp-idf/03_esp-brookesia/components/`, so build it inside
this repository.

1. Back up the factory firmware before you replace it. The source of the
   factory firmware is not public, so a backup is the only way back to it.

   ```bash
   esptool.py --chip esp32s3 -p <PORT> read_flash 0 0x2000000 factory-backup.bin
   ```

2. Build the project.

   ```bash
   idf.py -C firmware/brookesia set-target esp32s3 build
   ```

3. Flash all partitions the first time. AIChats needs the partition table,
   `srmodels.bin`, the application, and `storage.bin` to match.

   ```bash
   idf.py -C firmware/brookesia -p <PORT> flash monitor
   ```

4. After a code change, flash only the application. The NVS settings stay.

   ```bash
   idf.py -C firmware/brookesia -p <PORT> app-flash
   ```

The partition table reserves an 8 MB application, a 6 MB `storage` SPIFFS
partition, and a 960 KB ESP-SR model partition.

To restore the factory firmware, write the backup at offset `0x0`:

```bash
esptool.py --chip esp32s3 -p <PORT> write_flash 0x0 factory-backup.bin
```

## Launcher icons

The Crosshair and Button Test icons are 112 x 112 PNG files. The LVGL v9
`LVGLImage.py` script converts them to `ARGB8888` C data. After the build
resolves the dependencies, regenerate them with these commands:

```text
python managed_components/lvgl__lvgl/scripts/LVGLImage.py --ofmt C --cf ARGB8888 --name img_app_crosshair --output components/Crosshair/assets components/Crosshair/assets/img_app_crosshair.png
python managed_components/lvgl__lvgl/scripts/LVGLImage.py --ofmt C --cf ARGB8888 --name img_app_button_test --output components/ButtonTest/assets components/ButtonTest/assets/img_app_button_test.png
```

## Hardware validation

A build proves only that the code compiles. After a source change, run the
board checks in [`HARDWARE_VALIDATION.md`](HARDWARE_VALIDATION.md). The
component boundaries are in the [component guide](components/README.md).
