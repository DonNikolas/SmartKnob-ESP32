// Background telemetry. Each worker owns its peripheral and latest-value queue.
#pragma once
#include <stdint.h>
namespace KnobBackground {
struct Environment {
    float temperature = 0, humidity = 0, pressure = 0;
    uint32_t measuredAt = 0, sequence = 0;
    bool valid = false;
    const char* status = "Starting sensor";
};
struct Audio {
    uint32_t pairs = 0, lastPairAt = 0;
    uint32_t measuredAt = 0, frames = 0, errors = 0, candidates = 0;
    float rms = 0;
    const char* status = "Starting microphone";
};
void begin();
void read(Environment& environment, Audio& audio);
void suppressClick();
bool clickSuppressed(uint32_t now);
}
