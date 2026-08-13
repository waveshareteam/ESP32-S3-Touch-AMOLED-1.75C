# Continuous Integration

[English](ci.md) | [简体中文](ci_ZH.md)

The `Build Examples` workflow discovers, builds, and packages every first-party example. Firmware
published in GitHub Releases comes from this workflow; release firmware is not compiled manually.

## Discovery Boundary

- ESP-IDF projects are direct children of `examples/esp-idf/` containing `CMakeLists.txt` and a
  `main/` directory.
- Arduino sketches are direct children of `examples/arduino/examples/` containing a top-level `.ino` file.
- `examples/arduino/libraries/**`, local component samples, and `Firmware/**` are excluded.

The `workflow_dispatch` selector accepts `all`, an example directory name, or a repository-relative
path.

## Validated Matrix

Versions were revalidated against upstream releases on 2026-08-13:

| Framework | Version | Examples | Firmware artifacts |
| --- | --- | ---: | ---: |
| ESP-IDF | `v5.5.5` | 5 | 5 |
| ESP-IDF | `v6.0.2` | 5 | 5 |
| Arduino-ESP32 | `3.3.11` | 7 | 7 |

ESP-IDF targets `esp32s3`. Arduino uses
`esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,FlashMode=qio,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc,CDCOnBoot=cdc`
with the bundled libraries: 16 MB flash, 8 MB OPI PSRAM, the 16 MB partition
scheme, and hardware USB CDC enabled at boot for the board's native USB port.

The full workflow consists of a lightweight policy job, two discovery jobs, and up to 17
build/package jobs. Matrix jobs do not fail fast, so one failure does not hide results from the
other examples.

Every pull request and branch push first runs a lightweight policy job. Its rename-aware classifier
uses the complete base/head diff and fails closed when that diff is unavailable or empty. Markdown
changes do not start example builds; direct example source selects only that example; shared or
workflow inputs select the applicable full matrix. `Firmware/**` changes are reported separately and
never enter the normal example matrix.

## Artifact Contract

Each successful build uploads one `*-combined.zip` archive containing:

- The original offset-addressed binaries under `bin/`.
- A single `bin/<artifact-name>-combined.bin` image for offset `0x0`.
- `manifest.json` with framework, version, project, target, Git SHA, file sizes, and SHA256 checksums.
- `flash_combined.sh`, `flash_combined.bat`, and `flash_combined_args.txt`.
- `flash.sh`, `flash.bat`, and `flash_args.txt` for split-image flashing.
- A package README.

The packager rejects overlapping binary regions. The combined image fills unused address gaps with
`0xFF` and preserves each binary at the offset supplied by ESP-IDF or Arduino.

## Version Policy

CI tracks the latest stable patch in the ESP-IDF v5.5 line, the latest stable ESP-IDF v6 release, and
the latest stable Arduino-ESP32 release supported by the repository. Version updates should include:

1. Upstream release and migration-guide review.
2. Full matrix CI.
3. Hardware validation of affected demos.
4. Documentation and release-note updates.

## Release Gate

A release is ready only when:

1. The pull request policy job and every build/package job selected by its change scope succeed.
2. Required hardware validation or maintainer approval is complete.
3. The pull request is merged and the release tag points to the merged commit.
4. Tag-triggered CI succeeds for the full 17-entry matrix.
5. All tag-run archives pass `prepare_release_assets.py` validation.
6. The GitHub Release contains 17 combined ZIP files and `manifest-combined-assets.json`.

See [Release Scripts](../releases/README.md) for the maintainer commands.
