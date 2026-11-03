# FFT audio comparison — 2026-10-27

Goal: compare actual hand claps against speech, enclosure handling and passive
rotation on the assembled SmartKnob. This is a measurement firmware, not a
validated clap classifier. Phase labels are entered by the operator.

Motor outputs stay LOW; no haptic feedback is generated. Tests with active
motor/press feedback remain pending. The existing GUI files are not modified.

Acquire quiet, single clap, double clap, speech, case rubbing, passive rotation,
light case tapping, and quiet again, using commands 0 through 6. Use 10 clap
events/pairs and roughly 10 seconds for each other phase. Keep distance and
placement recorded. Repeat later with other distances and a different room;
do not evaluate only on the same recordings used to tune thresholds.

Store the raw log and record actual actions, false triggers and missed events
once classification is added. A measured spectral feature or an impulse flag
alone is not evidence that a clap has been correctly classified.

Source files: `src/clap_fft_test.cpp`, `src/clap_fft_features.h`.
FFT: 256-point Hann-windowed frames, 16 kHz, no overlap, DC removed.
The two receive slots belong to one PDM microphone; selected slot stays fixed.
Receive setup matches the supplied standalone clap test (DSR_16S).
Full serial records are queued to a separate task. Queue drops and read errors
are reported; timestamps and sequence numbers should also be inspected.

Host validation, from firmware:

```sh
g++ -std=c++17 -Wall -Wextra -Werror docs/tests/clap-fft-host.cpp -o clap-fft-host
./clap-fft-host
```

Passed: DC removal, sine RMS, band-energy fractions, spectral centroid,
spectral flatness, identical-frame flux, broadband impulse and clipping count.
Firmware syntax/printf types also checked with API stubs during preparation.
ESP32 compilation, serial throughput, microphone response and physical
discrimination remain unverified until the device test.
