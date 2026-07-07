# Firmware Artifacts

`Firmware/` contains factory binary artifacts for user flashing and recovery flows. These binaries are not source projects and are not built by CI.

Source-maintained firmware should live under `examples/esp-idf/`, `examples/arduino/examples/`, or another documented source directory with its own validation path.

CI build outputs are packaged by `releases/package_firmware.py` and uploaded as workflow artifacts. The generated zip contains `manifest.json`, flash helper scripts, `flash_args.txt`, and the binaries needed by esptool.

Use `releases/download_artifacts.py` to download firmware artifacts from a completed Actions run into `releases/downloads/`.

Generated archives, downloaded artifacts, and build outputs stay out of source control.

For runtime validation, flash the complete CI-generated package from one artifact directory. Mixing binaries from different runs or monitoring with symbols from another build can produce checksum mismatch warnings and misleading panic backtraces.
