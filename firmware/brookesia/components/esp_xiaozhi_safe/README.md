# ESP Xiaozhi safety override

[简体中文](README_ZH.md)

This directory vendors Espressif `esp_xiaozhi` 0.1.1 and is selected from
`components/XiaozhiApp/idf_component.yml` with `override_path`.

The local changes replace the unbounded WebSocket text and binary send waits
with a two-second timeout, reject timeout/short-write results, and bound chat
event posting to 100 ms. This prevents a stalled network or full event queue
from permanently holding the Xiaozhi audio transmit mutex and the shared
ES8311/ES7210 audio session.

The general component API is documented in the bundled
[English](esp_xiaozhi/README.md) and
[Chinese](esp_xiaozhi/README_CN.md) upstream READMEs. Keep those vendored
documents separate from this product-specific safety note.
