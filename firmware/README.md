## Status

- PlatformIO build completed successfully.
- Firmware uploaded to the custom SmartKnob-NB PCB.
- Serial output ran for one minute without visible interruptions.
- PSRAM memory access, sensors and motor have not been tested yet.

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