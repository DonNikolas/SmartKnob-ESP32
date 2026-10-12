# Combined controls test

Status: ESP32 build and initial combined hardware test passed.
Longer testing is still pending.

## Goal

Combine the previously tested motor, pressure input, display and LED ring.
Twelve detents select positions 01 to 12. Position 01 starts at 6 o'clock
when viewed from the front. Increasing positions run counterclockwise.
A press confirms the selected position and requests the existing motor click.
The display records the confirmed position and the LED briefly turns green.
This test does not control an external light.

## Settings

- Spring limit: 1.00 V command; stiffness: 3.00 V/rad.
- Click: +3.00 V for 5 ms, then -3.00 V for 5 ms.
- Pressure threshold: 15000 ADC counts; release below 7500.
- Twelve detents, smooth spring and bounded dropout hold enabled on boot.
- Baseline commutation on boot; the optional correction remains an A/B test.
- Manual start, 10-second sessions, and `x` to stop.
- Display: GC9A01, 240 x 240, tested 1 MHz SPI configuration.
- LEDs: 36 GRB pixels on GPIO 11, brightness 20/255, one selection marker.

Voltage commands are not measured winding voltages or current limits.
The earlier weak-field warning and intermittent tracking loss remain unresolved.
Existing CRC, stale-data, driver, speed and loop-gap stops remain active.
A pressure confirmation and a completed motor pulse are counted separately:
a dropout can cancel the pulse after the selection was already confirmed.

## Integration

The Arduino loop on core 1 owns the motor, encoder and HX711. A separate
core-0 task owns LVGL, the display, backlight and NeoPixel output. A queue of
length one copies the latest view state without waiting for rendering.
No motor object or pressure-input global is accessed from the UI task.
The display uses Arduino `HSPI` (physical SPI3 on ESP32-S3 / Arduino 2.0.17).
The encoder retains ESP-IDF `SPI2_HOST`; the GFX default would conflict here.
Backlight uses LEDC and motor output uses SimpleFOC's ESP32 MCPWM backend.

The UI follows current state, rather than replaying every intermediate detent.
Display updates at 1 MHz can visibly lag during fast rotation. Motor timing
under simultaneous display/LED activity still needs hardware verification.

## Build and first orientation check

From the repository's `firmware` directory:

```powershell
pio run -e combined_controls
pio run -e combined_controls -t upload
pio device monitor -e combined_controls -b 115200
```

The motor starts disabled. First check orientation with no motor force:

1. A white LED marks position 01. Use `[` or `]` to move it to 6 o'clock.
2. Press `p` once. Position 02 must be towards 5 o'clock, counterclockwise
   when viewed from the front. If it goes the other way, press `l` and then `p`.
3. Press `c` to leave preview and set the current shaft position as position 01.
4. Turn the knob slowly counterclockwise. The display must advance 01, 02, 03.
   If it decreases, press `d`, then `c`, and repeat. This only reverses the
   user-interface mapping; motor commutation is unchanged.
5. Copy the serial alignment values into `combined_ui.h` to retain them after
   reset: `LED_SIX_OCLOCK_INDEX`, `LED_CCW_INDEX_STEP`, and `RAW_TO_CCW`.
   The supplied LED index 0 is a starting value, not a verified physical location.

## Combined hardware test

1. Release the knob, send `t`, and wait for HX711 zero and a full filter.
2. Send `c` without pressing or rotating the knob.
3. Send `s` for one 10-second session. Twelve detents and smooth/hold modes
   are already selected. `x` stops immediately when processed by the loop.
4. Rotate slowly in both directions. Compare the detent, number, display dot
   and LED. Check the 12-to-01 wrap in both directions.
5. Press once, then hold. Expect one confirmation and one requested click;
   holding must not repeat it. Release, then press at another position.
6. Check whether display updates introduce noise or a loop-gap stop. Record
   the exact stop message rather than increasing timing limits.
7. Save a short video and the serial summary. Record cancelled clicks and
   remaining weak-field warnings. Do not label the integration verified until
   this test has been completed on the PCB.

Alignment commands are allowed only while OFF. Sending one while active
stops the motor; repeat it while OFF to apply it. `+` / `-` adjust pressure
threshold by 1000 counts while OFF. The existing diagnostic commands remain
available via `i`; `2` selects a single centre and `4` restores twelve detents.

## Software checks before delivery

- Motor code syntax checked with real SimpleFOC 2.3.3 / HX711 0.7.5 headers
  and host Arduino/SPI stubs.
- UI syntax checked with real LVGL 8.3.11 / NeoPixel 1.12.3 headers and
  Arduino/GFX/FreeRTOS stubs; GFX 1.4.7 constructor and SPI routing reviewed.
- Mocked motor/ADC tests covered all LED offsets in both directions,
  selection wrap, pressure confirmation, pulse timing, release rearming,
  dropout cancellation, explicit stop, ADC saturation and ADC timeout.
- These checks are not an ESP32 PlatformIO build, timing measurement or
  electrical validation. Those remain pending.

## Sources and AI assistance

ChatGPT combined the project's existing standalone test files and added the
UI task, state queue, selection mapping and orientation controls.
The user performs the PlatformIO build and PCB tests and records the results.

Architecture references reviewed:

- [Scott Bezek motor task](https://github.com/scottbez1/smartknob/blob/master/firmware/src/motor_task.cpp)
- [SeedLabs motor task](https://github.com/SeedLabs-it/smartknob-firmware/blob/main/firmware/src/motor_foc/motor_task.cpp)
- [GFX 1.4.7 ESP32 SPI implementation](https://github.com/moononournation/Arduino_GFX/blob/v1.4.7/src/databus/Arduino_ESP32SPI.cpp)
- [Arduino-ESP32 2.0.17 SPI bus definitions](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/esp32-hal-spi.h)

Scott's applicable Apache-2.0 notice remains in the source and in
`docs/licenses/Scott-SmartKnob-Apache-2.0.txt` in this repository.
