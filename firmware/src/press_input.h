// Adapted with ChatGPT assistance from the tested hx711_press.cpp.
// ADC counts are not calibrated force units.
#pragma once
#include <HX711.h>

namespace PressInput {
constexpr unsigned FILTER_SIZE = 8;
constexpr uint32_t PRESS_MS = 50, RELEASE_MS = 150, TIMEOUT_MS = 2000;
HX711 adc;
long threshold = 15000, offset = 0, magnitude = 0;
long values[FILTER_SIZE] = {};
int64_t sum = 0, zeroSum = 0;
unsigned index = 0, count = 0, zeroCount = 0;
bool valid = false, collecting = false, armed = false;
bool pressTiming = false, releaseTiming = false;
uint32_t pressAt = 0, releaseAt = 0, zeroAt = 0, lastData = 0;
bool blanking = false;
uint32_t blankAt = 0;
const char* fault = nullptr;

void resetDetection() { armed = pressTiming = releaseTiming = false; }
void resetFilter() {
    sum = 0; count = index = 0;
    for (unsigned i = 0; i < FILTER_SIZE; ++i) values[i] = 0;
}
void invalidate(const char* reason) {
    valid = collecting = false; fault = reason;
    resetDetection(); resetFilter();
}
void begin() {
    adc.begin(12, 13, 128);
    lastData = millis();
}
void tare() {
    valid = false; collecting = true; fault = nullptr;
    zeroSum = 0; zeroCount = 0; zeroAt = millis(); blanking = false;
    resetDetection(); resetFilter();
    Serial.println("ZERO: Keep the PCB unloaded and still; collecting 32 samples.");
}
bool readyToStart() {
    return valid && !collecting && count == FILTER_SIZE &&
           millis() - lastData < 250;
}
void changeThreshold(char key) {
    threshold += key == '+' ? 1000 : -1000;
    threshold = constrain(threshold, 1000L, 1000000L);
    resetDetection();
    Serial.printf("Threshold: %ld | Release below: %ld\n", threshold, threshold / 2);
}
// Called only on new ADC readings; holding cannot generate repeated events.
bool detect(long value, uint32_t now) {
    if (!armed) {
        if (value < threshold / 2) {
            if (!releaseTiming) { releaseTiming = true; releaseAt = now; }
            if (now - releaseAt >= RELEASE_MS) { armed = true; releaseTiming = false; }
        } else releaseTiming = false;
        return false;
    }
    if (value >= threshold) {
        if (!pressTiming) { pressTiming = true; pressAt = now; }
        if (now - pressAt >= PRESS_MS) { resetDetection(); return true; }
    } else pressTiming = false;
    return false;
}
// Suppress motor-vibration samples, then require a fresh release confirmation.
void afterClick() {
    blanking = true; blankAt = millis();
    resetDetection(); resetFilter();
}
bool poll(bool allowPress) {
    const uint32_t now = millis();
    if (collecting && now - zeroAt > 6000) invalidate("HX711 zero timeout");
    if (!adc.is_ready()) {
        if (now - lastData >= TIMEOUT_MS) invalidate("HX711 data timeout");
        return false;
    }
    // One read only after DOUT is ready: no wait for the next conversion.
    const long raw = adc.read();
    lastData = millis();
    if (raw <= -8388608L || raw >= 8388607L) {
        invalidate("HX711 ADC saturation"); return false;
    }
    if (collecting) {
        zeroSum += raw;
        if (++zeroCount >= 32) {
            offset = static_cast<long>(zeroSum / zeroCount);
            collecting = false; valid = true; fault = nullptr;
            resetFilter(); resetDetection();
            Serial.printf("ZERO SET: %ld counts. Waiting for filter samples.\n", offset);
        }
        return false;
    }
    if (blanking) {
        if (now - blankAt < 500) return false;
        blanking = false; resetFilter();
    }
    sum -= values[index]; values[index] = raw; sum += raw;
    index = (index + 1) % FILTER_SIZE;
    if (count < FILTER_SIZE) ++count;
    const long delta = valid ? static_cast<long>(sum / count) - offset : 0;
    magnitude = delta < 0 ? -delta : delta;
    if (!allowPress) { resetDetection(); return false; }
    return valid && count == FILTER_SIZE && detect(magnitude, now);
}
} // namespace PressInput
