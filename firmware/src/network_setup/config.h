// Shared provisioning record for the future Radar app. Generated with ChatGPT assistance.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
namespace KnobNetwork {
constexpr uint32_t FORMAT = 0x4B4E0001;
constexpr const char* NAMESPACE = "knob-network";
constexpr const char* KEY = "config";
struct Config {
    uint32_t format = FORMAT;
    char ssid[33] = {};
    char password[65] = {};
    double latitude = 47.696;
    double longitude = 8.635;
};
inline bool coordinate(const char* text, double limit, double& value) {
    if (!text || !*text) return false;
    char* end = nullptr;
    value = strtod(text, &end);
    return end != text && *end == '\0' && isfinite(value) && value >= -limit && value <= limit;
}
inline bool valid(const Config& c) {
    if (c.format != FORMAT || !memchr(c.ssid, 0, sizeof(c.ssid)) ||
        !memchr(c.password, 0, sizeof(c.password)) || !c.ssid[0]) return false;
    size_t length = strlen(c.password);
    if (length != 0 && (length < 8 || length > 63)) return false;
    return isfinite(c.latitude) && isfinite(c.longitude) &&
        fabs(c.latitude) <= 90 && fabs(c.longitude) <= 180;
}
}
