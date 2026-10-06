#include "settings.h"
#include <Arduino.h>
#include <Preferences.h>
#include <limits.h>

namespace {
Preferences preferences;
bool ready = false;
}

bool settings_init() {
    if (ready) preferences.end();
    ready = preferences.begin("smartknob-v1", false);
    if (!ready) Serial.println("NVS konnte nicht geoeffnet werden; verwende Standardwerte.");
    return ready;
}

int settings_load(const char *key, int defaultValue, int minValue, int maxValue, int step) {
    if (!key || minValue > maxValue || step <= 0) return defaultValue;
    int value = ready ? preferences.getInt(key, defaultValue) : defaultValue;
    // Clamp before doing step arithmetic. Invalid/old stored values must not
    // disable the backlight or leave a control outside its new range.
    if (value < minValue) value = minValue;
    if (value > maxValue) value = maxValue;
    return minValue + ((value - minValue) / step) * step;
}

bool settings_save(const char *key, int value) {
    if (!ready || !key) return false;
    if (preferences.getInt(key, INT_MIN) == value) return true;
    return preferences.putInt(key, value) == sizeof(int32_t);
}
