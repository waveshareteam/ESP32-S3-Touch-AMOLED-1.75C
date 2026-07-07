# Continuous Integration

This repository validates first-party examples with GitHub Actions.

## ESP-IDF

The ESP-IDF workflow discovers projects under the current ESP-IDF example root and any source firmware roots that contain a real ESP-IDF project entry point. It builds each selected project for `esp32s3` with the pinned CI matrix:

- ESP-IDF `v5.5.4`
- ESP-IDF `v6.0.2`

Factory binary folders are not source projects and are not built by CI.

## Arduino

The Arduino workflow discovers first-party sketches under `examples/Arduino-v3.3.5/examples/` and compiles them with:

- Arduino-ESP32 core `3.3.10`
- FQBN `esp32:esp32:esp32s3`
- Bundled libraries from `examples/Arduino-v3.3.5/libraries/`

Examples inside bundled libraries are intentionally excluded from product CI. They belong to the bundled libraries, not to this product example set.

## Dispatch Inputs

Both workflows accept `all`, a directory name, or a repo-relative path through `workflow_dispatch`.

Use `all` before release checks or after changing shared CI scripts, bundled libraries, or shared configuration files.

## Validation Policy

Local builds are intentionally not required for repository maintenance changes. Build validation should run through GitHub Actions so the same toolchains, versions, and matrices are used for pull requests and branch updates.