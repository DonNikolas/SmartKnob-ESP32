// HX711 acquisition and short/long press gestures. ADC counts are not force units.
// Generated with ChatGPT assistance; test on the assembled pressure mechanism.
#pragma once
#include <Arduino.h>
#include <HX711.h>
#include "model.h"

namespace KnobPressure {
enum class Event { None, Down, Short, Long };
class Input {
public:
    HX711 adc;
    static constexpr uint32_t TIMEOUT_MS = 2000;
    static constexpr unsigned FILTER_SIZE = 8;
    bool valid = false, zeroing = false, pressed = false;
    long offset = 0, magnitude = 0, rawMagnitude = 0;
    unsigned count = 0, zeroCount = 0;
    uint32_t lastData = 0;
    const char* fault = nullptr;
    void begin() { adc.begin(12, 13, 128); lastData = millis(); }
    void tare() {
        valid = false; zeroing = true; fault = nullptr;
        zeroCount = 0; zeroSum = 0; zeroAt = millis();
        zeroLow = 8388607; zeroHigh = -8388608;
        resetFilter(); resetGesture();
    }
    void afterClick() { blank = true; blankAt = millis(); }
    void resetGesture() {
        pressed = armed = longSent = releaseTiming = downTiming = false;
    }
    bool releasedReady(uint32_t now) const {
        return valid && count == FILTER_SIZE && armed && !pressed &&
               rawMagnitude < KnobApp::PRESS_THRESHOLD / 2 && now - lastData < 250;
    }
    Event poll(bool gestures) {
        const uint32_t now = millis();
        if (zeroing && now - zeroAt > 6000) fail("Pressure zero timed out");
        if (!adc.is_ready()) {
            if (now - lastData >= TIMEOUT_MS) fail("Pressure sensor unavailable");
            return Event::None;
        }
        const long raw = adc.read(); lastData = millis();
        if (raw <= -8388608L || raw >= 8388607L) { fail("Pressure ADC saturated"); return Event::None; }
        if (zeroing) {
            zeroSum += raw; if (raw < zeroLow) zeroLow = raw; if (raw > zeroHigh) zeroHigh = raw;
            if (++zeroCount == 32) {
                if (zeroHigh - zeroLow > KnobApp::PRESS_THRESHOLD / 2) {
                    fail("Pressure moved during zero; release and restart"); return Event::None;
                }
                offset = static_cast<long>(zeroSum / 32); valid = true; zeroing = false;
                resetFilter(); resetGesture();
            }
            return Event::None;
        }
        if (!valid) return Event::None;
        if (blank && now - blankAt < 150) return Event::None;
        blank = false;
        rawMagnitude = labs(raw - offset);
        sum -= values[index]; values[index] = raw; sum += raw;
        index = (index + 1) % FILTER_SIZE; if (count < FILTER_SIZE) ++count;
        magnitude = labs(static_cast<long>(sum / count) - offset);
        if (count != FILTER_SIZE) return Event::None;
        return sampleGesture(magnitude, rawMagnitude, now, gestures);
    }
    // Separate raw release detection avoids delaying a short release by 8 samples.
    Event sampleGesture(long filtered, long instant, uint32_t now, bool enabled) {
        const bool released = instant < KnobApp::PRESS_THRESHOLD / 2;
        if (!enabled) { resetGesture(); return Event::None; }
        if (!pressed) {
            if (!armed) {
                if (released) {
                    if (!releaseTiming) { releaseTiming = true; releaseAt = now; }
                    if (now - releaseAt >= 150) { armed = true; releaseTiming = false; }
                } else releaseTiming = false;
                return Event::None;
            }
            if (filtered >= KnobApp::PRESS_THRESHOLD && instant >= KnobApp::PRESS_THRESHOLD) {
                if (!downTiming) { downTiming = true; downAt = now; }
                if (now - downAt >= 50) {
                    pressed = true; armed = false; longSent = false; downTiming = false;
                    pressedAt = now; releaseTiming = false;
                    return Event::Down;
                }
            } else downTiming = false;
            return Event::None;
        }
        if (released) {
            if (!releaseTiming) { releaseTiming = true; releaseAt = now; }
            if (now - releaseAt >= 150) {
                const bool wasLong = longSent;
                resetGesture(); armed = true;
                return wasLong ? Event::None : Event::Short;
            }
            return Event::None; // Do not classify a release as a long press.
        }
        releaseTiming = false;
        if (!longSent && now - pressedAt >= 900) {
            longSent = true; return Event::Long;
        }
        return Event::None;
    }
private:
    long values[FILTER_SIZE] = {}, zeroLow = 0, zeroHigh = 0;
    int64_t sum = 0, zeroSum = 0;
    unsigned index = 0;
    bool armed = false, longSent = false, releaseTiming = false, downTiming = false, blank = false;
    uint32_t zeroAt = 0, releaseAt = 0, downAt = 0, pressedAt = 0, blankAt = 0;
    void resetFilter() { sum = 0; count = index = 0; for (long& v : values) v = 0; }
    void fail(const char* reason) { fault = reason; valid = zeroing = false; resetGesture(); }
};
}
