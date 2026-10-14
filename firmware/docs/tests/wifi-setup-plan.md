# WLAN and radar location setup

Prepared: 2026-10-14. ESP32 build and PCB verification pending.

## Scope

First step for the future Radar app: phone-based WLAN/location provisioning.
This is a separate `wifi_setup` firmware environment, not a completed radar.
Existing app GUI source and standalone test environments are retained.
All six motor control pins remain LOW throughout setup. LEDs remain off.
The screen uses the existing GC9A01 pinout, physical SPI3 (HSPI) at 20 MHz.

## Phone flow

1. Upload `wifi_setup`. The screen shows the hotspot name and password.
2. Join that hotspot on the phone. Choose to stay connected without internet.
3. Open http://192.168.4.1 manually if the setup page does not open automatically.
4. Enter the home 2.4 GHz Wi-Fi name and password. Standard personal Wi-Fi or
   an open network is supported; enterprise/certificate login is not implemented.
5. Confirm the radar centre. The initial approximate Schaffhausen city-centre
   coordinates are latitude 47.696 and longitude 8.635. They are editable.
6. Submit. Connection testing lasts up to 30 seconds. The phone may need to
   reconnect to the hotspot when the radio changes to the router's channel.
7. On success the screen says `Connected / settings saved`. The portal closes
   after 60 seconds. Without success it closes after ten minutes; reset reopens it.

A reset creates a fresh random hotspot password and tests saved Wi-Fi settings.
The setup remains available for 60 seconds after a successful saved connection.
A successful Wi-Fi association does not prove internet or flight API access.
There are no aircraft queries or additional cloud services in this step.

## Storage and failure handling

SSID, password and coordinates are saved as one versioned Preferences/NVS record
under namespace `knob-network`, key `config`. Source code contains no personal
Wi-Fi credentials. The record is for the future Radar integration to read.
Credentials are not printed to serial or reflected into form HTML. The generated
setup-hotspot password is shown only on the display. Form requests include a
per-boot token; responses are marked no-store. There is no production security
claim: the Wi-Fi record is ordinary device-local NVS, not encrypted storage.

New credentials are only persisted after a successful connection. Failed input,
connection timeouts or invalid coordinates do not intentionally overwrite the
last record. Storage write failure is shown separately. Existing config is
validated when loaded. Saved credentials are never prefilled into the web form.

Display shows failures, timeout and saved-state confirmation. The terminal is
optional; `x` stops provisioning and disconnects Wi-Fi. Reset restarts setup.
The motor never starts as a result of either Wi-Fi or form input.

## Test procedure

- Build with `pio run -e wifi_setup` before uploading.
- Verify hotspot and on-screen credentials; open the form on a phone.
- Test wrong Wi-Fi credentials: failure after 30 seconds, form remains usable.
- Test correct credentials: successful connection and saved confirmation.
- Reset with the same router available: check saved credentials reconnect.
- Reopen setup and test coordinate validation (decimal point, valid ranges).
- Return to `app_gui` with `pio run -e app_gui -t upload` if desired. This test
  does not yet make Wi-Fi available inside app_gui. Avoid erasing flash when
  checking persistence, as that would delete NVS too.
- Do not publish photos showing passwords or packet captures with credentials.

## Validation and sources

Host config validation tests passed for empty/unterminated fields, password
lengths, numeric parsing, invalid coordinate ranges, NaN and version mismatch.
These do not replace an ESP32 build or phone/radio/persistence tests.

Generated with ChatGPT assistance for this custom PCB and the user's requested
phone setup flow. Uses Arduino-ESP32 built-in WiFi, WebServer, DNSServer and
Preferences APIs. No OpenNextion source has been copied into this setup stage.

References:
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html
- Radar inspiration: https://github.com/OpenNextion/OpenNextion-Example-ESP32-Plane-Radar

## Hardware test result — 2026-10-14

- Firmware built and uploaded successfully.
- Wi-Fi credentials were entered through the phone setup page.
- The SmartKnob connected successfully.
- After disconnecting and reconnecting USB power, it reconnected
  automatically without entering credentials again.
- Flight data retrieval and radar integration have not been tested yet.
