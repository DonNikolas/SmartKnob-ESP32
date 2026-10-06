## Status

- PlatformIO build completed successfully.
- Firmware uploaded to the custom SmartKnob-NB PCB.
- Serial output ran for one minute without visible interruptions.
- Sensors, Display and motor have not been tested yet.

## LED ring test

Date: 2026-10-05

The test uses 36 LEDs on GPIO 11 with low brightness.

### Test sequence

- A single LED moves around the ring in red, green and blue.
- The whole ring then lights up in red, green and blue.
- The sequence repeats.

### Result

The firmware was built and uploaded successfully.
All 36 LEDs worked during the test, and the colours were correct.

### Test photos

![Red LED ring](docs/images/LED-Test/LED-Test-Red.jpg)
![Green LED ring](docs/images/LED-Test/LED-Test-Green.jpg)
![Blue LED ring](docs/images/LED-Test/LED-Test-Blue.jpg)

## PSRAM test

Date: 2026-10-06

- Detected PSRAM: 8 MiB.
- Allocated a 1 MiB test buffer in PSRAM.
- All four write/read patterns passed.
- This was a basic function test, not a test of the entire 8 MiB.

The initial size check compared usable heap capacity with physical
memory size. This was corrected to query physical PSRAM size.

[Serial test output](docs/tests/psram-test.txt)

## AI assistance

ChatGPT generated the LED and PSRAM test code based on my requirements
and helped with the PlatformIO configuration and troubleshooting.
I built and uploaded both programs, tested them on my PCB and
recorded the results.