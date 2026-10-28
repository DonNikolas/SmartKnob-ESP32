# Settings and standby clock

Implemented from the uploaded MQTT preparation files, 2026-10-28.

## Host validation

Run on Linux with g++ and the existing background.h/mqtt_config.h dependencies:

```sh
g++ -std=c++11 -Wall -Wextra -Isrc/app_gui docs/tests/settings-menu-host.cpp -o /tmp/settings-menu-test
/tmp/settings-menu-test
```

The test covers navigation and default cancellation, bounded standby options,
automatic standby disabled, exact timeout, waking, millis wraparound, running
timer protection and completion display, valid version-1 migration, invalid
records, unsynchronised clock and both Zurich DST boundaries in 2026.
During patch validation, background.h and MQTT label constants were provided
as host dependencies; no MQTT connection or hardware behaviour was simulated.
Host tests passed. Full ESP32 compilation, NVS, Wi-Fi and display tests pending.

## Hardware checks

- Build app_gui and upload. Existing values and MQTT page 6 must remain.
- Confirm Settings has Wi-Fi, Auto standby and Back. No Info/Status Demo entry.
- Change timeout to 30 sec, leave Settings and wait. Clock appears after inactivity.
- Wake by turning and pressing. Wake action must not send a Light command.
- Choose Off and wait longer than 30 sec. Home must remain awake.
- Change to 2 min, wait for settings save, power-cycle and check selection.
- Run a timer longer than the timeout. It must continue; end indication gets 10 sec.
- Check clock against local time with Internet, then disconnect Internet: clock continues.
- Wi-Fi confirmation defaults to Back. Back must not restart.
- Confirm setup. Motor stays off. Screen shows hotspot/password and portal address.
- Try incorrect credentials: existing saved connection must survive cancellation/reset.
- Save valid new credentials: normal GUI returns, Radar and MQTT reconnect.
- Check IR companion reconnection after returning to normal mode.
- Setup exit/reset must not loop into setup. Setup expiry is 10 min.
- Record Boxtec error shown when a page has no valid reading; do not declare a fix yet.

The portal intentionally runs in a separate boot mode within app_gui. This avoids
sharing port 80 and the hotspot with the active IR service. App preferences are
saved with motor output disabled before reboot; a running timer is not restored.
Credentials are not printed and the portal uses a per-boot form token.
