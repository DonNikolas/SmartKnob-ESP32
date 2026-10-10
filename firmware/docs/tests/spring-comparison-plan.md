# Experimental spring comparison

Date: 2026-10-09
Status: software checks completed; ESP32 build and hardware test pending.

## Purpose

Compare a constant electrical zero with a smooth position-dependent electrical
zero on this particular SmartKnob-NB assembly. This test does not establish
absolute mechanical angle accuracy and does not resolve the weak-field warning.
The v6 diagnostic in `src/virtual_spring.cpp` remains a separate environment.

## Sources and AI assistance

The BLDCMotor + BLDCDriver6PWM arrangement and circular sensor filtering follow
Scott Bezek's SmartKnob approach. The Seedlabs implementation was also reviewed.
ChatGPT generated this experimental adaptation and analyzed the two user logs.
The owner must build, upload and test the result on the PCB.

- https://github.com/scottbez1/smartknob/blob/master/firmware/src/motor_task.cpp
- https://github.com/scottbez1/smartknob/blob/master/firmware/src/mt6701_sensor.cpp
- https://github.com/SeedLabs-it/smartknob-firmware/tree/main/firmware/src/motor_foc
- Retained upstream license: ../licenses/Scott-SmartKnob-Apache-2.0.txt

## Model and limits

Seven pole pairs and CCW sensor direction are provisional parameters supported
by the v6 measurements. The direction-independent offset model is:

`offset_deg = 22.04286664 - 18.18597597*sin(raw_angle) - 24.82943521*cos(raw_angle)`

Here raw_angle is the circularly filtered, otherwise uncorrected sensor angle
in radians. The constant SimpleFOC electrical zero is `(22.04286664 + 270)`
degrees, approximately 5.097 rad. Correction changes commutation only. Both
spring modes calculate restoring effort from the same raw-angle displacement.

The fit used interior rows 1..31 in both directions from profile 01, excluding
startup and turnaround points. A separate direction term was included during
the fit but is deliberately not applied to the controller. Its cause is not
established. Switching that term with velocity could introduce a discontinuity.

The first harmonic was chosen for simplicity. When fitted on profile 01 and
checked against profile 02 (including the diagnostic direction term), its RMS
residual is about 8.51 electrical degrees, with a maximum absolute residual of
20.35 electrical degrees. This is held-out profile agreement, NOT a measured
motor-control accuracy or an independent encoder calibration. The raw profile
interpolation comparison (about 5.70 degrees RMS) is a different metric and must
not be reported as this model's residual. `analyze_motor_profiles.py` reproduces
these calculations using Python and NumPy; its arrays are transcribed subsets
of the raw logs, which remain the source of record.

No change to the failed v6 consistency criteria is made. The prototype uses an
explicit experimental model rather than claiming the original calibration passed.

- Motor remains OFF until `s`.
- `c` is required to select a centre; mode switching preserves that centre.
- A session lasts at most 10 seconds, with a 500 ms startup ramp.
- The voltage command is limited to 0.30 V; there is no current measurement.
- Displacement is limited to +/-45 degrees around the centre.
- `x`, CRC failure, driver fault, large sample jump, excessive speed, or a
  delayed control loop stops the motor.
- TRACK LOST immediately disables phases. Recovery before 5 ms is allowed
  with a 2 ms voltage ramp; otherwise the session stops. This is unchanged
  between comparison modes and can still affect the feel with frequent losses.
- Weak-field-only samples are accepted experimentally and the warning remains
  visible. This is not production-ready behaviour.
- A mode change during a running session stops it; repeat the selection while OFF.
- Values are not persisted. Changes to magnet/motor/sensor mounting or phase
  wiring invalidate the fitted parameters.

## Build and upload

Run from the `firmware` folder:

```powershell
pio run -e spring_comparison
pio run -e spring_comparison -t upload
pio device monitor -e spring_comparison -b 115200
```

## Initial hardware procedure

1. Keep the motor free and wait for OFF status.
2. Send `0` to select BASELINE, then `c` to record the current centre.
3. Send `s`. First check small deflections of approximately 5-10 degrees in
   both directions. The force should act towards the centre. Press `x` if it
   pushes away, behaves unexpectedly, or the motor heats noticeably.
4. The session stops after 10 seconds. Record the stop report and observations.
5. Return to the same centre while OFF. Send `1`, then `s`, and repeat the same
   small movements. Do not send `c` again for this comparison.
6. Compare restoring direction, smoothness, return position and dropout counts.
   Do not increase voltage or bypass a stop just to obtain a visible effect.

A build success alone does not establish functioning or stable motor control.
Do not merge this experimental feature as a verified spring until results are
recorded. Long-term reliability and the original +/-180-degree behaviour remain
outside this first comparison test.

## Software validation performed

- Syntax/API check against the actual SimpleFOC 2.3.3 class headers, using host
  Arduino/SPI stubs. This is not a complete ESP32 firmware build.
- Mocked-I/O tests of correction sign, angle wrapping and monotonicity, manual
  start, restoring-command sign and voltage bound, short and long dropout,
  CRC failure, driver fault, control-loop delay, active mode switching, manual
  stop, session timeout and displacement limit.
- These checks do not simulate real motor dynamics or establish thermal safety.

## Hardware result

Pending. No corrected spring result has been recorded yet.
