# Release Scripts

The scripts in this directory package CI build outputs, download workflow artifacts, validate
combined firmware, and stage GitHub Release assets.

Published firmware must come from the successful tag workflow. The commands below can be used for
script development, but local firmware builds are not release inputs.

## Package Contents

`package_firmware.py` reads the framework build output and creates
`<artifact-name>-combined.zip` with:

- Original offset-addressed binaries under `bin/`.
- `bin/<artifact-name>-combined.bin`, flashed at `0x0`.
- Split and combined flash scripts for shell and Windows.
- Split and combined command text files.
- `manifest.json` with source, version, offsets, sizes, and SHA256 checksums.
- A package README.

For ESP-IDF, offsets and esptool options come from `flasher_args.json`. For Arduino, the exported
`.merged.bin` is used when available; otherwise the standard ESP32-S3 binary layout is inferred.

## Package An ESP-IDF Build

```bash
python3 releases/package_firmware.py \
  --framework esp-idf \
  --project examples/esp-idf/02_lvgl_demo_v9 \
  --build-dir build/02_lvgl_demo_v9-v6.0.2 \
  --name ESP32-S3-Touch-AMOLED-1.75C-02_lvgl_demo_v9-v6.0.2 \
  --framework-version v6.0.2 \
  --target esp32s3 \
  --git-sha <git-sha> \
  --output-dir release-artifacts
```

## Package An Arduino Build

```bash
python3 releases/package_firmware.py \
  --framework arduino \
  --project examples/arduino/examples/01_HelloWorld \
  --build-dir build/01_HelloWorld-3.3.10 \
  --name ESP32-S3-Touch-AMOLED-1.75C-01_HelloWorld-arduino-3.3.10 \
  --framework-version 3.3.10 \
  --target esp32s3 \
  --git-sha <git-sha> \
  --output-dir release-artifacts
```

## Download CI Artifacts

Download all firmware from a completed workflow run and retain the inner combined ZIP files:

```bash
python3 releases/download_artifacts.py \
  --run-id <run-id> \
  --keep-archives \
  --clean
```

Without `--run-id`, the downloader selects the latest successful `examples.yml` run for the current
branch. Use `--artifact <name>` for one artifact or `--pattern "firmware-esp-idf-*"` for a subset.

Artifacts are extracted under `releases/downloads/run-<run-id>/`. Authentication uses
`GH_TOKEN`, `GITHUB_TOKEN`, or the active GitHub CLI login.

## Stage A Release

After the tag workflow succeeds, validate and stage all 17 firmware archives:

```bash
python3 releases/prepare_release_assets.py \
  --input-dir releases/downloads/run-<run-id> \
  --output-dir releases/dist/v1.0.1 \
  --version v1.0.1 \
  --git-sha <tag-commit-sha> \
  --clean
```

The script rejects missing combined images, incorrect offsets, checksum mismatches, duplicate asset
names, mixed Git SHAs, and an unexpected archive count. It copies validated ZIP files and writes
`manifest-combined-assets.json`.

Upload the 17 ZIP files, the combined-assets manifest, and the release notes from
`releases/v1.0.1.md` to the GitHub Release.
