// Local app state. Generated with ChatGPT assistance from the Arduino prototype.
// Owned by the motor task; the renderer receives immutable copies.
#pragma once
#include <stdint.h>
#include "background.h"
#include "home_feeds.h"

namespace KnobApp {
constexpr int APP_COUNT = 9;
constexpr int SENSOR_APP = 5, RADAR_APP = 6, MORE_APP = 7, HOME_FEEDS_APP = 8;
constexpr int LED_COUNT = 36;
constexpr int LED_BOTTOM = 0;
constexpr int LED_CCW_STEP = 1;
constexpr int RAW_TO_CCW = 1;
constexpr int PRESS_THRESHOLD = 15000;
enum class Screen : uint8_t { Boot, Home, Value, Menu, Status, Standby, Stopped, Fault, Sensors, Radar, Timer, Light, HomeFeeds };
enum class Notice : uint8_t { Applied, Info, Demo };
struct App {
    const char* name;
    const char* caption;
    const char* unit;
    uint32_t colour;
    int low, high, step, initial, divisor;
};
static constexpr App APPS[APP_COUNT] = {
    {"Timer", "DURATION", "min", 0xFF1800, 1, 60, 1, 15, 1},
    {"Music", "VOLUME", "%", 0x00FF20, 0, 100, 1, 75, 1},
    {"Light", "BRIGHTNESS", "", 0xFFD000, 0, 100, 1, 60, 1},
    {"Temperature", "SETPOINT", "°C", 0x0060FF, 160, 300, 5, 220, 10},
    {"System", "DISPLAY", "%", 0xC000FF, 10, 100, 1, 80, 1},
    {"Sensors", "LIVE READINGS", "", 0x00FFD0, 0, 0, 0, 0, 1},
    {"Radar", "AIRCRAFT", "", 0xFF8000, 0, 0, 0, 0, 1},
    {"More", "OPTIONS", "", 0xFFFFFF, 0, 0, 0, 0, 1},
    {"Home Sensors", "BOXTEC", "", 0xFF4080, 0, 0, 0, 0, 1}
};
inline int wrap(int v, int n) { int r = v % n; return r < 0 ? r + n : r; }
inline int ledIndex(int offset) { return wrap(LED_BOTTOM + LED_CCW_STEP * offset, LED_COUNT); }
struct State {
    Screen screen = Screen::Boot;
    Notice notice = Notice::Applied;
    int selected = 0, menu = 0, draft = 15;
    int radarRange = 3, radarAircraft = 0;
    int homeFeed = 0;
    bool radarDetails = false;
    int values[5] = {15, 75, 60, 220, 80};
    uint32_t revision = 0, activityAt = 0, noticeAt = 0;
    // Fractional displacement from the selected Home detent; visual use only.
    float homeOffset = 0;
    KnobBackground::Environment environment;
    KnobBackground::Audio audio;
    uint32_t timerRemaining = 0, timerTotal = 0, timerUpdatedAt = 0, timerDoneAt = 0;
    bool timerRunning = false, timerDone = false;
    bool settingsPending = false, settingsFailed = false;
    uint32_t clicks = 0;
    unsigned zeroSamples = 0;
    bool zeroing = false;
    bool motorOn = false;
    bool weakField = false;
    char reason[80] = {};
};
class Model {
public:
    State state;
    Screen returnTo = Screen::Home;
    void activate(uint32_t now) { state.screen = Screen::Home; touch(now); }
    void touch(uint32_t now) { state.activityAt = now; ++state.revision; }
    bool interactive() const {
        return state.screen == Screen::Home || state.screen == Screen::Value ||
               state.screen == Screen::Menu || state.screen == Screen::Status || state.screen == Screen::Sensors || state.screen == Screen::Radar || state.screen == Screen::Timer || state.screen == Screen::Light || state.screen == Screen::HomeFeeds;
    }
    int brightness() const {
        if (state.screen == Screen::Standby) return 10;
        return state.screen == Screen::Value && state.selected == 4 ? state.draft : state.values[4];
    }
    // 50 one-percent steps per turn: two full turns cover 0 to 100.
    int slotsPerTurn() const {
        if (state.screen == Screen::Home) return 8;
        if (state.screen == Screen::Value) return 50;
        return 12;
    }
    bool canTurn(int dir) const {
        if (state.screen != Screen::Value) return interactive();
        const App& a = APPS[state.selected];
        return dir > 0 ? state.draft < a.high : state.draft > a.low;
    }
    void turn(int dir, uint32_t now) {
        if (state.screen == Screen::Standby) { activate(now); return; }
        if (!interactive()) return;
        if (state.screen == Screen::Timer) {
            if (!state.timerRunning) {
                state.timerDone = false; state.timerRemaining = 0;
                state.draft = state.values[0]; state.screen = Screen::Value;
            }
            touch(now); return;
        }
        if (state.screen == Screen::HomeFeeds) { state.homeFeed=wrap(state.homeFeed+dir,KnobHomeFeeds::COUNT);touch(now);return; }
        if (state.screen == Screen::Radar) {
            if (state.radarDetails) state.radarAircraft += dir;
            else state.radarRange = wrap(state.radarRange + dir, 4);
            touch(now); return;
        }
        if (state.screen == Screen::Sensors || state.screen == Screen::Light) { touch(now); return; }
        if (state.screen == Screen::Home) state.selected = wrap(state.selected + dir, APP_COUNT);
        else if (state.screen == Screen::Menu) state.menu = wrap(state.menu + dir, 4);
        else if (state.screen == Screen::Value) {
            const App& a = APPS[state.selected];
            int next = state.draft + dir * a.step;
            state.draft = next < a.low ? a.low : next > a.high ? a.high : next;
            // All editable app values are live preferences; no confirmation needed.
            // The timer preset is saved here, but only a press starts its countdown.
            state.values[state.selected] = state.draft;
        } else state.screen = returnTo; // Consume dismissal; do not change a value.
        touch(now);
    }
    void shortPress(uint32_t now) {
        if (state.screen == Screen::Home) {
            if (state.selected == 0 && (state.timerRemaining || state.timerDone)) state.screen = Screen::Timer;
            else if (state.selected == RADAR_APP) { state.screen = Screen::Radar; state.radarDetails = false; state.radarAircraft = 0; }
            else if (state.selected == HOME_FEEDS_APP) state.screen = Screen::HomeFeeds;
            else if (state.selected == 2) state.screen = Screen::Light;
            else if (state.selected == SENSOR_APP) state.screen = Screen::Sensors;
            else if (state.selected == MORE_APP) { state.menu = 0; state.screen = Screen::Menu; }
            else { state.draft = state.values[state.selected]; state.screen = Screen::Value; }
        } else if (state.screen == Screen::HomeFeeds) { state.screen=Screen::Home;
        } else if (state.screen == Screen::Light) { touch(now); return;
        } else if (state.screen == Screen::Timer) {
            updateTimer(now);
            if (state.timerDone) {
                state.timerDone = false; state.draft = state.values[0]; state.screen = Screen::Value;
            } else { state.timerRunning = !state.timerRunning; state.timerUpdatedAt = now; }
        } else if (state.screen == Screen::Radar) { state.radarDetails = !state.radarDetails; state.radarAircraft = 0;
        } else if (state.screen == Screen::Value) {
            state.values[state.selected] = state.draft;
            if (state.selected == 0) {
                state.timerRemaining = state.timerTotal = uint32_t(state.draft) * 60000U;
                state.timerUpdatedAt = now; state.timerRunning = true; state.timerDone = false;
                state.screen = Screen::Timer;
            } else show(Notice::Applied, Screen::Home, now);
        } else if (state.screen == Screen::Menu) {
            if (state.menu == 0) show(Notice::Info, Screen::Menu, now);
            else if (state.menu == 1) show(Notice::Demo, Screen::Menu, now);
            else if (state.menu == 2) state.screen = Screen::Standby;
            else state.screen = Screen::Home;
        } else if (state.screen == Screen::Status) state.screen = returnTo;
        else if (state.screen == Screen::Standby || state.screen == Screen::Sensors) state.screen = Screen::Home;
        else return;
        touch(now);
    }
    void longPress(uint32_t now) {
        if (state.screen == Screen::Home) state.screen = Screen::Standby;
        else if (state.screen == Screen::Value || state.screen == Screen::Menu || state.screen == Screen::Radar || state.screen == Screen::Timer || state.screen == Screen::Light || state.screen == Screen::HomeFeeds) state.screen = Screen::Home;
        else if (state.screen == Screen::Status) state.screen = returnTo;
        else if (state.screen == Screen::Standby || state.screen == Screen::Sensors) state.screen = Screen::Home;
        else return;
        touch(now);
    }
    void updateTimer(uint32_t now) {
        if (!state.timerRunning) return;
        const uint32_t elapsed = now - state.timerUpdatedAt;
        state.timerUpdatedAt = now;
        if (elapsed >= state.timerRemaining) {
            state.timerRemaining = 0; state.timerRunning = false;
            state.timerDone = true; state.timerDoneAt = now; ++state.revision;
        } else state.timerRemaining -= elapsed;
    }
    void tick(uint32_t now) {
        updateTimer(now);
        if (state.screen == Screen::Status && now - state.noticeAt >=
            (state.notice == Notice::Applied ? 650U : 2200U)) {
            state.screen = returnTo; touch(now);
        }
        if (state.screen == Screen::Home && now - state.activityAt >= 60000U) {
            state.screen = Screen::Standby; touch(now);
        }
    }
private:
    void show(Notice message, Screen back, uint32_t now) {
        state.notice = message; state.noticeAt = now;
        returnTo = back; state.screen = Screen::Status;
    }
};
}
