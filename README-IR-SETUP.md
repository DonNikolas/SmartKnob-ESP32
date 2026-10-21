# SmartKnob IR light control

The SmartKnob sends commands to my ESP8266 IR board. The board then
sends the IR signal to the lamp. I reused the pin assignment and IR
codes from my previous smart home project.

I developed and tested this integration with ChatGPT assistance.

## Current status

Both programs build and upload successfully. The SmartKnob and
ESP8266 connect, and ON/OFF commands appear in the ESP8266 serial monitor.

During the test on 19 October 2026, the lamp did not respond.
It still worked with its original remote. I compared the old and new
sketches: GPIO14, the IR codes and the NEC32 send calls are identical.

The cause is still being investigated. Lamp control is not yet confirmed
working.

## Upload to the SmartKnob

Run these commands from the `firmware` folder:

```powershell
pio run -e app_gui
pio run -e app_gui -t upload
```

The SmartKnob creates a local Wi-Fi network for the IR board at
`192.168.8.1`. The Light app shows Offline until the IR board connects.

The saved home Wi-Fi settings remain unchanged. Radar and Home Sensors
use the home network. The IR connection does not need a router or internet.

## Upload to the IR board

Open this sketch in Arduino IDE:

`ir-bridge/SmartKnob_IR_Bridge/SmartKnob_IR_Bridge.ino`

This sketch is for the ESP8266 IR board, not the SmartKnob's ESP32-S3.
Use the board and port settings from the previous working IR sketch.

Required versions:

- ESP8266 Arduino core: 3.1.2
- IRremoteESP8266: 2.8.6
- ArduinoJson: 6.21.5

Keep `config_private.h` and `receiver_state.h` in the same folder as
the sketch.

The IR output uses GPIO14. The serial monitor runs at 115200 baud
and prints each sent command.

## Private configuration

Both devices need matching connection settings. These are stored in:

- `firmware/include/ir_link_private/config.h`
- `ir-bridge/SmartKnob_IR_Bridge/config_private.h`

These files are excluded from Git and must not be committed.

After cloning the repository, generate them from the repository root:

```powershell
python tools/generate_ir_pairing.py
```

If the files already exist and match, keep them. To deliberately create
a new pairing, use `--replace` and upload both programs again.

## Controls

| Action | Command |
| --- | --- |
| Turn in the increasing direction | Brighter |
| Turn in the opposite direction | Dimmer |
| Short press in Light | ON/OFF |
| Long press in Light | Return to the app menu |
| Detected double clap | ON/OFF and a short white LED flash |

The Light app has 12 detents per turn. Opening the app sends no command.
After startup or an interrupted connection, the first toggle requests ON.

`ON sent` means the IR board has reported sending the command.
It does not confirm that the lamp received it. The lamp sends no status
back, so the app cannot show its actual brightness or power state.

## Remaining tests

- Find out why the lamp does not respond.
- Test ON/OFF and brightness with the real lamp.
- Test double-clap control and check for false triggers.
- Disconnect and reconnect the IR board. Old commands must not replay.
- Check that the motor, display, Timer and Radar still work smoothly.
- Test local lamp control without the home router.

The home Wi-Fi and the IR hotspot share the SmartKnob's radio.
Connecting to the home network may briefly disconnect the IR board.
Commands during a connection gap are discarded.

The IR hotspot uses `192.168.8.0/24`, so the home network should use
a different subnet. Only one IR board can connect to the hotspot.