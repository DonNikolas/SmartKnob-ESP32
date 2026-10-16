# IR light integration

Status: both firmware builds, uploads and the wireless connection verified.
Actual lamp response remains untested.

## Architecture

The motor task only enqueues a tiny action with a zero-wait FreeRTOS send. A
separate core-0 task owns HTTP, the command protocol and its bounded queue. The
GUI observes a copied snapshot and submits fresh double-clap actions. Radar and
the IR AP use AP+STA; the existing provisioning and preference stores are not
rewritten. Motor pins, gains, voltage limits and timing guards are unchanged.

The Light screen is appended to the enum, preserving existing screen IDs.
Preferences retain their previous schema. The old Light percentage is unused;
relative IR commands cannot establish absolute lamp brightness. Timer LED
orientation retains the verified twelve-o'clock origin.

## Delivery semantics

- Authenticated polling on `/ir/poll`, random private AP credentials and API key.
- Random server session and receiver boot IDs; monotonic nonzero command ID.
- Eight commands maximum; 1.2-second command lifetime. Offline requests are
  discarded. Overflow rejects the new request. Expiry clears the remaining
  sequence rather than executing delayed relative commands.
- A separate 16-element FreeRTOS ingress queue absorbs brief cross-core bursts;
  requests older than 300 ms there are discarded before entering the protocol.
- Receiver polls every 140 ms, sends a full NEC32 frame once, and acknowledges
  that ID on the next poll. A lost ACK returns the same ID; the receiver suppresses
  re-transmission. Invalid responses, unknown commands and late replies are ignored.
- On a receiver reboot, queued commands are cleared. On a server reboot, the
  receiver performs a new session handshake. No commands are saved in flash.
- This is not guaranteed exactly-once physical lamp actuation. Power loss between
  IR transmission and acknowledgment leaves the outcome uncertain; it is not
  automatically retried after a reboot. The lamp has no return channel.
- Power is a requested state only. After an uncertain queue/disconnection, the
  next toggle explicitly requests ON. Another remote can desynchronize it.
- Double-clap classification retains its existing limitations. A fresh detected
  pair requests one toggle, except during Boot, Fault or Stopped screens.

## Existing verified codes

These come from the user's working `Receive-IR.ino`, not guessed remote codes.

| Command | NEC32 |
| --- | --- |
| ON | 0x00F7C03F |
| OFF | 0x00F740BF |
| Brighter | 0x00F700FF |
| Dimmer | 0x00F7807F |

GPIO14 and `sendNEC(code, 32)` retain the original transmitter behavior.

## Host checks

From repository root, using a C++11 compiler:

```sh
g++ -std=c++11 firmware/docs/tests/ir-light-host.cpp -o ir-light-host
./ir-light-host
g++ -std=c++11 firmware/docs/tests/ir-receiver-host.cpp -o ir-receiver-host
./ir-receiver-host
g++ -std=c++11 firmware/docs/tests/timer-settings-host.cpp -o timer-settings-host
./timer-settings-host
```

These cover queue order/bounds, lost-ACK retries, receiver deduplication, reboot,
expiry, clock rollover, Light interaction and unchanged timer/preferences behavior.
Additional development-host checks used the actual controller with mocked motor,
HX711 and flash timing, plus real LVGL 8.3.11 rendering of all screens. Light
short/long press and turn dispatch passed. LVGL retained 25,808 bytes of its
64 KiB heap after the rendering checks. These checks do not emulate Wi-Fi timing.

Hardware checklist and installation instructions: `README-IR-SETUP.md` at the
repository root. Record actual results here after testing. In particular check
motor deadline faults under concurrent HTTPS Radar requests and IR polling;
do not relax the existing timing guards to conceal a scheduling problem.

## API references checked

- Arduino-ESP32 2.0.17 `WebServer.h` and `WiFiAP.h`:
  https://github.com/espressif/arduino-esp32/tree/2.0.17/libraries
- ESP8266 Arduino 3.1.2 HTTPClient API:
  https://github.com/esp8266/Arduino/tree/3.1.2/libraries/ESP8266HTTPClient
- IRremoteESP8266 2.8.6 `IRsend.h`:
  https://github.com/crankyoldgit/IRremoteESP8266/blob/v2.8.6/src/IRsend.h

## Hardware test — 2026-10-16

Both firmware programs built and uploaded successfully.
The SmartKnob and IR bridge connected successfully.

The demo shows the SmartKnob and IR transmitter without a lamp.
Actual lamp response, including power and brightness control,
has not yet been tested.

[Demo video](../videos/ir-light-control-demo.mp4)

  
