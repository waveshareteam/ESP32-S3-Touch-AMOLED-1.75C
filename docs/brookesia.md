# Brookesia

`examples/esp-idf/03_esp-brookesia/` is treated as a source-maintained rich UI example and is included in ESP-IDF CI.

Brookesia compatibility should be checked through CI before updating shared UI, LVGL, display, touch, or audio dependencies. If a future upstream Brookesia release changes the supported ESP-IDF range, update the example manifests and this note together.

## Runtime Notes

The Brookesia and rich LVGL examples use the ESP-IDF v6-compatible LVGL/Brookesia custom memory path. Keep `CONFIG_LV_MEM_CUSTOM` and `CONFIG_ESP_BROOKESIA_MEMORY_USE_CUSTOM` enabled unless the examples are re-tested on hardware across both ESP-IDF CI versions.

`examples/esp-idf/03_esp-brookesia/` remains in CI for compile and artifact coverage. Runtime issues such as display, touch, PMU, or sensor initialization still need board-level verification after CI is green.
