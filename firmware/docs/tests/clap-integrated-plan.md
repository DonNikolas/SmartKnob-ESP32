# Integrated FFT clap and IR test

Prepared 2026-11-01. The user confirmed that manual power and dimming already
operate the real lamp. The combined FFT/motor/IR test is still pending.

## Before the test

- Apply the update and build/upload `app_gui`.
- Use the fully assembled Knob and the working IR companion and lamp.
- Wait quietly for startup calibration. Leave all detector thresholds unchanged.
- If the Sensors app reports no signal or resync, resolve capture first. Do not
  compensate for an intermittent microphone by lowering the clap threshold.
- Start a serial log if possible; note the wall time of each test section.

## Record actual counts

| Test | Actual attempts | Recognised pairs / lamp toggles | Notes |
| --- | --- | --- | --- |
| 10 double claps, about 0.5 m away, 3 s between pairs | | | |
| 10 double claps from normal intended operating distance | | | |
| 10 single claps, at least 2 s apart | | | |
| 1 minute normal speech | | | |
| 1 minute music at a normal room level | | | |
| 20 slow/fast turns through app detents, without clapping | | | |
| 20 short presses with haptic feedback, without clapping | | | |
| 10 long presses and app exits, without clapping | | | |
| 20 ordinary touches or light taps on the case | | | |

Do not mark every requested count as completed automatically. Enter what was
actually performed. Record missed detections and false positives separately.
Finite success in this test is not a guarantee of general reliability.

## Integration checks

1. Confirm one recognised pair gives one white flash and one lamp toggle.
   Check from Home, Light, another app and Standby. Return of LEDs must preserve
   the previous app indication. App selection must not change from a clap.
2. Press the knob repeatedly: haptic clicks alone must not toggle the lamp.
   A deliberate clap immediately after a click may be missed during the 300 ms
   suppression interval. Wait at least one second for the ordinary clap test.
3. Confirm turning/display animation and short/long presses remain usable.
   Stop and retain the serial log if a motor/control fault occurs. Do not raise
   motor timeout limits to hide a scheduling problem.
4. Unplug the IR companion and double-clap. Recognition can still flash white;
   serial should show the request dropped/offline. Reconnect the companion:
   there must be no old toggle. A new pair should issue a new request.
5. With `x` stopped, claps must not send lamp requests or replace the fault/stop
   ring. Restart using the existing `r` command before continuing normal use.
6. Leave the device running for at least 15 minutes, then repeat presses,
   rotation and a counted set of double claps. Check the timer, sensor readings
   and network apps during normal use.

## Local host checks

From the `firmware` directory on a system with g++:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined docs/tests/clap-integrated-host.cpp -o clap-integrated-host
./clap-integrated-host
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=undefined -I docs/tests/clap-integrated-stubs docs/tests/clap-background-host.cpp -o clap-background-host
./clap-background-host
```

Optional replay: pass the three original device-monitor logs as arguments to
`clap-integrated-host`. The test compares its pair count with `DOUBLE_CLAP`
events in each supplied log. It assumes logs start with sufficient quiet input
for calibration; these three recordings meet that condition. It is a regression
check, not an independent classification label or a measured accuracy score.

Local results: both host executables passed; replay counts were 0 / 5 / 5.
Hardware result: pending. Add build result, firmware commit, distance, actual
counts, faults and observations after testing.
