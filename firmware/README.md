## Status

- PlatformIO build completed successfully.
- Firmware uploaded to the custom SmartKnob-NB PCB.
- Serial output ran for one minute without visible interruptions.

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

## Display test

Date: 2026-10-06

The display test runs on the current SmartKnob PCB with the display
adapter connected by a flex cable.

### Configuration

- GC9A01 display, 240 x 240 pixels.
- SPI clock: 1 MHz.
- Arduino_GFX: 1.4.7.
- LVGL: 8.3.11.

### Troubleshooting

The first flex cable produced an incorrect pin mapping.
After replacing it, the backlight worked but no image appeared.

Reducing the SPI clock from 20 MHz to 1 MHz did not fix the problem.
The display started working after I checked and resoldered its connections.
This suggests a solder connection problem.

### Result

The display test worked after resoldering.
The recorded test uses 1 MHz SPI. Higher speeds have not yet been
retested after the repair.

[Display test video](docs/images/Display-Test/display-test-demo.mov)

![Display test setup](docs/images/Display-Test/display-test-setup.jpg)

### AI assistance

ChatGPT generated the test code and helped with configuration and
troubleshooting. I checked the wiring, resoldered the display connections
and tested the hardware.

## BME280 test

Date: 2026-10-06

- SDA: GPIO 48.
- SCL: GPIO 41.
- BME280 detected at address 0x76.
- Sensor ID: 0x60.
- Initialization passed.
- Temperature, humidity and pressure readings were received every two seconds.

The basic function test passed. Measurement accuracy has not been
checked against a reference instrument.

Another I2C device responded at 0x10. Its function was not tested.

[Serial test output](docs/tests/bme280-test.txt)

ChatGPT generated the test code. I built, uploaded and tested it on my PCB.

## Microphone test — initial results

Date: 2026-10-07

- Microphone: ICS-41350.
- PDM clock: GPIO 38.
- Data: GPIO 39.
- Channel select: GPIO 40, LOW.
- PCM sample rate: 16 kHz, 16-bit mono.

The firmware built and uploaded successfully.
Sample data was received continuously during the office test.
RMS values ranged from 7.7 to 9.9 and peak-to-peak values reached 176.
No clipped samples were reported.

A clear response to controlled sound has not yet been confirmed.
Clap detection is not implemented. Further checks of the microphone
signal and shorter analysis windows are planned.

[Serial test output](docs/tests/microphone-test.txt)

ChatGPT assisted in generating the test code. I uploaded it and recorded the
measurements on my PCB.