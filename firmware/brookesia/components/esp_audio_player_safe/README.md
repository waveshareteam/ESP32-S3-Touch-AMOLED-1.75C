# ESP audio-player safety override

[简体中文](README_ZH.md)

This directory vendors `chmorgan/esp-audio-player` 1.1.0 and is selected by
[`../bsp_extra/idf_component.yml`](../bsp_extra/idf_component.yml) through
`override_path`. The public component version and legacy playback API remain
1.1.0-compatible. See the bundled
[upstream README](esp-audio-player/README.md) for the general API and supported
formats.

## Local lifecycle fixes

The override makes player destruction deterministic before an application
releases its shared audio owner or SD mount lease:

- A completion semaphore lets the deleting task wait until the audio worker has
  committed to shutdown and will no longer access instance storage.
- A delete gate rejects concurrent deletion of the same instance.
- Shutdown is queued at the front with a bounded send and a five-second overall
  wait. If the worker cannot stop in time, deletion returns
  `ESP_ERR_TIMEOUT` and keeps the complete instance alive for a safe retry.
- Once shutdown succeeds, cleanup drains queued PLAY requests and closes every
  transferred `FILE` that the worker did not consume.
- The legacy singleton is cleared only after instance deletion succeeds, so a
  failed attempt cannot expose a freed or half-deleted player.
- A worker callback cannot delete its own task, and new control events are
  rejected after shutdown begins.

These changes preserve the ownership rule documented by `audio_player_play()`:
after a successful handoff, the player owns and closes the `FILE`; after a
failed handoff, the caller still owns it.

## Caller contract

Call `audio_player_delete()` and require `ESP_OK` before closing the codec,
releasing the `bsp_extra` audio owner, or unmounting storage that contains an
active or queued file. If deletion times out, do not free callbacks or dependent
state; resolve the stalled worker and retry deletion.

The component remains under the bundled Apache License 2.0. Keep the nested
upstream README and license intact when updating the vendored source, and
reapply or retire the local changes deliberately after comparing with the new
upstream lifecycle behavior.

## Validation

Build the complete Brookesia project and repeat the MusicPlayer rapid
enter/exit and cross-application audio-ownership checks in
[`../../HARDWARE_VALIDATION.md`](../../HARDWARE_VALIDATION.md). A clean UI exit
is not sufficient: serial output must show no timeout, stale callback,
cross-owner release, or SD lease left busy.
