# Repository Structure

This repository uses the canonical Waveshare ESP32 product layout for maintained examples:

```text
examples/esp-idf/              first-party ESP-IDF examples
examples/arduino/examples/     first-party Arduino sketches
examples/arduino/libraries/    bundled Arduino libraries used by sketches
config/                        shared configuration notes and overlays
docs/                          maintainer and repository notes
.github/                       CI and collaboration templates
Firmware/                      factory flashing and recovery binaries
Schematic/                     hardware schematic resources
releases/                      CI firmware packaging and artifact helpers
```

The historical versioned example roots have been replaced by the canonical roots above. CI discovery scans only the canonical source roots, while manual dispatch still accepts older path strings as selector aliases during the transition.

## Source And Binary Boundaries

- ESP-IDF source examples are under `examples/esp-idf/`.
- First-party Arduino sketches are under `examples/arduino/examples/`.
- Bundled Arduino libraries are under `examples/arduino/libraries/`.
- Factory firmware binaries are under `Firmware/` and are documented as flash/recovery artifacts, not CI build outputs.
- CI-generated firmware packages are workflow artifacts produced by `releases/package_firmware.py`.