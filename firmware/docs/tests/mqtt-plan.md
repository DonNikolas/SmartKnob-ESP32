# MQTT preparation

Send the local BME280 temperature and show one received numeric value on page
6 of Home Sensors. Existing five HTTPS/Boxtec pages remain separate from MQTT.

Defaults are disabled and contain no broker address or credentials. Private
configuration belongs in `include/mqtt_private/config.h`, ignored by Git.
The checked installer preserves existing source outside its integration edits.

The new worker owns its network client on core 0. The BME worker sends samples
through a one-element queue; the GUI reads a copied MQTT snapshot. No motor,
Wi-Fi mode, IR-command or Preferences operations occur in MQTT callbacks.

Temperature: numeric Celsius, QoS 0, non-retained, ten-second interval, valid
readings only, maximum local sample age six seconds. Subscriber receipt is not
acknowledged by this firmware. Status uses retained online and an offline will.
Input: exact topic, finite decimal number, display only, maximum 48 payload
bytes, five-minute receive timeout. Network disconnect and invalid payloads
invalidate the displayed value. Retained input age at the publisher is unknown.

Tests completed during preparation:

- Numeric parser acceptance/rejection and timestamp rollover tests.
- Checked installer on available project sources, then repeated application.
- MQTT worker C++ syntax check with simplified API stubs.

Host parser test, from firmware:

```sh
g++ -std=c++17 -Wall -Wextra -Werror docs/tests/mqtt-value-host.cpp -o mqtt-value-host
./mqtt-value-host
```

Still pending:

- Actual ESP32 compilation and page 6 navigation.
- Broker authentication, subscription, publish and reconnect.
- TLS certificate/clock verification if TLS is required.
- Valid/invalid incoming data and receive timeout on the display.
- UI, pressure, haptics, IR and other network apps under connection failures.
- Confirm the current model wraps page navigation using KnobHomeFeeds::COUNT.

Reference: PubSubClient 2.8 API, https://pubsubclient.knolleary.net/api.html
Connect/subscribe/publish, last will, socket timeout and buffer size APIs.
Separate tasks prevent direct network waits in the motor loop; they do not
prove absence of shared CPU, memory or radio interference.
