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
remain BCLK GPIO9, LRCK GPIO45, DIN GPIO10, MCLK GPIO16, DOUT GPIO8, and PA GPIO46. Top physical
PWR button routes to GPIO3 through an inverting MOSFET, while the bottom BOOT button routes to
GPIO0. QMI8658, AXP, and other signals not repeated in the local header remain authoritative in the
schematic and managed BSP. A successful build does not verify runtime behavior on physical hardware.

## Low-Level Hardware Architecture Notes

### Physical Buttons & PMIC Interrupt Line
- **PWR Button (GPIO3)**: Top button routes through an inverting BSS138 N-channel MOSFET. `gpio_get_level(GPIO_NUM_3)` returns `1` when pressed and `0` when released.
- **BOOT Button (GPIO0)**: Bottom button is the active-LOW boot switch.
- **PMIC Interrupt Routing**: The AXP2101 IRQ line does not connect to the ESP32. Firmware must poll GPIO3 directly for power button events instead of reading PMIC IRQ registers.

### Power Rail Topology & Quiescent Optimization
- **Active Regulators**: The board routes only DCDC1 (3.3V system logic) and ALDO1 (3.3V audio codec analog supply).
- **Floating Regulators**: DCDC2–5, ALDO2–4, BLDO1–2, CPUSLDO, and DLDO1–2 do not connect to external circuits.
- **Power Optimization**: Disable all unused rails at boot to reduce quiescent battery consumption.

### CO5300 Display Window Coordinate Alignment
- **Coordinate Alignment**: The CO5300 controller requires even-aligned update windows (`x1 &= ~1`, `y1 &= ~1`, `x2 |= 1`, `y2 |= 1`).
- **Failure Mode**: Setting odd bounding box coordinates causes horizontal display shift artifacts or controller lockups.

### CST9217 Touch Standby & Hardware Wake Timing
- **Deep Sleep**: CST9217 enters ultra-low power standby via I2C command `0xD105`.
- **Hardware Wakeup**: Pull touch reset (GPIO2) LOW for 10 ms, then drive HIGH for 50 ms before sending I2C transactions.

### Clocking & Modem Sleep Constraints
- **RTC Crystal**: The board lacks an external 32.768 kHz crystal.
- **BLE Light-Sleep Clock**: Retain the 40 MHz crystal during BLE light-sleep (`CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y`, `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y`).
- **GPIO Sleep Retention**: Leave `CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND` disabled so floating lines do not reset display or touch controllers.

### Audio Codec Gain Steps
- **Hardware Codecs**: Audio output uses Everest Semi ES8311. Microphone input uses Everest Semi ES7210 dual-channel ADC.
- **PGA Quantization**: The ES7210 Programmable Gain Amplifier operates in 3 dB steps. A configured gain of 33 dB snaps to 34.5 dB.

### Internal DMA RAM Pipelining Under Wireless Load
- **Buffer Fragmentation**: Wi-Fi and Bluetooth stacks fragment internal SRAM, leaving under 16 KB contiguous DMA memory.
- **Bounce Buffering**: Allocate two fixed internal DMA bounce buffers (such as 7 KB each) and stream display slices through FreeRTOS tasks to prevent flush allocation aborts.
