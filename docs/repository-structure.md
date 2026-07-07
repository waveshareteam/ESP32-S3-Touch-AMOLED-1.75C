# Repository Structure

This repository currently keeps the original public example roots:

```text
examples/ESP-IDF-v5.5/
examples/Arduino-v3.3.5/
Firmware/
Schematic/
```

The CI workflow understands these existing roots to avoid path churn while the repository is being modernized.

The preferred long-term layout for new Waveshare ESP32 product repositories is:

```text
examples/esp-idf/
examples/arduino/
config/
docs/
.github/
firmware/
hardware/
releases/
```

Future structure work should move first-party examples into the canonical roots, keep bundled Arduino libraries with the Arduino examples, and leave compatibility notes for old public paths.

## Source And Binary Boundaries

- ESP-IDF source examples are currently under `examples/ESP-IDF-v5.5/`.
- First-party Arduino sketches are currently under `examples/Arduino-v3.3.5/examples/`.
- Bundled Arduino libraries are currently under `examples/Arduino-v3.3.5/libraries/`.
- Factory firmware binaries are currently under `Firmware/` and are documented as flash/recovery artifacts, not CI build outputs.
- CI-generated firmware packages are workflow artifacts produced by `releases/package_firmware.py`.