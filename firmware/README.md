# Firmware

[English](README.md) | [简体中文](README_ZH.md)

Ready-to-flash firmware tested on the ESP32-S3-Touch-AMOLED-1.75C board. Each file is a merged image flashed at `0x0`; the date in the file name is its version.

| File | Purpose |
| --- | --- |
| `ESP32-S3-Touch-AMOLED-1.75C_FactoryOnly_20260114.bin` | Factory firmware, restores the out-of-box demo |

Flash with esptool:

```bash
python -m pip install esptool
esptool.py --chip esp32s3 --port <PORT> erase_flash
esptool.py --chip esp32s3 --port <PORT> write_flash 0x0 <FILE>.bin
```
