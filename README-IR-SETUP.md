# SmartKnob IR light control — first hardware test

Generated with ChatGPT assistance for the existing SmartKnob and the user's
working ESP8266 IR board. This is a patch for main after timer-settings v2.

## Install the SmartKnob patch

Copy the contents of this package into the repository root, merging directories.
Do not replace/delete entire existing directories. Keep the generated private
headers with their matching `.gitignore` files. The package already contains a
matching random WLAN password and API key; no credentials need to be entered.

In the existing `firmware` PowerShell terminal:

```powershell
pio run -e app_gui
pio run -e app_gui -t upload
```

The Light app initially displays Offline until the companion is running.
The new AP is local: `192.168.8.1`. The existing stored WLAN credentials remain
unchanged. Internet-dependent Radar continues using the station connection.
Lamp control does not require an external router or Internet.

## Install the IR companion

The supplied `Receive-IR.ino` uses ESP8266 libraries. This companion therefore
also targets ESP8266, not ESP32-S3. Use the same board selection and USB port
that worked with that old IR sketch. Do not flash this companion on SmartKnob.

Open `ir-bridge/SmartKnob_IR_Bridge/SmartKnob_IR_Bridge.ino` in Arduino IDE.
Keep `config_private.h` and `receiver_state.h` in the same sketch directory.
Use ESP8266 Arduino core 3.1.2 and install:

- IRremoteESP8266 2.8.6
- ArduinoJson 6.21.5 (not 7.x)

IR output is GPIO14, unchanged from the working old sketch. Existing IR driver
hardware remains unchanged. Upload to the IR board. Its serial monitor at
115200 reports each IR transmission without printing credentials.

## Controls

| Action | Result |
| --- | --- |
| Open Light | Show IR link and last command; no guessed brightness percentage |
| Turn in the existing increasing direction | Send one brighter command per detent |
| Turn in the opposite direction | Send one dimmer command per detent |
| Short press inside Light | Alternate explicit ON/OFF requests |
| Long press inside Light | Return Home |
| Detected double clap | Toggle the lamp request and flash the ring white |

Light has 12 detents per turn. On startup or after an uncertain/disconnected
sequence the first toggle requests ON. Opening Light alone sends nothing.
Commands are never restored from saved settings. The old stored light percentage
is retained only for preferences-record compatibility and is no longer displayed
or sent. Timer, Music, Temperature, System and Radar preferences remain intact.

`ON sent` means the IR board acknowledged transmitting ON. It does not prove
that the lamp received it. The original remote can change the lamp independently,
so requested and actual state can differ. The yellow ring indicates link/activity,
not a measured brightness. Presses retain the existing haptic click.

## Verify before committing

1. In Light, press twice: confirm the real lamp turns ON, then OFF.
2. Turn slowly in both directions; confirm brighter/dimmer direction and one
   response per detent. Hold a press and verify it is not repeatedly toggled.
3. Double-clap in Home and Light: one toggle and a brief white ring, then restore.
4. Unplug the IR board, turn the knob, reconnect. Old commands must not replay;
   a new press should explicitly request ON. Repeat with SmartKnob restarted.
5. Use Timer and switch to Radar. Check that the UI/motor remain smooth while
   IR commands are sent, and the Timer LEDs still start at twelve o'clock.
6. Disconnect the external WLAN/router. Local lamp control must still work.
   Reconnect it and verify Radar recovers without stale lamp operations.

AP+STA shares a radio; joining another WLAN can change the AP channel and briefly
disconnect the bridge. Requests during a gap are discarded. Avoid an external
LAN using the same 192.168.8.0/24 subnet. The AP permits one companion station.

## Private configuration

Do not publish this ZIP: it contains the generated private pairing headers.
The two headers are ignored by their adjacent `.gitignore` files. Commit source,
docs, the generator and ignore files, but never force-add the private headers.
A fresh clone can regenerate a pairing using `python tools/generate_ir_pairing.py`.
To intentionally replace a pairing use `--replace`, then reflash both boards.
Do not commit the old sketches with their embedded home WLAN credentials.

## Validation status

Host protocol/model tests, motor-controller tests with hardware mocks, and real
LVGL 8.3.11 rendering passed. ESP32/ESP8266 firmware compilation and the actual
lamp/network/haptic tests must be performed on the user's equipment. No claim
of physical lamp response or wireless timing reliability is made by host tests.
