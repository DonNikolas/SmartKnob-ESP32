// Behaviour-preserving extraction from pressure.h (bidirectional diagnostic v1).
// No Arduino, HX711, motor or display dependencies: all time/sample inputs are explicit.
#include "pressure_logic.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
void pressure_init(PressureState* s) { memset(s,0,sizeof(*s)); s->startAllowed=true; }
void pressure_precheck(PressureState* s,uint32_t now) {
    if(s->zeroing && now-s->zeroAt>6000)pressure_fail(s,"Pressure zero timed out");
}
void pressure_set_start_allowed(PressureState* s, bool allowed) { s->startAllowed = allowed; 
}

bool pressure_is_blocked(const PressureState* s) { return s->blocked; 
}

void pressure_clear_interval(PressureState* s) { s->intervalSamples = 0; s->intervalValid = false; 
}

const char* pressure_diagnostic_state(const PressureState* s, uint32_t now) {
        if (s->fault) return "BI_FAULT";
        if (s->zeroing) return "BI_TARE";
        if (!s->valid) return "BI_NO_ZERO";
        if (s->blank && now - s->blankAt < 150) return "BI_CLICK_BLANK";
        if (s->count != PRESSURE_FILTER_SIZE) return "BI_FILTER_FILL";
        if (s->blocked) return "BI_BOUNDARY_BLOCKED";
        if (s->pressed) {
            if (s->releaseTiming) return "BI_RELEASE_PENDING";
            if (s->longSent) return s->pressDirection > 0 ? "BI_LONG_POS" : "BI_LONG_NEG";
            return s->pressDirection > 0 ? "BI_HELD_POS" : "BI_HELD_NEG";
        }
        if (s->downTiming) return s->candidateSign > 0 ? "BI_PENDING_POS" : "BI_PENDING_NEG";
        return s->armed ? "BI_ARMED" : "BI_WAIT_NEUTRAL";
    
}

void pressure_tare(PressureState* s, uint32_t now) {
        s->blank = false; s->minimumDelta = s->maximumDelta = 0; pressure_clear_interval(s);
        s->valid = false; s->zeroing = true; s->fault = NULL;
        s->zeroCount = 0; s->zeroSum = 0; s->zeroAt = now;
        s->zeroLow = 8388607; s->zeroHigh = -8388608;
        pressure_reset_filter(s); pressure_reset_gesture(s);
    
}

void pressure_after_click(PressureState* s, uint32_t now) { s->blank = true; s->blankAt = now; pressure_reset_filter(s); 
}

void pressure_reset_gesture(PressureState* s) {
        s->pressed = s->armed = s->longSent = s->releaseTiming = s->downTiming = s->blocked = false;
        s->candidateSign = s->pressDirection = 0;
    
}

bool pressure_released_ready(const PressureState* s, uint32_t now) {
        return s->valid && s->count == PRESSURE_FILTER_SIZE && s->armed && !s->pressed &&
            s->rawMagnitude < PRESSURE_RELEASE_COUNTS && s->magnitude < PRESSURE_RELEASE_COUNTS && now - s->lastData < 250;
    
}

// State machine: arm -> confirm down -> held -> release -> rearm.
PressureEvent pressure_sample_gesture(PressureState* s, long filtered, long instant, uint32_t now, bool enabled) {
        if (!enabled) { pressure_reset_gesture(s); return PRESSURE_NONE; }
        const bool neutral = labs(instant) < PRESSURE_RELEASE_COUNTS && labs(filtered) < PRESSURE_RELEASE_COUNTS;
        if (!s->pressed && !s->armed) {
            // Includes blocked presses and rebound after a completed gesture.
            if (neutral) {
                if (!s->releaseTiming) { s->releaseTiming = true; s->releaseAt = now; }
                if (now - s->releaseAt >= PRESSURE_REARM_MS) {
                    pressure_reset_gesture(s); s->armed = true;
                }
            } else s->releaseTiming = false;
            return PRESSURE_NONE;
        }
        if (!s->pressed) {
            const int sign = filtered >= PRESSURE_PRESS_COUNTS && instant >= PRESSURE_PRESS_COUNTS ? 1 :
                filtered <= -PRESSURE_PRESS_COUNTS && instant <= -PRESSURE_PRESS_COUNTS ? -1 : 0;
            if (sign == 0) { s->downTiming = false; s->candidateSign = 0; return PRESSURE_NONE; }
            if (!s->startAllowed) {
                s->blocked = true; s->armed = s->downTiming = s->releaseTiming = false;
                ++s->blockedPresses; return PRESSURE_NONE;
            }
            if (!s->downTiming || sign != s->candidateSign) {
                s->downTiming = true; s->downAt = now; s->candidateSign = sign;
            }
            if (now - s->downAt >= PRESSURE_PRESS_MS) {
                s->pressed = true; s->armed = s->longSent = s->downTiming = s->releaseTiming = false;
                s->pressDirection = sign; s->pressedAt = now; ++s->downEvents;
                return PRESSURE_DOWN;
            }
            return PRESSURE_NONE;
        }
        // Release in the latched direction. An opposite overshoot is release,
        // never a new press or a continuation of a long press.
        if (instant * s->pressDirection < PRESSURE_RELEASE_COUNTS) {
            if (!s->releaseTiming) { s->releaseTiming = true; s->releaseAt = now; }
            if (now - s->releaseAt >= PRESSURE_RELEASE_MS) {
                const bool wasLong = s->longSent; ++s->releases;
                pressure_reset_gesture(s); // Rearming needs a NEW stable neutral interval.
                if (!wasLong) { ++s->shortEvents; return PRESSURE_SHORT; }
            }
            return PRESSURE_NONE;
        }
        s->releaseTiming = false;
        if (!s->longSent && filtered * s->pressDirection >= PRESSURE_RELEASE_COUNTS && now - s->pressedAt >= PRESSURE_LONG_MS) {
            s->longSent = true; ++s->longEvents; return PRESSURE_LONG;
        }
        return PRESSURE_NONE;
    
}

void pressure_reset_filter(PressureState* s) { s->sum = 0; s->count = s->filterIndex = 0; for (unsigned i=0;i<4;++i) s->values[i] = 0; 
}

void pressure_fail(PressureState* s, const char* reason) { s->fault = reason; s->valid = s->zeroing = false; pressure_reset_gesture(s); 
}

// One ADC sample (or no sample). now is poll start; readAt is read completion.
PressureEvent pressure_poll(PressureState* s, uint32_t now, bool ready, long raw, uint32_t readAt, bool gestures) {


        if (!ready) {
            if (now - s->lastData >= PRESSURE_TIMEOUT_MS) pressure_fail(s, "Pressure sensor unavailable");
            return PRESSURE_NONE;
        }
        s->lastData = readAt; s->lastRaw = raw; ++s->totalSamples;
        if (s->valid) {
            s->observedDelta = (raw - s->offset) * PRESSURE_PRESS_SIGN;
            if (!s->intervalValid) { s->intervalMin = s->intervalMax = s->observedDelta; s->intervalValid = true; }
            if (s->observedDelta < s->intervalMin) s->intervalMin = s->observedDelta;
            if (s->observedDelta > s->intervalMax) s->intervalMax = s->observedDelta;
            ++s->intervalSamples;
        }
        if (raw <= -8388608L || raw >= 8388607L) { pressure_fail(s, "Pressure ADC saturated"); return PRESSURE_NONE; }
        if (s->zeroing) {
            s->zeroSum += raw;
            if (raw < s->zeroLow) s->zeroLow = raw;
            if (raw > s->zeroHigh) s->zeroHigh = raw;
            if (++s->zeroCount == 32) {
                if (s->zeroHigh - s->zeroLow > 5000) {
                    pressure_fail(s, "Pressure moved during zero; release and restart"); return PRESSURE_NONE;
                }
                s->offset = (long)(s->zeroSum / 32); s->valid = true; s->zeroing = false;
                pressure_reset_filter(s); pressure_reset_gesture(s);
            }
            return PRESSURE_NONE;
        }
        if (!s->valid) return PRESSURE_NONE;
        if (s->blank && now - s->blankAt < 150) return PRESSURE_NONE;
        s->blank = false;
        s->signedDelta = (raw - s->offset) * PRESSURE_PRESS_SIGN;
        if (s->signedDelta < s->minimumDelta) s->minimumDelta = s->signedDelta;
        if (s->signedDelta > s->maximumDelta) s->maximumDelta = s->signedDelta;
        s->rawMagnitude = labs(s->signedDelta);
        s->sum -= s->values[s->filterIndex]; s->values[s->filterIndex] = raw; s->sum += raw;
        s->filterIndex = (s->filterIndex + 1) % PRESSURE_FILTER_SIZE;
        if (s->count < PRESSURE_FILTER_SIZE) ++s->count;
        s->filteredDelta = ((long)(s->sum / s->count) - s->offset) * PRESSURE_PRESS_SIGN;
        s->magnitude = labs(s->filteredDelta);
        if (s->count != PRESSURE_FILTER_SIZE) return PRESSURE_NONE;
        return pressure_sample_gesture(s, s->filteredDelta, s->signedDelta, now, gestures);
    
}
