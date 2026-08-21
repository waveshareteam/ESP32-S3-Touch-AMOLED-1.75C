# Components

[English](components.md) | [简体中文](components_ZH.md)

The examples use the managed Waveshare board component
`waveshare/esp32_s3_touch_amoled_1_75c` (`^3.0.0`). The current release supports
ESP-IDF 5.5 and later; CI verifies the product examples with ESP-IDF `v5.5.5`
and `v6.0.2`.

Repository-local components have distinct ownership and are intentionally retained:

- `brookesia_app_squareline_demo` is product UI feature code used by the Brookesia example.
- `brookesia_core` is an embedded upstream framework. Preserve its source, attribution, and
  documentation unless an explicit upstream synchronization is performed.
- `bsp_extra` is board-local audio glue layered on the managed BSP and codec APIs.

Future component work should prefer managed Waveshare and Espressif components when an equivalent component is available and compatible with the selected CI matrix.

Keep repository-local glue such as board-specific demo composition, temporary compatibility code,
and example-only assets near the example that consumes it. Reusable BSP, display, touch, sensor,
audio, and bus fixes should move to the shared component source first when possible. Component names
or directory placement alone are not sufficient evidence for removal.

## Hardware Cross-Check Boundary

A read-only comparison against the repository schematic confirms LCD QSPI data GPIO4–GPIO7,
SCLK GPIO38, CS GPIO12, reset GPIO1, and the 466×466 resolution in the Arduino board header.
It also confirms touch I2C SDA GPIO15/SCL GPIO14, interrupt GPIO11, and reset GPIO2; the reset
definitions have been corrected so LCD reset and touch reset are no longer conflated. Audio values
remain BCLK GPIO9, LRCK GPIO45, DIN GPIO10, MCLK GPIO16, DOUT GPIO8, and PA GPIO46. QMI8658,
AXP, and other signals not repeated in the local header remain authoritative in the schematic and
managed BSP. A successful build does not verify runtime behavior on physical hardware.
