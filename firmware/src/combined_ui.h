// Generated with ChatGPT assistance. Values are copied between cores by queue.
#pragma once
#include <stdint.h>

namespace CombinedUi {
constexpr int POSITIONS = 12;
constexpr int LED_COUNT = 36;

// Physical LED chain runs counterclockwise, viewed from the front.
constexpr int LED_SIX_OCLOCK_INDEX = 0;
constexpr int LED_CCW_INDEX_STEP = 1;
// UI direction only. Never change the motor's SENSOR_SIGN to reverse the UI.
constexpr int RAW_TO_CCW = 1;

inline int wrapIndex(int value, int count) {
    int result = value % count;
    return result < 0 ? result + count : result;
}
inline int ledIndex(int position, int bottom, int direction) {
    return wrapIndex(bottom + direction * wrapIndex(position, POSITIONS) *
                     (LED_COUNT / POSITIONS), LED_COUNT);
}
static_assert(LED_COUNT % POSITIONS == 0, "LED count must fit the detents");

struct State {
    uint8_t position = 0;
    int8_t confirmed = -1;
    uint8_t bottomLed = LED_SIX_OCLOCK_INDEX;
    int8_t ledDirection = LED_CCW_INDEX_STEP;
    uint32_t confirmations = 0;
    bool running = false;
    bool zeroing = false;
    bool pressureReady = false;
    bool centred = false;
    bool alignment = false;
};

// Call begin while the motor is disabled. Only the UI task touches LVGL,
// Arduino_GFX, NeoPixel or backlight PWM. publish never waits for rendering.
bool begin();
void publish(const State& state);
}
