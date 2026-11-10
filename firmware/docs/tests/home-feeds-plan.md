# Home Sensors

Generated with ChatGPT assistance.

## Status — 2026-10-16

The firmware built and uploaded successfully. The user tested the Home Sensors
app on the SmartKnob and accepted the simplified display. A demonstration video
was recorded. This establishes basic operation, not a completed reliability test
or independent verification of the physical measurements.

[Home Sensors demo](../videos/home-sensors-demo.mp4)

## Interaction and appearance

Home Sensors is appended as the ninth carousel app, with a pink colour and home
icon. Existing app indices remain unchanged. Home retains eight physical detents
per turn. Inside Home Sensors there are twelve detents per turn; each step changes
the selected measurement, wrapping across the five pages. Short or long press
returns Home. The local BME280 Sensors app remains separate.

The display shows the measurement name, value, unit where applicable, and page
counter such as 1/5. The source-age text, Boxtec footer and network-status text
were removed at the user's request. Garage Light uses ON/OFF without a unit.
Unavailable or invalid readings show --. Previously received values can remain
visible after a connection failure; a visible number alone is not proof that the
connection is currently working.

The original measurement timestamp remains available internally. Values older
than PRIVATE_FEED_ID seconds are dimmed, without an age label. This threshold may require
adjustment for sensors with slower update intervals. The humidity feed had an
old source timestamp during development; fetching it again does not make the
underlying measurement current.

## Implementation

- Dedicated core-0 worker; no motor access, synchronous GUI fetches or new NVS writes.
- Uses the existing configured station WLAN and clock setup.
- Fetches the selected feed every 15 seconds while the app is open.
- A two-second minimum between request starts limits rapid browsing traffic.
- An in-flight request may finish after leaving the app; no new request starts then.
- Each measurement has a separate cache to keep values matched to their labels.
- Requires a numeric value and valid source timestamp; null does not become zero.
- A bounded response sink rejects oversized responses. Errors preserve cached values.
- HTTPS certificate verification remains enabled; no insecure fallback.
- Existing settings schema is retained. The new app selection can be saved;
  the measurement page starts at Freezer after reboot. Remote values are not saved.
- Older firmware may reject a settings record selecting the new app index 8 and
  load defaults. Motor constants, pressure handling and IR pairing are unchanged.

## Development validation

Host checks used actual ArduinoJson 6.21.5, the app model, motor-controller code
with hardware mocks, and real LVGL 8.3.11 rendering. Checks covered malformed
responses, source timestamps, preserved values, navigation, saved app selection,
IR gesture dispatch and delayed preference writes. The final view was checked
with Garage Light values 1, 0, 117 and missing data, and the page-only footer.
Host checks do not establish physical sensor accuracy or wireless timing reliability.

From repository root, with a local ArduinoJson 6.x include directory:

```sh
g++ -std=c++11 -I path/to/ArduinoJson/src firmware/docs/tests/home-feeds-host.cpp -o home-feeds-host
./home-feeds-host
```

## Follow-up checks

- Compare all displayed values with the intended physical home measurements.
- Observe both real Garage Light states and confirm the feed changes between 0 and 1.
- Check Wi-Fi loss/recovery and cached-value behaviour over an extended period.
- Check rapid navigation and concurrent network activity for motor deadline faults.
- Regression-check Timer, IR lamp control and local BME280 readings.
- Implement and test the separately agreed direct MQTT communication.

API reference: https://github.com/emoncms/emoncms/blob/master/Modules/feed/feed_controller.php
Public ISRG roots: https://letsencrypt.org/certificates/
