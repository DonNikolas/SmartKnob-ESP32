#include "test_platform.h"
#include "Arduino.h"
#include "Preferences.h"
#include "WiFi.h"
#include <stdlib.h>
#include <cassert>
#include "../config.h"

HostSerial Serial;
HostWiFi WiFi;
std::map<std::string, int32_t> hostNvs;
bool hostNvsOpen = true, hostNvsWrite = true, hostPwmAttach = true;
int hostWrites = 0, hostWifiState = WL_DISCONNECTED, hostReconnects = 0, hostNtpStarts = 0;
uint32_t hostDuty = 0;
time_t hostEpoch = 0;

void digitalWrite(int, int) {}
bool ledcAttach(uint8_t pin, uint32_t freq, uint8_t resolution) {
    assert(pin == PIN_BL && freq == BACKLIGHT_PWM_HZ && resolution == BACKLIGHT_PWM_BITS);
    return hostPwmAttach;
}
bool ledcWrite(uint8_t pin, uint32_t duty) {
    assert(pin == PIN_BL && duty <= 1023);
    hostDuty = duty;
    return true;
}
bool Preferences::begin(const char *, bool) { return hostNvsOpen; }
void Preferences::end() {}
int32_t Preferences::getInt(const char *key, int32_t defaultValue) {
    auto found = hostNvs.find(key);
    return found == hostNvs.end() ? defaultValue : found->second;
}
size_t Preferences::putInt(const char *key, int32_t value) {
    if (!hostNvsWrite) return 0;
    hostNvs[key] = value;
    ++hostWrites;
    return sizeof(int32_t);
}
void HostWiFi::begin(const char *, const char *) { ++hostReconnects; }
int HostWiFi::status() { return hostWifiState; }
bool HostWiFi::reconnect() { ++hostReconnects; return true; }
void configTzTime(const char *tz, const char *, const char *, const char *) {
    ++hostNtpStarts;
    setenv("TZ", tz, 1);
    tzset();
}
extern "C" time_t time(time_t *out) noexcept {
    if (out) *out = hostEpoch;
    return hostEpoch;
}
