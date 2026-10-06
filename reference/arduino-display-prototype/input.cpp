#include "input.h"
#include "config.h"
#include <Arduino.h>

namespace {
struct Button {
    int pin;
    bool rawDown, down, longSent, armed;
    uint32_t changedAt, pressedAt, repeatedAt;
};
Button buttons[3];
InputEvent queue[8];
uint8_t head = 0, tail = 0;
bool activity = false;

void enqueue(InputEvent event) {
    uint8_t next = (head + 1) % 8;
    if (next != tail) { queue[head] = event; head = next; }
}

void sample(Button &b, int index, uint32_t now) {
    bool raw = digitalRead(b.pin) == LOW;
    if (raw != b.rawDown) { b.rawDown = raw; b.changedAt = now; }
    if (raw != b.down && (uint32_t)(now - b.changedAt) >= DEBOUNCE_MS) {
        b.down = raw;
        activity = true;
        if (!b.armed) {
            if (!b.down) b.armed = true;
            return;
        }
        if (b.down) {
            b.pressedAt = b.repeatedAt = now;
            b.longSent = false;
            if (index < 2) enqueue(index == 0 ? EVT_LEFT : EVT_RIGHT);
        } else if (index == 2 && !b.longSent) {
            // Include the release edge timestamp; release debounce must not
            // turn a short click just below 600ms into a long click.
            enqueue((uint32_t)(b.changedAt - b.pressedAt) >= LONGPRESS_MS
                    ? EVT_PRESS_LONG : EVT_PRESS_SHORT);
        }
    }
    if (!b.down || !b.armed) return;
    activity = true; // A held button also prevents idle standby.
    if (index == 2 && b.rawDown && !b.longSent && (uint32_t)(now - b.pressedAt) >= LONGPRESS_MS) {
        b.longSent = true;
        enqueue(EVT_PRESS_LONG); // Fires while held; no extra short click on release.
    }
#if INPUT_REPEAT_ENABLED
    if (index < 2 && b.rawDown && (uint32_t)(now - b.pressedAt) >= INPUT_REPEAT_DELAY_MS &&
        (uint32_t)(now - b.repeatedAt) >= INPUT_REPEAT_MS) {
        b.repeatedAt = now;
        enqueue(index == 0 ? EVT_LEFT : EVT_RIGHT);
    }
#endif
}
} // namespace

void input_init() {
    const int pins[] = {PIN_ROT_LEFT, PIN_ROT_RIGHT, PIN_BTN_PRESS};
    uint32_t now = millis();
    head = tail = 0;
    activity = false;
    for (int i = 0; i < 3; ++i) {
        pinMode(pins[i], INPUT_PULLUP);
        // Ignore keys already held at startup until their first release.
        bool down = digitalRead(pins[i]) == LOW;
        buttons[i] = {pins[i], down, down, down, !down, now, now, now};
    }
}

InputEvent input_poll() {
    uint32_t now = millis();
    for (int i = 0; i < 3; ++i) sample(buttons[i], i, now);
    if (head == tail) return EVT_NONE;
    InputEvent event = queue[tail];
    tail = (tail + 1) % 8;
    return event;
}

bool input_any_activity() {
    bool result = activity;
    activity = false;
    return result;
}
