# Plane Radar integration — v1

Generated with ChatGPT assistance, 2026-10-14. Experimental hardware validation pending.

## Previously tested

Standalone `wifi_setup` was built, uploaded and tested by the owner. Phone
provisioning and automatic reconnection after removing USB power succeeded.
That result does not validate the integrated network/motor workload below.

## Controls and presentation

- Radar is the eighth carousel app (before More); existing apps remain available.
- Home retains eight detents per revolution and continuous app-colour LED blending.
- Radar colour: orange. The LED arc retains this app colour while viewing Radar.
- Radar view: turn to cycle 5, 10, 15 and 25 km radius. North is up.
- Short press toggles radar/details. In details, turn to cycle the aircraft
  currently inside the selected range, ordered by distance. The list may reorder
  after a refresh; selection is by list position, not a permanently tracked ID.
- Long press returns Home. Existing pressure feedback and double-clap flash remain.
- Triangle direction represents ground track, not necessarily aircraft heading.
  A dot indicates unknown track.
- Details show callsign (or hex ID), type if supplied, distance, barometric altitude
  in metres and ground speed in km/h. Missing values show `--`, not zero.
- Altitude is converted from reported barometric feet, not height above local ground.
- Centre uses the saved provisioning coordinates (default Schaffhausen 47.696, 8.635).
- Existing motor voltage, calibration, dropout handling and stop guards are unchanged.

## Network design

Read the existing versioned `knob-network/config` Preferences record. No credentials
are embedded in source, printed or written back by this app. Run `wifi_setup` again
if credentials/coordinates need changing, then upload `app_gui` again.

Wi-Fi starts before motor initialization. HTTPS fetches and JSON parsing run in a
separate core-0 task and publish copied snapshots through a one-element queue.
They never call motor functions. This separation does not guarantee real-time
behaviour: Wi-Fi interrupts and shared ESP32 resources still require hardware testing.
The existing 2 ms control-loop stop remains enabled.

No AP portal is opened in the integrated GUI. The network is automatically
reconnected at 30-second intervals. SNTP must establish a valid clock before HTTPS.
TLS validates the hostname/certificate using public roots downloaded from
https://pki.goog/roots.pem on 2026-10-14. There is no insecure fallback. A changed
certificate chain may require updating the CA file.

Fetch only while Radar is open, about every 15 seconds after the preceding fetch
finishes. An in-flight request may finish after leaving Radar. Errors back off for
60 seconds. Wi-Fi/clock/error states are shown in the Radar screen.

Fetch 14 nautical miles, then filter locally to 25 km. Retain the nearest 24 aircraft.
The response body is capped at 128 KiB in PSRAM; filtered JSON uses a 48 KiB bounded
allocation. Malformed or oversized responses retain the last successful snapshot.
Reject missing/invalid positions and positions older than 15 seconds at the source.
Reject provider timestamps more than 60 seconds from the SNTP clock. At 30 seconds
since receipt, show Stale; at 60 seconds hide aircraft until another good response.
An empty successful response means no aircraft, not a connection failure.

The small-area projection approximates north/east distances around the configured
centre. This is a hobby display, not navigation or operational aviation equipment.
Coverage, aircraft visibility and freshness depend on the data provider.

## Provider and references

Flight data: [adsb.fi](https://adsb.fi/).
API documentation and terms: https://github.com/adsbfi/opendata/blob/main/README.md
Endpoint: https://opendata.adsb.fi/api/v3/lat/47.696/lon/8.635/dist/14

The public API is for personal, non-commercial use, requires attribution and is
rate limited to one request per second. This app polls substantially less often.
A commercial deployment needs a separate review/agreement with the provider.

Visual/function inspiration: OpenNextion Plane Radar:
https://open-nextion-app-store.nextion.tech/firmware/plane-radar/
Reviewed source:
https://github.com/OpenNextion/OpenNextion-Example-ESP32-Plane-Radar/blob/open-nextion-boards/src/services/adsb_client.cpp
The current upstream client also uses adsb.fi v3. This implementation is newly
written for the existing SmartKnob LVGL/core split; upstream firmware is not flashed
or copied wholesale. No runway layer or flight route/destination lookup is included.

## Verification performed on the host

- A real public API response for Schaffhausen was retrieved and parsed successfully.
- Synthetic parser tests: missing fields, invalid JSON, stale positions, callsign
  trimming, distance sorting, feet/metres and knots/km/h conversion.
- App model: Radar entry, range wrap, detail selection, long-press exit and existing apps.
- Actual LVGL 8.3.11 host render of Radar/details and the other screens, including
  LED flash restoration and available LVGL heap (about 29 KiB remained).
- The relevant HTTPClient and WiFiClientSecure method signatures were checked
  against Arduino-ESP32 2.0.17 headers.
- No ESP32 compile/upload or combined hardware test has yet been performed for v1.

## Hardware test to record

1. Build/upload `app_gui`; leave the knob released during pressure zeroing.
2. Check ordinary app navigation, LEDs, press feedback and motor smoothness.
3. Open Radar. Allow Wi-Fi and SNTP to connect, then verify live data or a clear
   empty/error message. Confirm all four ranges and details/back navigation.
4. Rotate and press during multiple network refreshes. Stop with `x` if needed.
   Record any motor stop, reboot, jitter or audio interference; do not relax guards.
5. Disconnect WLAN temporarily: confirm error/stale handling, then recovery.
6. Check double-clap detection, Sensors, standby/wake and a USB restart.
7. Run at least 10 minutes and record stability, motor temperature and free-memory
   behaviour before describing the integrated workload as validated.

  ## Hardware test result — 2026-10-15

- Firmware built and uploaded successfully.
- The Radar app displayed aircraft data on the SmartKnob.
- The displayed data matched the owner's comparison.
- A demonstration video was recorded.
- Extended stability and network recovery testing remain pending.

[Demo video](../videos/plane-radar-demo.mp4)
