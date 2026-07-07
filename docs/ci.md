# Continuous Integration

The repository uses `.github/workflows/examples.yml` to build first-party ESP-IDF and Arduino examples.

`scripts/discover_examples.py` creates the build matrix for both surfaces:

- ESP-IDF examples under `examples/ESP-IDF-v5.5/`
- Arduino sketches under `examples/Arduino-v3.3.5/examples/`

Bundled Arduino library examples under `examples/Arduino-v3.3.5/libraries/` are intentionally excluded from product CI.

## Versions

The current CI matrix uses:

- ESP-IDF `v5.5.4`
- ESP-IDF `v6.0.2`
- Arduino-ESP32 core `3.3.10`
- Arduino FQBN `esp32:esp32:esp32s3`

## Build Artifacts

Each successful source build is packaged by `releases/package_firmware.py` and uploaded as a GitHub Actions artifact. Firmware packages include a manifest, flash helper scripts, flash arguments, and binaries under `bin/`.

Checked-in factory binaries under `Firmware/` are recovery artifacts and are not rebuilt or re-uploaded by CI.

## Manual Dispatch

Use `workflow_dispatch` with `target=all`, an example directory name, or a repo-relative example path.

Examples:

```text
all
02_lvgl_demo_v9
examples/ESP-IDF-v5.5/02_lvgl_demo_v9
examples/Arduino-v3.3.5/examples/01_HelloWorld
```

Build validation should run through GitHub Actions so pull requests and branch updates use the same toolchains and matrix.