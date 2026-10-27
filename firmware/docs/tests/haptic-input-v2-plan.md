# Pressure and motor feedback v2

Prepared 2026-10-23 with ChatGPT assistance.
Baseline: public main e18744b0f2ea663d94c11cb3e8a063f027666dbb.
Supersedes the earlier press-turn fix. Assembled hardware validation is pending.

## Changes

- Short-press action and click now occur together on confirmed release. Down
  alone never clicks. Long acts once at 900 ms. Waking consumes the first press.
- Positive signed pressure delta, threshold 15000 counts relative to startup
  tare. Negative bending cannot trigger a press. Release uses absolute delta
  below 7500. Four-sample average, 50 ms confirmation, 60 ms release debounce.
- New presses require 120 ms since rotation above 0.35 rad/s. Loads encountered
  while rotating must be released before rearming. Pending gestures tolerate
  small movement but cancel at six degrees net travel or a detent crossing.
- Navigation no longer freezes during pressure. Crossing a detent cancels the
  pending pressure gesture and still advances the selection.
- Haptic gain scales with spacing: coarse detents at least 3.8 V/rad, fine
  50-per-turn detents about 0.5 V at half-width instead of about 0.14 V.
  Hysteresis is 7% of width capped at two degrees; deadband 6% capped at one.
- Velocity damping opposes motion and is capped at 0.15 V. Total spring demand
  remains capped at 1 V; the existing 200 V/s slew limiter remains.
- Click remains +3 V for 5 ms then -3 V for 5 ms. An isolated TRACK LOST tick
  outputs zero; recovery uses only the remaining original pulse envelope.
  An already started pulse is never restarted or lengthened. Five-millisecond
  dropout protection and other fault stops remain active.
- Click blanking remains 150 ms; filter history clears after a pulse. ADC
  saturation and acquisition timeout are checked even during blanking.
- Stop output includes pressure range, gesture counters, turn cancellations,
  click gap ticks and speed. Serial printing only occurs with motor disabled.
  x stops and reports; r restarts while stopped.

Voltage commands are not measured torque/current. Motor calibration is unchanged
and specific to the original tested motor and mounting. Weak magnetic field,
mechanical preload/friction, wiring faults or a different motor remain hardware
issues. ADC, driver, encoder, control-loop timing, speed and end-stop protections
were not disabled. This patch does not guarantee operation without faults.

## Quadra review

https://github.com/katbinaris/Quadra
Reviewed commit df8720752e557eb30e123d53e6e54e2b6b018c79, especially
NanoDepsidf/src/control_task.c, haptic_params.h and motor_config.h.

Useful principles are spring/damper detents, hysteresis and profile-specific
parameters. Quadra uses separate keys, not this HX711 pressure mechanism.
Its voltage limits, motor and 10 kHz architecture do not transfer directly.
Quadra uses PolyForm Noncommercial 1.0.0. No Quadra implementation was copied;
haptics.h independently implements standard spring/damper relationships.
Existing Scott attribution in app_gui_main.cpp is retained.

## Tests

From firmware, with g++ and Python installed:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I docs/tests/haptic-stubs docs/tests/haptic-input-host.cpp -o haptic-test
./haptic-test
python docs/tests/haptic-integration-host.py
```

Host tests passed for pressure timing, negative loads, motion rejection,
angle wrap, saturation, timeout and bounded spring/damping output. Integration
checks compile the actual GUI gesture, detent and control functions against the
actual model with peripheral effects mocked: action/feedback order, wake,
light toggles, navigation during pressure, timed pulse recovery and the retained
5 ms dropout stop. The existing misleading-indentation warning in
light_protocol.h is not promoted to an error by this integration test.
These tests do not establish physical stability or tactile quality.
Full ESP32 build was attempted but not completed in this environment: dependency
downloads repeatedly failed checksum verification on one mirror; after switching
to another verified mirror, installation stalled at SimpleFOC. The build was
stopped during dependency installation, before compiling the application.
Compile locally with the existing project dependencies before flashing.
Existing timer/settings and IR protocol/model host regression tests also passed.

## Installation and hardware checks

Copy the ZIP's firmware folder into the existing repository root, merging
folders and replacing the included source files. Keep all other files, especially
private pairing headers. Build locally using your existing pairing:

```powershell
pio run -e app_gui
pio run -e app_gui -t upload
```

1. Use the original calibrated motor. Boot untouched and allow startup tare.
2. Try 20 short presses, returning Home between them. Record successful app
   openings, missed presses and any feedback without action.
3. Test five long presses and wake presses. Long must not also trigger Short.
4. Rotate at least 30 detents each direction without pressing. No app opening.
   Then stop briefly and press. Turning while pressing should cancel the gesture,
   but must not freeze the carousel or pull against an obsolete centre.
5. Test fine value detents, reversal near boundaries and steady holding.
6. Run normal operation for five minutes including Timer, IR and network apps.
   If stopped, retain the exact stop reason and counters. Do not raise protection
   thresholds to make a failed test look successful.

Record hardware results before claiming reliable operation or committing a
successful hardware-test result.
