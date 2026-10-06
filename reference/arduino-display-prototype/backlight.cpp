#include "backlight.h"
#include "config.h"
#include <Arduino.h>
#include <esp_arduino_version.h>

#if ESP_ARDUINO_VERSION_MAJOR < 3
#error "Die PWM-Ansteuerung benoetigt Arduino-ESP32 3.x (LEDC-Pin-API)."
#endif

namespace {
constexpr uint32_t kDutyMax = (1UL << BACKLIGHT_PWM_BITS) - 1;
bool ready = false;
uint32_t currentDuty = 0, startDuty = 0, targetDuty = 0, fadeStartedMs = 0;
int targetPercent = DISPLAY_MIN_PERCENT;

void writeDuty(uint32_t duty) {
    if (!ready) return;
    uint32_t output = BACKLIGHT_ACTIVE_HIGH ? duty : kDutyMax - duty;
    if (!ledcWrite(PIN_BL, output)) {
        Serial.println("Backlight-PWM: Schreiben fehlgeschlagen.");
        ready = false;
    }
    currentDuty = duty;
}
}

bool backlight_init() {
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, BACKLIGHT_ACTIVE_HIGH ? LOW : HIGH);
    ready = ledcAttach(PIN_BL, BACKLIGHT_PWM_HZ, BACKLIGHT_PWM_BITS);
    if (!ready) {
        Serial.println("Backlight-PWM: GPIO/Kanal konnte nicht eingerichtet werden.");
        return false;
    }
    currentDuty = startDuty = targetDuty = 0;
    writeDuty(0); // Black only during initialization; UI brightness is always >=10%.
    return ready;
}

void backlight_set_percent(int percent, bool smooth) {
    if (percent < DISPLAY_MIN_PERCENT) percent = DISPLAY_MIN_PERCENT;
    if (percent > DISPLAY_MAX_PERCENT) percent = DISPLAY_MAX_PERCENT;
    backlight_poll(); // Retarget from the actual interpolated PWM level.
    targetPercent = percent;
    uint32_t nextDuty = ((uint32_t)percent * kDutyMax + 50) / 100;
    if (!smooth) {
        startDuty = targetDuty = nextDuty;
        writeDuty(nextDuty);
        return;
    }
    if (nextDuty == targetDuty) return;
    startDuty = currentDuty;
    targetDuty = nextDuty;
    fadeStartedMs = millis();
}

void backlight_poll() {
    if (!ready || currentDuty == targetDuty) return;
    uint32_t elapsed = (uint32_t)(millis() - fadeStartedMs);
    uint32_t duty = targetDuty;
    if (elapsed < BACKLIGHT_FADE_MS) {
        int32_t delta = (int32_t)targetDuty - (int32_t)startDuty;
        duty = (uint32_t)((int32_t)startDuty + delta * (int32_t)elapsed / BACKLIGHT_FADE_MS);
    }
    if (duty != currentDuty) writeDuty(duty);
}

int backlight_target_percent() { return targetPercent; }
