# Pressure and motor feedback v3

Download rebuilt 2026-10-25 from the previously delivered v3 changes.
Generated with ChatGPT assistance. This is a SOURCE PATCH, not a compiled binary.
The four production files retain the v3 behaviour. Tests and this note were
recreated after the temporary download expired. No new feature is introduced.

## Installation

Merge the included firmware folder into the repository root. Replace included
files only; do not delete existing folders or private pairing headers.
From the firmware directory:

```powershell
pio run -e app_gui
pio run -e app_gui -t upload
```

## Behaviour

- Click on confirmed Down, once. It acknowledges pressure detection.
- Short action remains on release, without a second click. Long acts once after
  900 ms and does not first execute Short (e.g. no light toggle when going back).
- Threshold 12000 positive counts relative to startup tare; release below 6000
  absolute counts. Four-sample average, 50 ms Down, 60 ms release debounce.
- New presses require 120 ms since rotation above 0.35 rad/s. Load first seen
  while rotating must be released before rearming. Cancel pending gestures on
  six degrees net motion or a detent crossing; navigation remains free to move.
  An intentionally cancelled gesture can have a press click but no app action.
- Own click motion is ignored by the motion gate during its blanking interval.
  ADC saturation and timeout checks remain active during blanking.
- Coarse spring gain at least 3.8 V/rad; fine detents target 0.5 V at half-width.
  Hysteresis 7% of spacing capped at 2 degrees, deadband 6% capped at 1 degree.
  Damping 0.025 V/(rad/s), capped at 0.15 V. Total spring capped at 1 V, existing
  slew limit 200 V/s. Voltage commands are not measured force/current.
- Click +3 V for 5 ms, then -3 V for 5 ms. Short TRACK LOST gaps output zero;
  recovery uses only the remaining original envelope, never extending it.
- Existing ADC, driver, encoder, speed, timing and end-stop protections remain.
  x stops and reports diagnostics; r restarts while stopped.

Calibration remains specific to the original tested motor and mounting.
Weak magnetic field and mechanical/preload faults are not resolved by software.

## References

Quadra (https://github.com/katbinaris/Quadra), reviewed previously at commit
 df8720752e557eb30e123d53e6e54e2b6b018c79, informed the general spring/damper
and hysteresis approach. Quadra uses separate keys rather than this HX711
mechanism. Its implementation was not copied; it is PolyForm Noncommercial.
Existing Scott attribution in app_gui_main.cpp remains unchanged.

## Validation

Recreated host tests passed on 2026-10-25: exact threshold, short/long gestures,
negative load and rotation rejection, angle wrap, ADC faults, bounded damping,
click on Down only, no Short on long-press navigation, detent movement during
pressure, bounded pulse recovery and retained 5 ms dropout stop.

After merging into the full project, run from firmware:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I docs/tests/haptic-stubs docs/tests/haptic-input-host.cpp -o haptic-test
./haptic-test
python docs/tests/haptic-integration-host.py
```

The integration test compiles actual control/gesture functions with peripheral
mocks. It does not verify physical timing, torque or electrical safety.
No completed ESP32 build or physical validation is claimed for v3 here.
The previous full build attempt was blocked during dependency downloads.

Use the calibrated motor. Test turning without pressing, 20 short presses,
five long/wake presses, fine detents, reversals and steady holding. Record missed
presses, unintended actions and exact STOP diagnostics. Do not increase fault
limits to hide hardware failures. Test normal Timer/IR/network use afterwards.
