# App GUI v1

Prepared: 2026-10-12. Initial ESP32 build succeeded and initial GUI operation was reported working. Updated detent mapping still requires a PCB test.

## Scope

This version connects the Arduino prototype's app layout to the tested motor,
HX711 pressure input, display and LED ring. All interface text is English.
The standalone test environments are retained.

| App | Local setting | Range | Step |
| --- | --- | --- | --- |
| Timer | Duration preset | 1–60 min | 1 min |
| Music | Volume setting | 0–100% | 1% |
| Light | Brightness setting | 0–100% | 1% |
| Temperature | Temperature setpoint | 16–30 °C | 0.5 °C |
| System | Actual display backlight | 10–100% | 1% |
| Sensors | Measured temperature, humidity, pressure | Read-only | Every 2 seconds |
| More | Info, status demo, standby, back | Four items | One item |

Timer is a duration preset, not a running countdown in this version.
Music, Light and Temperature are local settings without external-device
connections. Temperature is a setpoint, not a BME280 measurement.
The System app directly controls display backlight PWM. Applied settings are
kept in RAM until restart. The UI says "Applied / Until restart", not "Saved".
Wi-Fi, the clock and persistent storage are not part of this integration step.

## Interaction

- Turn counterclockwise to advance or increase, clockwise to go back/decrease.
- Home: eight detents per revolution (45 degrees each). Each step selects the
  next of seven apps; one physical revolution advances eight selections.
- Value screens: fifty detents per revolution (7.2 degrees each).
  Music/Light: 1% per step, exactly two revolutions between endpoint centres.
  System: 1% per step, 1.8 revolutions for its 10–100% range.
  Timer keeps 1 minute per step; Temperature keeps 0.5 °C per step.
- More screen: twelve detents per revolution.
- Display/LED markers represent the selected app or value, not shaft position.
- Short press: open, apply or select. The action occurs on confirmed release.
- A 900 ms hold after press detection: back/cancel. Releasing after a hold
  does not also generate a short press.
- A motor click is requested on initial press detection, as tactile feedback.
- Home hold: standby. More also offers standby.
- After 60 seconds without activity on Home, enter standby automatically.
- Standby disables motor output, darkens the LED ring and dims the display.
  A turn or press wakes it; the wake gesture is consumed.
- System brightness previews immediately. Hold to cancel restores the applied
  value. Applying it changes the RAM value until restart.

The first app marker starts at 6 o'clock. The physical LED chain and increasing
selection run counterclockwise. Home light-arc centres are spaced by 36/7 LEDs. Value screens
use a filling ring starting at 6 o'clock. The same direction is used on-screen.
The tested mapping is in `src/app_gui/model.h`: LED_BOTTOM=0, LED_CCW_STEP=1,
RAW_TO_CCW=1. This direction setting does not change motor commutation.

## Startup and operation

On boot, the motor remains disabled and the display asks the user to release
the knob. After three seconds, 32 HX711 samples establish the unloaded zero.
The code rejects an unstable zero and waits for a full filter, released input
and a usable stationary angle before enabling the motor at its current angle.
The zero check cannot distinguish an unloaded knob from a steady force applied
throughout the entire zeroing process: actually release the knob during boot.

There is no 10-second session limit and no normal-operation terminal menu.
`x` stops and latches the motor OFF. `r` restarts only while stopped; the reset
button also restarts the complete release/zero sequence. A fault never causes
an automatic motor restart.

## Motor and input settings

- Same measured baseline commutation model, seven pole pairs and PCB pinout
  as the tested combined-controls program.
- Spring limit 1.00 V command, stiffness 3.00 V/rad, slew limit 200 V/s.
- Click command ±3.00 V, 5 ms per direction, bounded independently of runtime.
- Pressure threshold 15000 counts, release threshold 7500.
- Press debounce 50 ms; raw release confirmation 150 ms, preventing the
  eight-sample rolling average from misclassifying a short release as a hold.
- Samples shortly after a motor click are ignored for 150 ms while the logical
  press state is retained. Another press requires a confirmed release.
- At value limits, the logical value is clamped and the current detent acts
  as an elastic end stop. Excessive displacement stops the motor.
- Brief tracking loss retains the existing bounded hold below 2 ms at low
  speed. A click is cancelled on an invalid encoder frame.
- Driver, CRC, angle-jump, speed, pressure timeout and timing checks remain.
  A control-loop gap over 2 ms or encoder outage reaching 5 ms stops output.

Weak magnetic field remains unresolved. Continuous operation is a new operating
condition: the previous short hardware tests did not validate long-duration
motor/driver temperatures or supply current. Voltage commands are not measured
winding voltages or measured current limits. Stop if motor/driver heating is
unexpected, and record the result before treating continuous use as validated.

## Implementation

`app_gui_main.cpp` owns motor control and app state on core 1. `pressure.h`
owns pressure acquisition and gesture classification. `model.h` defines the
local app state and interaction rules. `view.cpp` renders copied snapshots on
core 0. No LVGL, display drawing, LED writes or flash-storage writes run in the
motor loop. The snapshot queue keeps the latest state instead of replaying a
backlog of screen updates.

The display retains the tested 8 MHz SPI setting and uses physical SPI3
(Arduino HSPI on this ESP32-S3 core). The encoder retains SPI2_HOST. Backlight
uses LEDC while SimpleFOC uses MCPWM. The UI uses the existing 64 KiB LVGL
configuration. No changes to `include/lv_conf.h` are needed.

## Validation before delivery

- Host syntax checks with real SimpleFOC 2.3.3, HX711 0.7.5, LVGL 8.3.11 and
  NeoPixel 1.12.3 headers; hardware APIs stubbed where required.
- All app screens rendered with real LVGL 8.3.11 and visually inspected.
- Repeated rendering across all value ranges passed LVGL memory checks;
  approximately 31 KiB of the 64 KiB LVGL heap remained free.
- Mocked control tests covered automatic startup, operation beyond ten seconds,
  detent selection, value steps, 3 V pulse timing, short-release action, long
  cancellation, standby/wake, cancelled clicks and latched sensor faults.
- Model/input tests covered all value bounds, apply/cancel, brightness rollback,
  startup held input, no extra short event after a hold, timer rollover and
  rejection of an unstable pressure zero.

These checks do not replace the ESP32 build, real-time timing checks or PCB tests.

## First PCB test

1. Build `app_gui`. Upload only after the build succeeds.
2. Keep hands off the knob during startup. Observe "Release knob" and zeroing.
3. Confirm that the menu becomes usable without a terminal start command.
4. Turn through all seven apps in both directions. Check LEDs and display together.
5. Test a short press, a long press, and a held press followed by release.
6. Change a value, cancel, reopen and verify that the previous value remains.
7. Under System, preview brightness; test both cancel and apply.
8. Enter standby, then wake by turning and by pressing. No extra app should open.
9. Verify `x` stops the motor and ordinary knob input cannot restart it.
10. Note timing stops, missed/cancelled clicks and thermal observations. Record
    build output, a short demo and remaining limitations before committing results.

## Sources and AI assistance

ChatGPT adapted the supplied Arduino UI and the tested combined-controls code,
added the app model, automatic startup and pressure gesture handling, and ran
the host checks. PCB testing is performed by the project author.

The motor code retains the Scott Bezek SmartKnob attribution and the repository's
Apache-2.0 notice. The previously reviewed Scott/SeedLabs task separation informed
the motor/UI split. Icons/layout and fonts are reused from the supplied prototype.
The font license is included as `docs/licenses/GUI-Font-OFL-1.1.txt`.

## Live carousel update

- Home presents the next counterclockwise selection on the left. Turning left
  brings that card into the centre. Value direction and motor commutation are unchanged.
- Motor snapshots include fractional displacement from the selected Home detent.
  The renderer follows that displacement, including the physical return towards
  a detent, with a short damped visual spring. Card positions, sizes and colours
  interpolate continuously; labels and LEDs identify the logical selection.
- The six-app wrap and detent boundary use continuous circular coordinates.
- Animation runs exclusively in the display task. Eight Home detents and fifty
  value detents per revolution remain, as do the existing motor limits and guards.
- Host checks: source syntax, real LVGL rendering, left/right neighbour placement,
  fractional motion, settling, detent-boundary continuity and last/first wrap passed.
- PCB build, animation responsiveness and simultaneous motor timing for this update
  remain to be tested. Initial GUI and previous detent update were reported working.

## Carousel responsiveness update

The user reported visible lag during fast rotation with the visual spring.
The renderer now follows the latest fractional knob position directly, without
an additional simulated spring. The physical spring return remains visible.
Unchanged labels, card styles and stacking order are not updated each frame;
subpixel movements under approximately half a pixel do not trigger a redraw.
State snapshots are published every 10 ms through the existing latest-only queue.

Display SPI is now experimentally 20 MHz instead of 8 MHz. This speeds pixel
transfer but requires validation with the actual flex cable and display. If
pixels corrupt or the display becomes unstable, restore DISPLAY_SPI_HZ to
8000000UL in src/app_gui/view.cpp and report the result. Motor limits, sensor
SPI and stop checks remain unchanged. No frame-rate guarantee is made.

Host checks passed for source syntax, real LVGL rendering/heap integrity,
selection direction, wrap continuity and immediate tracking of the latest
position. ESP32 build, display integrity and motor timing remain PCB checks.

## Continuous LED feedback

The user reported smooth motor and display operation with the faster carousel.
Home now shows a raised-cosine light arc approximately six LEDs wide. Its centre
follows the same fractional app coordinate as the carousel, including physical
spring return. Adjacent app colours blend continuously. LED 0 is at six o'clock;
increasing selection follows the physical counterclockwise chain. Each app spans
six LEDs, so the ring represents the six-app carousel rather than absolute shaft
angle (Home still has eight physical detents per revolution).

Value rings now render a fractional final LED instead of truncating the fill to
whole LEDs. Global brightness remains 20/255. Standby, fault indication and the
short white press highlight are retained. No motor code or limits were changed.
Host checks passed for real LVGL rendering, glow wrap and detent continuity,
symmetric falloff, fractional value fill, endpoint bounds and standby darkness.
ESP32 build and PCB appearance/timing of this LED update still require testing.

## Sensors and double-clap integration — 2026-10-13

Inputs: the author's uploaded bme280_test.cpp and clap_detection.cpp.
The previous smooth LED update was reported working on the PCB. This integration
has passed host checks but has not yet been built or tested on the ESP32.

Sensors is a seventh app, before More. It displays measured temperature (°C),
relative humidity (%RH), and local pressure (hPa, not sea-level corrected).
Short press or hold returns Home. Rotation does not edit readings. Temperature
remains the separate local setpoint editor. Home still has eight detents per turn;
values still have fifty. Sensor LED positioning uses the seven-app mapping.

BME280 uses SDA 48 / SCL 41 at 100 kHz, address 0x76 and ID 0x60. Forced-mode
x1 sampling and a two-second read interval are retained. Errors hide numbers;
data older than six seconds is marked stale. Initialization/read failures retry.
The BME task can wait for its peripheral without blocking motor control or audio.

ICS-41350 uses CLK 38 / DATA 39 / SELECT 40 LOW, I2S0 PDM stereo at 16 kHz,
256-frame analysis and I2S_PDM_DSR_16S, matching the supplied working test.
Detection retains minimum RMS 600, background factor 4, rise factor 1.8,
maximum impulse duration 160 ms and two quiet windows. Pair onset spacing is
120–550 ms; successful pairs have a 1000 ms lockout. A two-second background
learning period follows warmup. This is heuristic detection, not a guarantee
that every pair of short sounds is clapping.

A detected pair overlays all 36 LEDs in white for approximately 200 ms at the
existing brightness 20/255, then restores the current app display. The pair
never toggles a setting or starts the motor. It also flashes in standby and
then returns dark; stopped/fault LED indication takes precedence. Detection
status is visible at the bottom of Sensors. Audio errors reset/relearn detection.
Pair timing uses contiguous audio frames; overflow/gaps discard pending pairs.

Motor press-click requests suppress detection for 250 ms and clear any pending
pair. This reduces self-triggering but can miss real claps during a press.
Detent/motor noises and other ambient sounds still require combined PCB tests.

Tasks: motor/control stays on core 1. Audio runs on core 0 at priority 2 and
blocks waiting for DMA audio. BME and display run at priority 1 on core 0.
Each worker publishes a one-element queue; only the display task writes LEDs
and LVGL. No background serial logging is added. Motor guards remain unchanged.

Validation: detector tests for single clap, valid pair, long impulse, late pair,
lockout, suppression and time rollover; Sensors/More navigation; real LVGL screen
rendering and heap integrity; all-white overlay and restoration, including standby
and fault precedence; host syntax checks. Hardware APIs were mocked in host tests.
The IDF 4.4.7 I2S header and BME library 2.2.4 declarations were checked.

PCB test: first build app_gui. Upload after success, release the knob for zeroing
and stay quiet during audio learning. Open Sensors and check plausible changing
readings plus 'Clap: ready'. Try one clap (no flash), then two 120–550 ms apart
(white flash, previous ring returns). Repeat from another app and standby.
Also turn/press without clapping to check false triggers and control-loop faults.
Record the observed result before marking this integration hardware-tested.

ChatGPT adapted the supplied sensor/detector source, added background tasks,
queue-based telemetry, sensor UI and the transient LED overlay. The author
performs the ESP32 build and PCB tests.

## Clean interface and app-colour easing — 2026-10-13

Removed decorative SMART KNOB headings, press/hold instructions, wake hints,
and success text. Confirmation retains its check icon. App names, value captions,
units and readings remain. Sensors hides normal live/ready messages; initialization,
learning and fault/stale messages remain. Release-knob and stop/fault instructions
are retained when relevant. RAM-only settings behaviour is unchanged.

Home LED colour follows the same continuous fractional app coordinate as the
carousel. Smoothstep blending gives exact app colours at detent centres and a
soft transition in either direction, including the last/first app wrap. No timed
colour animation adds lag. The temporary white clap/press highlights still take
precedence, then restore the current app colour. Motor settings are unchanged.

Host validation: actual LVGL screens rendered and inspected, normal sensor status
hidden, detent and wrap colour continuity, existing LED overlay restoration and
LVGL heap checks passed. PCB appearance remains to be checked after upload.

## Distinct app palette — 2026-10-13

Replaced pastel accents with saturated app colours shared by the display and LED
ring: Timer #FF1800 (red), Music #00FF20 (green), Light #FFD000 (yellow),
Temperature #0060FF (blue), System #C000FF (violet), Sensors #00FFD0 (turquoise),
More #FFFFFF (white). Sensors heading now reads the shared palette directly.
Global LED brightness and position-based colour blending remain unchanged.
Physical colour appearance at brightness 20/255 still needs a visual PCB check.
