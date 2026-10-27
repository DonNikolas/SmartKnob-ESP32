#pragma once
#include <stdint.h>
#include <stdlib.h>

// Standalone diagnostic, not a replacement for the application's input class.
namespace PressureDiagnostic {
enum Event : unsigned { NONE=0, ARMED=1, DOWN=2, RELEASE=4, SHORT=8, LONG=16 };
struct Button {
    long press=20000, release=10000;
    bool armed=false, held=false, longSent=false, timing=false;
    uint32_t since=0, downAt=0;
    void reset() { armed=held=longSent=timing=false; }
    unsigned sample(long filtered, long instant, long releaseSignal, uint32_t now) {
        const bool low=labs(releaseSignal)<release;
        if (!armed && !held) {
            if (!low) { timing=false; return NONE; }
            if (!timing) { timing=true; since=now; }
            if (now-since<60) return NONE;
            timing=false; armed=true; return ARMED;
        }
        if (armed && !held) {
            if (filtered<press || instant<press) { timing=false; return NONE; }
            if (!timing) { timing=true; since=now; }
            if (now-since<50) return NONE;
            held=true; armed=false; timing=false; downAt=now; longSent=false;
            return DOWN;
        }
        if (low) {
            if (!timing) { timing=true; since=now; }
            if (now-since>=60) {
                const unsigned result=RELEASE | (longSent ? 0u : unsigned(SHORT));
                reset(); armed=true; return result;
            }
            // Do not turn a pending release into a long press.
        } else {
            timing=false;
            if (!longSent && now-downAt>=900) { longSent=true; return LONG; }
        }
        return NONE;
    }
};
}
