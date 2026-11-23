# ESP32-S3-Touch-AMOLED-1.75C hardware validation

[简体中文](HARDWARE_VALIDATION_ZH.md)

Run this checklist on a 1.75C board after every source change. A check fails if
it causes a reboot, a watchdog reset, a stack overflow, a white screen, a
partly drawn screen, or an audio owner conflict.

Each check names the serial log line to look for. The log comes from
`idf.py -C firmware/brookesia -p <PORT> monitor`. Opening the serial port
resets the board.

## 1. Flash and boot

1. Flash all partitions:

   ```bash
   idf.py -C firmware/brookesia -p <PORT> flash monitor
   ```

2. Confirm that the loading screen moves through its stages, reaches **Ready**,
   and fades to the full 466 x 466 launcher.
3. Confirm that the log has one `Installed <name>` line for each of the 10
   applications.
4. Confirm that the log has these lines and no `E (` lines:

   ```text
   Brookesia firmware is ready
   display_power: Loaded brightness 100%, timeout 60 s
   display_power: PWR key GPIO3 level at start: 0
   ```

   The brightness and timeout values are the defaults on a board with no saved
   settings.

## 2. Screen timeout

1. Open **Settings > Display**. Select **15 seconds** under **Screen timeout**.
   Confirm that the check mark moves to that row.
2. Leave the board alone. Confirm that the screen dims at 10 s and turns off at
   15 s. The log shows:

   ```text
   display_power: Screen Awake -> Dimmed (idle 10xxx ms, timeout 15 s)
   display_power: Screen Dimmed -> Off (idle 15xxx ms, timeout 15 s)
   ```

3. Tap a launcher icon on the dark screen. Confirm that the screen wakes and
   that the application does not open.
4. Let the screen turn off again. Swipe in from the screen edge. Confirm that
   the screen wakes and that no back or home gesture runs.
5. Confirm that the screen shows current content after wake. For example, the
   status bar clock must show the current time.
6. Hold one finger on the screen for 20 s. Confirm that the screen stays at
   full brightness.
7. Press PWR briefly. Confirm that the screen turns off. Press PWR briefly again.
   Confirm that the screen wakes.
8. Move the brightness slider. Confirm that the brightness changes while you
   move it.
9. Reset the board. Confirm that the log shows the brightness you set and
   `timeout 15 s`.
10. Select **Never**. Leave the board alone for 2 minutes. Confirm that the
    screen stays at full brightness.

## 3. Button Test

1. Open **Button Test** with both keys released. Confirm that PWR shows
   `RAW LOW` and BOOT shows `RAW HIGH`. Both cards must show `RELEASED` and
   `WAITING`.
2. Tap PWR briefly. Confirm that the PWR card shows `RAW HIGH` and `PRESSED`
   while you hold the key, then `PASS LATCHED`. The tap also turns the screen
   off. Tap PWR again to wake the screen.
3. Press BOOT. Confirm that the BOOT card shows `RAW LOW` and `PRESSED`, then
   `PASS LATCHED`.
4. Confirm that the footer shows `ALL BUTTONS PASSED`.
5. Leave and reopen Button Test five times. Confirm that the launcher stays
   responsive and that each new session clears both results.

## 4. Audio

1. Open **SpecAnalyzer**. Speak near the microphones. Confirm that the spectrum
   moves.
2. Open **MusicPlayer**. If `spiffs/music/` was empty at build time, confirm
   that the application opens and reports no tracks. If it had tracks, play one
   and confirm the sound.
3. Switch between MusicPlayer, SpecAnalyzer, and AIChats. Each application must
   get the audio session or report it busy. None may reboot the board.

## 5. Soak

Open and close every launcher application at least 20 times while you watch
the serial log. The firmware passes with no panic, watchdog reset, stack
overflow, or heap assertion. Record the firmware SHA256, the battery reading,
the Wi-Fi RSSI, and the serial log with the result.
