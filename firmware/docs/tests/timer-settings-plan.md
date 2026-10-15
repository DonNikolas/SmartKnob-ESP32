# Timer and persistent app settings — v2

Generated with ChatGPT assistance, 2026-10-15.
Based on the integrated app GUI including the tested Plane Radar app.

## Timer controls

- Open Timer, turn to select 1–60 minutes, short press to start.
- While counting: short press pauses. Another short press resumes.
- While paused: turn to discard the paused countdown and return to duration editing.
  The first turn opens the editor; subsequent turns change the duration.
- Long press returns Home without cancelling a running/paused countdown.
- Reopening Timer shows that existing countdown. Other apps remain usable.
- At zero, the LED ring flashes red for ten seconds; the Timer screen shows Finished.
  Short press on Finished acknowledges and returns to the duration editor.
- No buzzer/audio output or automatic motor pulse is implemented for completion.
- Countdown runs in standby and during a latched motor stop; fault/stop LED
  indications take precedence over the completion flash. A stopped motor stays stopped.
- No forced screen switch on completion. Open Timer to see/acknowledge Finished.
- After a power interruption the preset duration is restored, but a countdown is
  not resumed or reconstructed. Starting a timer is always an explicit user action.

The timer uses unsigned elapsed milliseconds (wrap-safe for the supported duration),
not a loop counter or network time. It does not write elapsed seconds to flash.

## Persistent settings

Saved in NVS namespace `knob-app`, key `settings`, as one versioned/checksummed record:

- Timer duration, Music volume, Light brightness, Temperature setpoint,
  and System/display brightness.
- Radar range and the last selected carousel app.

The already existing WLAN/location record in `knob-network` is unchanged.
Saving placeholder Music/Light/Temperature values does not add real device control.
Values are adopted while turning, without a confirming press. Long press returns
to Home and keeps the changed value. Editing the timer preset does not start a countdown. Pressure zero, motor
calibration, active screen, live sensor values, countdown progress and alarm state
are intentionally not persisted. Pressure is zeroed normally at each boot.

Writes are coalesced until at least two seconds after the last stored-value change
and user activity. The knob must be released and nearly stationary for at least
500 ms. Writes are allowed while the editor remains open, with no requirement to
land within two degrees of its original detent centre. No writes occur during a
press, feedback pulse, rotation or startup. The pending-save dot has been removed.
After changing a value, release the knob and wait at least three seconds before
removing power. The error message remains visible if saving fails.

NVS can pause both CPU cores. The motor-owning task disables the driver BEFORE
Preferences writes. It then returns to the ordinary loop, gets a fresh encoder
sample and uses the existing guarded start and voltage ramp. This can briefly
soften the detent during an idle save; it must not be described as zero-impact.
No motor voltage/speed/dropout thresholds have been increased or disabled.
Reference: https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/storage/spi_flash.html

A failed write displays Settings not saved. It retries
at most every thirty seconds when the same idle conditions are satisfied. A
successful write is read back and checked. Invalid/missing records use defaults.
No automatic full-flash erase or reset of other NVS namespaces is performed.

## Verification performed

Host tests passed:
- Start, pause, resume, background completion and acknowledgement.
- Discarding a paused timer and milliseconds counter rollover.
- Record round-trip, corrupt checksums, unsupported versions, range/step validation.
- Real LVGL 8.3.11 rendering of running, paused and finished states; existing
  Radar/other screens and LED precedence checks. About 26 KiB LVGL heap remained.
- Mocked controller test with 120 ms flash delays: driver off before every write,
  normal guarded restart without timing fault, coalescing, no repeated unchanged
  writes, failed-write retry and readback. Countdown continued during motor stop.

These are host checks, not ESP32 compilation or physical hardware validation.

## Hardware acceptance test (pending)

1. Build/upload `app_gui`, release the knob during startup zeroing.
2. Set a one-minute timer. Test pause for ten seconds, resume, Home and another app.
3. Confirm completion/ten-second red flash, reopen Timer, acknowledge.
4. Start another timer; pause and turn to return to duration editing.
5. Set System brightness without confirming; release the knob and wait three seconds.
6. Set Music/Light/Temperature values and Radar range. No confirmation is needed.
   Release the knob and wait three seconds after the last change.
7. Disconnect/reconnect USB. Verify stored values, brightness, range and selection.
   The timer must NOT restart automatically; WLAN must still reconnect normally.
8. Test ordinary haptics, pressure, Sensors, clap flash and Radar after idle saves.
   Record any reboot, unexpected movement or stop message without relaxing guards.
9. Power loss during the autosave waiting interval may retain the older settings;
   this is expected, not a confirmed-save failure.

## Result — 2026-10-15

- Firmware built and uploaded successfully.
- Timer operation and automatic settings persistence were tested on the device.
- Settings remained available after restarting.
- The timer LED ring now matches the display: starting at 12 o'clock
  and shrinking counterclockwise.
- The pending-save indicator was removed.
- Extended stability testing remains pending.

[Demo video](../videos/timer-settings-demo.mp4)
