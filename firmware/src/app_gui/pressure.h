// Bidirectional pressure diagnostic v1. Overlay for DRIFT v2.
// ADC counts are relative to tare, not calibrated force.
#pragma once
#include <Arduino.h>
#include <HX711.h>
#include <stdlib.h>

namespace KnobPressure {
enum class Event { None, Down, Short, Long };
class Input {
public:
    HX711 adc;
    static constexpr uint32_t TIMEOUT_MS = 2000;
    static constexpr unsigned FILTER_SIZE = 4;
    static constexpr long PRESS_COUNTS = 40000, RELEASE_COUNTS = 10000;
    static constexpr int PRESS_SIGN = 1;
    static constexpr uint32_t PRESS_MS = 50, RELEASE_MS = 60, REARM_MS = 150, LONG_MS = 900;
    bool valid = false, zeroing = false, pressed = false;
    long offset = 0, magnitude = 0, rawMagnitude = 0;
    long signedDelta = 0, filteredDelta = 0, minimumDelta = 0, maximumDelta = 0;
    long lastRaw = 0, observedDelta = 0, intervalMin = 0, intervalMax = 0;
    uint32_t totalSamples = 0, intervalSamples = 0;
    uint32_t motionCancels = 0, downEvents = 0, shortEvents = 0, longEvents = 0;
    uint32_t releases = 0, blockedPresses = 0;
    bool intervalValid = false;
    unsigned count = 0, zeroCount = 0;
    uint32_t lastData = 0;
    const char* fault = nullptr;
    int pressDirection = 0;

    void setStartAllowed(bool allowed) { startAllowed = allowed; }
    bool isBlocked() const { return blocked; }
    void clearInterval() { intervalSamples = 0; intervalValid = false; }
    const char* diagnosticState(uint32_t now) const {
        if (fault) return "BI_FAULT";
        if (zeroing) return "BI_TARE";
        if (!valid) return "BI_NO_ZERO";
        if (blank && now - blankAt < 150) return "BI_CLICK_BLANK";
        if (count != FILTER_SIZE) return "BI_FILTER_FILL";
        if (blocked) return "BI_BOUNDARY_BLOCKED";
        if (pressed) {
            if (releaseTiming) return "BI_RELEASE_PENDING";
            if (longSent) return pressDirection > 0 ? "BI_LONG_POS" : "BI_LONG_NEG";
            return pressDirection > 0 ? "BI_HELD_POS" : "BI_HELD_NEG";
        }
        if (downTiming) return candidateSign > 0 ? "BI_PENDING_POS" : "BI_PENDING_NEG";
        return armed ? "BI_ARMED" : "BI_WAIT_NEUTRAL";
    }
    void begin() { adc.begin(12, 13, 128); lastData = millis(); }
    void tare() {
        blank = false; minimumDelta = maximumDelta = 0; clearInterval();
        valid = false; zeroing = true; fault = nullptr;
        zeroCount = 0; zeroSum = 0; zeroAt = millis();
        zeroLow = 8388607; zeroHigh = -8388608;
        resetFilter(); resetGesture();
    }
    void afterClick() { blank = true; blankAt = millis(); resetFilter(); }
    void resetGesture() {
        pressed = armed = longSent = releaseTiming = downTiming = blocked = false;
        candidateSign = pressDirection = 0;
    }
    bool releasedReady(uint32_t now) const {
        return valid && count == FILTER_SIZE && armed && !pressed &&
            rawMagnitude < RELEASE_COUNTS && magnitude < RELEASE_COUNTS && now - lastData < 250;
    }
    Event poll(bool gestures) {
        const uint32_t now = millis();
        if (zeroing && now - zeroAt > 6000) fail("Pressure zero timed out");
        if (!adc.is_ready()) {
            if (now - lastData >= TIMEOUT_MS) fail("Pressure sensor unavailable");
            return Event::None;
        }
        const long raw = adc.read(); lastData = millis(); lastRaw = raw; ++totalSamples;
        if (valid) {
            observedDelta = (raw - offset) * PRESS_SIGN;
            if (!intervalValid) { intervalMin = intervalMax = observedDelta; intervalValid = true; }
            if (observedDelta < intervalMin) intervalMin = observedDelta;
            if (observedDelta > intervalMax) intervalMax = observedDelta;
            ++intervalSamples;
        }
        if (raw <= -8388608L || raw >= 8388607L) { fail("Pressure ADC saturated"); return Event::None; }
        if (zeroing) {
            zeroSum += raw;
            if (raw < zeroLow) zeroLow = raw;
            if (raw > zeroHigh) zeroHigh = raw;
            if (++zeroCount == 32) {
                if (zeroHigh - zeroLow > 5000) {
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
        signedDelta = (raw - offset) * PRESS_SIGN;
        if (signedDelta < minimumDelta) minimumDelta = signedDelta;
        if (signedDelta > maximumDelta) maximumDelta = signedDelta;
        rawMagnitude = labs(signedDelta);
        sum -= values[filterIndex]; values[filterIndex] = raw; sum += raw;
        filterIndex = (filterIndex + 1) % FILTER_SIZE;
        if (count < FILTER_SIZE) ++count;
        filteredDelta = (static_cast<long>(sum / count) - offset) * PRESS_SIGN;
        magnitude = labs(filteredDelta);
        if (count != FILTER_SIZE) return Event::None;
        return sampleGesture(filteredDelta, signedDelta, now, gestures);
    }
    Event sampleGesture(long filtered, long instant, uint32_t now, bool enabled) {
        if (!enabled) { resetGesture(); return Event::None; }
        const bool neutral = labs(instant) < RELEASE_COUNTS && labs(filtered) < RELEASE_COUNTS;
        if (!pressed && !armed) {
            // Includes blocked presses and rebound after a completed gesture.
            if (neutral) {
                if (!releaseTiming) { releaseTiming = true; releaseAt = now; }
                if (now - releaseAt >= REARM_MS) {
                    resetGesture(); armed = true;
                }
            } else releaseTiming = false;
            return Event::None;
        }
        if (!pressed) {
            const int sign = filtered >= PRESS_COUNTS && instant >= PRESS_COUNTS ? 1 :
                filtered <= -PRESS_COUNTS && instant <= -PRESS_COUNTS ? -1 : 0;
            if (sign == 0) { downTiming = false; candidateSign = 0; return Event::None; }
            if (!startAllowed) {
                blocked = true; armed = downTiming = releaseTiming = false;
                ++blockedPresses; return Event::None;
            }
            if (!downTiming || sign != candidateSign) {
                downTiming = true; downAt = now; candidateSign = sign;
            }
            if (now - downAt >= PRESS_MS) {
                pressed = true; armed = longSent = downTiming = releaseTiming = false;
                pressDirection = sign; pressedAt = now; ++downEvents;
                return Event::Down;
            }
            return Event::None;
        }
        // Release in the latched direction. An opposite overshoot is release,
        // never a new press or a continuation of a long press.
        if (instant * pressDirection < RELEASE_COUNTS) {
            if (!releaseTiming) { releaseTiming = true; releaseAt = now; }
            if (now - releaseAt >= RELEASE_MS) {
                const bool wasLong = longSent; ++releases;
                resetGesture(); // Rearming needs a NEW stable neutral interval.
                if (!wasLong) { ++shortEvents; return Event::Short; }
            }
            return Event::None;
        }
        releaseTiming = false;
        if (!longSent && filtered * pressDirection >= RELEASE_COUNTS && now - pressedAt >= LONG_MS) {
            longSent = true; ++longEvents; return Event::Long;
        }
        return Event::None;
    }
private:
    bool startAllowed = true, blocked = false;
    long values[FILTER_SIZE] = {}, zeroLow = 0, zeroHigh = 0;
    int64_t sum = 0, zeroSum = 0;
    unsigned filterIndex = 0;
    bool armed = false, longSent = false, releaseTiming = false, downTiming = false, blank = false;
    int candidateSign = 0;
    uint32_t zeroAt = 0, releaseAt = 0, downAt = 0, pressedAt = 0, blankAt = 0;
    void resetFilter() { sum = 0; count = filterIndex = 0; for (long& v : values) v = 0; }
    void fail(const char* reason) { fault = reason; valid = zeroing = false; resetGesture(); }
};
}
