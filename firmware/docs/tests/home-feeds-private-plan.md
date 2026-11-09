# Private home-feed configuration

The five complete HTTPS addresses belong in
`firmware/include/home_feeds_private/config.h`. This file is ignored by Git.
Use `config.example.h` as the template for a new installation. The page order
is freezer, humidity, garage temperature, garage light and air pressure.

Without the local file the firmware builds with empty addresses. HTTP pages
show `Not configured`; the independent MQTT page continues to work.
The exact optional PHP warning prefix is private too. Unknown HTML and errors
must still be rejected. TLS verification and request timeouts are unchanged.

Only page numbers, never URLs or private feed IDs, are logged.
The garage light page still displays On/Off using an explicit page property.

## Checks on the device

- Build and upload app_gui with the migrated private file.
- Check all five HTTP pages, including garage light On/Off.
- Check the sixth MQTT page and BME280 temperature publishing.
- For a clean checkout without private config, verify `Not configured`.
- Run `git check-ignore firmware/include/home_feeds_private/config.h` from root.
- Do not commit local backup folders, logs or private configuration.

The owner confirmed MQTT sending and receiving on 2026-11-09, before this patch.
Device verification after this configuration change is pending.

## Publication history

Ignoring a file does not remove addresses from earlier commits, forks or clones.
If old URLs allow private data access, restrict that access at the server and
replace any exposed credentials. History cleanup is a separate operation.
