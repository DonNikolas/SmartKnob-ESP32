#include "network_time.h"
#include "wifi_config.h"
#include <Arduino.h>
#include <WiFi.h>

namespace {
bool configured = false;
bool ntpStarted = false;
uint32_t lastAttemptMs = 0;
}

void network_time_init() {
    configured = KNOB_WIFI_SSID[0] != '\0';
    ntpStarted = false;
    if (!configured) {
        Serial.println("WLAN: SSID in wifi_config.h eintragen. UI bleibt bedienbar.");
        return;
    }
    WiFi.persistent(false); // Credentials are provided by the sketch, not rewritten to NVS.
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(KNOB_WIFI_SSID, KNOB_WIFI_PASSWORD);
    lastAttemptMs = millis();
    // No waiting loop, delay(), scan or getLocalTime(timeout) in the UI loop.
}

void network_time_poll() {
    if (!configured) return;
    if (WiFi.status() == WL_CONNECTED) {
        if (!ntpStarted) {
            configTzTime(KNOB_TIMEZONE, KNOB_NTP_SERVER_1, KNOB_NTP_SERVER_2);
            ntpStarted = true; // SNTP subsequently resynchronizes in the background.
        }
    } else if ((uint32_t)(millis() - lastAttemptMs) >= KNOB_WIFI_RETRY_MS) {
        lastAttemptMs = millis();
        WiFi.reconnect(); // Also retry after an initially unavailable AP.
    }
}

bool network_time_configured() { return configured; }
bool network_time_connected() { return configured && WiFi.status() == WL_CONNECTED; }

bool network_time_local(struct tm *result) {
    if (!result) return false;
    time_t now = time(NULL);
    // Jan 2024..Jan 2100, checked in int64 so the literals fit all host tests.
    if ((int64_t)now < 1704067200LL || (int64_t)now >= 4102444800LL) return false;
    return localtime_r(&now, result) != NULL;
}
