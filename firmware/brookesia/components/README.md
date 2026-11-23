# Brookesia firmware components

[简体中文](README_ZH.md)

This directory holds the applications, the shared services, the board support
package, and the pinned upstream overrides of the ESP32-S3-Touch-AMOLED-1.75C
firmware. Each group below has a different maintenance boundary.

The project `CMakeLists.txt` adds the Brookesia core and the SquareLine demo
from `examples/esp-idf/03_esp-brookesia/components/` through
`EXTRA_COMPONENT_DIRS`. They are not copied here.

## Applications

| Directory | Launcher application |
| --- | --- |
| [`brookesia_app_calculator/`](brookesia_app_calculator/) | Calculator |
| [`ButtonTest/`](ButtonTest/) | Raw level test of the PWR key (GPIO3) and the BOOT key (GPIO0) |
| [`Crosshair/`](Crosshair/) | Circular display and touch alignment target |
| [`draw/`](draw/) | DrawPanel |
| [`Gravitysphere/`](Gravitysphere/) | Gravity ball driven by the QMI8658 |
| [`MusicPlayer/`](MusicPlayer/) | MP3 and WAV playback from SPIFFS |
| [`Settings/`](Settings/) | Wi-Fi, sound, display brightness and screen timeout, battery, and device information |
| [`SpecAnalyzer/`](SpecAnalyzer/) | Live spectrum of the ES7210 microphones |
| [`XiaozhiApp/`](XiaozhiApp/) | AIChats voice and text application |

## Shared services

| Directory | Responsibility |
| --- | --- |
| [`bsp_extra/`](bsp_extra/) | ES8311 and ES7210 audio, PA diagnostics, file playback, and the exclusive audio session owner |
| [`display_power/`](display_power/) | Screen brightness and the screen timeout: dim, off, touch wake, and the PWR key toggle |
| [`storage_service/`](storage_service/) | SD card leases. The 1.75C has no SD slot, so every lease fails and MusicPlayer falls back to SPIFFS. |
| [`system_status/`](system_status/) | AXP2101 battery and Wi-Fi status for the status bar and Settings |

Applications use these public APIs. An application must not open a second
codec, set the panel brightness directly, or keep its own Wi-Fi or battery
state.

## Local overrides

| Directory | Upstream base and local change |
| --- | --- |
| [`esp_audio_player_safe/`](esp_audio_player_safe/) | `chmorgan/esp-audio-player` 1.1.0 with a deterministic cleanup of the worker and the queued file |
| [`esp_xiaozhi_safe/`](esp_xiaozhi_safe/) | Espressif `esp_xiaozhi` 0.1.1 with bounded network and event queue waits |

Each override keeps the public version and API of its upstream component. Keep
the override manifest, the source, the wrapper README, and the `override_path`
of the consumer in sync. Do not replace an override with a registry download
until upstream has the fix, or the fix is no longer necessary.

## Board support package

`waveshare__esp32_s3_touch_amoled_1_75/` is the Waveshare 1.75 board support
package, changed for the 1.75C pins. It keeps the 1.75 component name. The
1.75C changes are:

- I2S MCLK on GPIO16, LCD reset on GPIO1, and touch reset on GPIO2.
- No TCA9554 IO expander. `BSP_PWR_KEY_IO` (GPIO3) and `BSP_BOOT_KEY_IO`
  (GPIO0) name the key inputs.
- No microSD slot. `BSP_CAPS_SDCARD` is `0`, and `bsp_sdcard_mount()` returns
  `ESP_ERR_NOT_SUPPORTED`.

Code under an upstream component or a `third_party/` directory keeps its
upstream license, attribution, README, and names. Review media and prompt
assets under their own licenses before you distribute them.

## Validation

After a change to an application or a shared service, build the whole project
and repeat the related sections of
[`../HARDWARE_VALIDATION.md`](../HARDWARE_VALIDATION.md). A change to audio or
to application cleanup also needs fast enter and exit tests and switches
between applications. A build does not prove resource ownership on the board.
