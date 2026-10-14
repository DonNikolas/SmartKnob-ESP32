// SmartKnob WLAN/location setup: separate test; motor outputs remain LOW.
// Generated with ChatGPT assistance. No flight data is fetched in this stage.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>
#include <esp_system.h>
#include "network_setup/config.h"

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint32_t CONNECT_TIMEOUT_MS = 30000, PORTAL_TIMEOUT_MS = 600000;
Arduino_ESP32SPI displayBus(18, 42, 3, 8, GFX_NOT_DEFINED, HSPI);
Arduino_GC9A01 display(&displayBus, 10, 0, true);
Adafruit_NeoPixel ring(36, 11, NEO_GRB + NEO_KHZ800);
WebServer server(80);
DNSServer dns;
KnobNetwork::Config saved, pending;
bool haveSaved = false, connecting = false, requested = false, tryingNew = false;
bool portalOn = false, displayReady = false, finished = false, fatal = false;
uint32_t requestAt = 0, connectAt = 0, portalAt = 0, finishedAt = 0;
String apName, apPassword, formToken;
const char* statusText = "Enter Wi-Fi on phone";

String randomHex() {
    char value[17];
    snprintf(value, sizeof(value), "%08lX%08lX", (unsigned long)esp_random(), (unsigned long)esp_random());
    return String(value);
}
void line(const String& text, int y, int size, uint16_t colour = 0xFFFF) {
    display.setTextSize(size); display.setTextColor(colour);
    int width = text.length() * 6 * size;
    display.setCursor(max(4, (240 - width) / 2), y); display.print(text);
}
void draw() {
    if (!displayReady) return;
    display.fillScreen(0x0841);
    line("WI-FI SETUP", 27, 2, 0x07FF);
    if (portalOn) {
        line(apName, 60, 1);
        line("Hotspot password", 81, 1, 0xAD55);
        line(apPassword, 98, 2);
        line("http://192.168.4.1", 130, 1);
        line(statusText, 153, 1);
        if (finished) line("Saved / portal closes soon", 175, 1);
        else line("2.4 GHz Wi-Fi", 175, 1, 0xAD55);
    } else {
        line(finished ? "Setup complete" : "Setup closed", 84, 2);
        line(statusText, 121, 1);
        line("Reset to open setup", 160, 1, 0xAD55);
    }
    line("Motor OFF", 200, 1, 0xAD55);
}
bool loadConfig() {
    Preferences prefs;
    if (!prefs.begin(KnobNetwork::NAMESPACE, true)) return false;
    bool ok = prefs.getBytesLength(KnobNetwork::KEY) == sizeof(saved) &&
        prefs.getBytes(KnobNetwork::KEY, &saved, sizeof(saved)) == sizeof(saved);
    prefs.end();
    return ok && KnobNetwork::valid(saved);
}
bool storeConfig(const KnobNetwork::Config& value) {
    Preferences prefs;
    if (!prefs.begin(KnobNetwork::NAMESPACE, false)) return false;
    bool ok = prefs.putBytes(KnobNetwork::KEY, &value, sizeof(value)) == sizeof(value);
    prefs.end(); return ok;
}
void sendPage(int code, const String& html) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(code, "text/html; charset=utf-8", html);
}
String page() {
    String html;
    html.reserve(3800);
    html = F("<!doctype html><html lang='en'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>SmartKnob setup</title><style>body{font:16px system-ui;background:#10171d;color:#edf4ef;max-width:420px;margin:32px auto;padding:20px}input,button{box-sizing:border-box;width:100%;padding:12px;margin:6px 0 18px;border-radius:8px;border:1px solid #50606a;font:inherit}button{background:#79ddd0;font-weight:bold}small{color:#adbdc7}</style><h1>SmartKnob setup</h1><p>");
    html += statusText;
    html += F("</p><form method='post' action='/save'><input type='hidden' name='token' value='");
    html += formToken;
    html += F("'><label>Wi-Fi name (2.4 GHz)<input name='ssid' maxlength='32' required autocomplete='off'></label><label>Wi-Fi password<input name='password' type='password' maxlength='63' autocomplete='new-password'></label><small>Leave the password blank only for an open network. Existing credentials are not shown.</small><h2>Radar centre</h2><p>Default: Schaffhausen city centre. Use decimal degrees with a dot.</p><label>Latitude<input name='latitude' type='number' step='any' min='-90' max='90' required value='");
    html += String(haveSaved ? saved.latitude : 47.696, 6);
    html += F("'></label><label>Longitude<input name='longitude' type='number' step='any' min='-180' max='180' required value='");
    html += String(haveSaved ? saved.longitude : 8.635, 6);
    html += F("'></label><button>Connect and save</button></form><small>Settings are stored on this SmartKnob after connection succeeds. No flight data is requested yet.</small></html>");
    return html;
}
void handleSave() {
    if (server.arg("token") != formToken) { sendPage(403, "Invalid form. Reload the setup page."); return; }
    if (connecting || requested) { sendPage(409, "Connection test in progress. Wait up to 30 seconds, then reload."); return; }
    String ssid = server.arg("ssid"), password = server.arg("password");
    KnobNetwork::Config candidate;
    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63 ||
        strlen(ssid.c_str()) != ssid.length() || strlen(password.c_str()) != password.length() ||
        !KnobNetwork::coordinate(server.arg("latitude").c_str(), 90, candidate.latitude) ||
        !KnobNetwork::coordinate(server.arg("longitude").c_str(), 180, candidate.longitude)) {
        sendPage(400, "Invalid name or coordinates. Go back and check the fields."); return;
    }
    ssid.toCharArray(candidate.ssid, sizeof(candidate.ssid));
    password.toCharArray(candidate.password, sizeof(candidate.password));
    if (!KnobNetwork::valid(candidate)) { sendPage(400, "Use an 8-63 character Wi-Fi password, or leave blank for an open network."); return; }
    pending = candidate; requested = true; tryingNew = true; finished = false;
    requestAt = millis();
    sendPage(200, "<meta name='viewport' content='width=device-width,initial-scale=1'><h2>Testing connection</h2><p>Watch the SmartKnob display. This can take 30 seconds. If your phone disconnects, reconnect to the SmartKnob hotspot.</p><a href='/'>Return to setup</a>");
}
void closePortal() {
    server.stop(); dns.stop(); WiFi.softAPdisconnect(true);
    if (WiFi.status() == WL_CONNECTED) WiFi.mode(WIFI_STA);
    portalOn = false; draw();
}
void setup() {
    for (uint8_t pin : MOTOR_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    pinMode(9, OUTPUT); digitalWrite(9, LOW);
    Serial.begin(115200);
    ring.begin(); ring.clear(); ring.show();
    displayReady = display.begin(20000000UL); display.setTextWrap(false);
    ledcSetup(0, 5000, 10); ledcAttachPin(9, 0); ledcWrite(0, 818);
    // WLAN driver persistence is disabled; only the explicit validated record is saved.
    WiFi.persistent(false); WiFi.mode(WIFI_AP_STA); WiFi.setAutoReconnect(false);
    apName = "SmartKnob-" + randomHex().substring(0, 4);
    apPassword = randomHex().substring(0, 12); formToken = randomHex();
    if (!displayReady || !WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0)) ||
        !WiFi.softAP(apName.c_str(), apPassword.c_str())) {
        statusText = "Display / hotspot failed"; fatal = true;
        Serial.println("Setup failed. Motor remains OFF."); draw(); return;
    }
    portalOn = true; portalAt = millis();
    dns.start(53, "*", WiFi.softAPIP());
    server.on("/", HTTP_GET, [](){sendPage(200, page());});
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound([](){server.sendHeader("Location", "http://192.168.4.1/", true); server.send(302, "text/plain", "");});
    server.begin(); haveSaved = loadConfig();
    if (haveSaved) { pending = saved; requested = true; tryingNew = false; requestAt = millis(); statusText = "Checking saved Wi-Fi"; }
    draw();
    Serial.println("WLAN setup ready. Credentials are shown on the display only. x closes setup.");
}
void loop() {
    if (fatal) { delay(20); return; }
    if (Serial.available() && Serial.read() == 'x') {
        requested = connecting = false; WiFi.disconnect(false, false);
        statusText = "Stopped by user"; closePortal(); fatal = true; return;
    }
    if (portalOn) { dns.processNextRequest(); server.handleClient(); }
    if (requested && millis() - requestAt >= 500) {
        requested = false; connecting = true; connectAt = millis();
        WiFi.disconnect(false, false);
        WiFi.begin(pending.ssid, pending.password);
        statusText = "Connecting..."; draw();
    }
    if (connecting && millis() - connectAt >= 500 && WiFi.status() == WL_CONNECTED && WiFi.SSID() == String(pending.ssid)) {
        connecting = false;
        if (!tryingNew || storeConfig(pending)) {
            saved = pending; haveSaved = true; finished = true; finishedAt = millis();
            statusText = "Connected / settings saved";
            Serial.println("Wi-Fi connected. Settings available for the future Radar app.");
        } else statusText = "Connected / save failed";
        memset(pending.password, 0, sizeof(pending.password)); draw();
    } else if (connecting && millis() - connectAt >= CONNECT_TIMEOUT_MS) {
        connecting = false; WiFi.disconnect(false, false);
        memset(pending.password, 0, sizeof(pending.password));
        statusText = "Wi-Fi failed / retry"; draw();
    }
    if (finished && WiFi.status() != WL_CONNECTED) { finished = false; statusText = "Wi-Fi connection lost"; draw(); }
    if (portalOn && ((finished && millis() - finishedAt >= 60000) || millis() - portalAt >= PORTAL_TIMEOUT_MS)) {
        if (!finished) statusText = "Setup time expired";
        closePortal();
    }
    delay(2);
}
