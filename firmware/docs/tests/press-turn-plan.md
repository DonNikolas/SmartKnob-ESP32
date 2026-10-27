# Press / turn separation — 2026-10-23

Experimental software fix, prepared with ChatGPT assistance after the assembled
SmartKnob video showed unintended app opens and ended with `Speed limit`.
Hardware validation and ESP32 compilation are pending.

## Findings and changes

Previously `labs(raw - offset)` treated either load direction as a press. While
`pressed` or `consumeGesture` was true, detentError returned before advancing the
selected detent, but the spring continued to pull towards the old centre.
This can explain resistance/frozen navigation; it does not prove the precise
cause of the recorded speed-limit stop.

- PRESS_DIRECTION = +1 selects rising raw ADC readings, based on the latest
  mounted-board press log. The absolute PRESS_THRESHOLD remains 15000.
- Gesture motion detection uses encoder speed >= 0.35 rad/s or accumulated
  displacement >= 1.5 degrees. It waits 180 ms after detected rotation and
  requires 150 ms near the real pressure zero before rearming.
- Rotation cancels pending/held pressure gestures. Keeping pressure applied
  while stopping rotation cannot generate a delayed short/long action.
- Negative load does not initiate a press. Rearming still requires an absolute
  deviation below half the threshold, not merely a negative reading.
- An active pressure gesture no longer freezes the detent centre. Crossing a
  detent cancels the gesture before changing the selection. Existing bounded
  value end stops remain in place.
- Self-generated 10 ms haptic pulses and the existing 150 ms settling interval
  do not trigger the normal motion rejection. Detent crossings still cancel.
- Stop output now includes signed pressure delta, filtered delta, angular speed
  and gesture counters. Print occurs only after disabling the motor. `x` can
  be used to stop and obtain a snapshot; `r` restarts while stopped.

Motor voltage, calibration, 15 rad/s speed limit, sensor-dropout handling, ADC
saturation detection and pressure data timeout are unchanged.

## Limitations

This is a heuristic. A pressure spike occurring before detectable rotation may
still produce initial haptic feedback. Very slow/small rotation and real axial
pressure while gripping cannot be perfectly separated by software. Pressing
immediately after turning may require a brief release/pause. Mechanical
cross-coupling, pressure drift and weak-field warnings remain separate issues.
No automatic zero drift correction or runtime calibration was added.

## Host checks performed

Run from the repository root with a C++ compiler and Python:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -I firmware/docs/tests/press-turn-stubs firmware/docs/tests/press-turn-host.cpp -o /tmp/press-turn-host
/tmp/press-turn-host
python firmware/docs/tests/press-turn-detents.py
```

Passed: short press, long press without duplicate short, negative-load rejection,
rotation before/during a press, no delayed action after stopping, release/rearm,
slow accumulated rotation, angular wrap, millis wrap, haptic settling, real
pressure-class tare/filter path, saturation during blanking and data timeout.
The detent harness extracts and compiles the actual detentError function with
host adapters; it checks selection while pressed, gesture cancellation, reverse
and multiple detents, value bounds and relative light requests.
These checks do not exercise ESP32 peripherals, timing, torque or real ADC data.

## On-device acceptance

1. Build/upload app_gui; leave the knob unloaded during startup zeroing.
2. Turn slowly/quickly both directions in Home without pressing: no app opens,
   no haptic clicks caused by pressure and no fixed-centre resistance.
3. Stop, release, then short press: one feedback pulse and one app open.
4. Long press inside an app: one return Home, no subsequent short action.
5. Start pressing then turn: selection may move, but releasing must not open the
   new selection. Verify a subsequent normal press after release works.
6. Check Timer start/pause, Standby wake, value limits and Light commands.
7. If stopped, save the entire STOP reason and input snapshot. Do not increase
   limits to suppress the report. Record any missed presses and circumstances.
