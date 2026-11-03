// Pure C pressure state and API. ADC units are counts, not calibrated force.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PRESSURE_TIMEOUT_MS 2000
#define PRESSURE_FILTER_SIZE 4
#define PRESSURE_PRESS_COUNTS 40000
#define PRESSURE_RELEASE_COUNTS 10000
#define PRESSURE_PRESS_SIGN 1
#define PRESSURE_PRESS_MS 50
#define PRESSURE_RELEASE_MS 60
#define PRESSURE_REARM_MS 150
#define PRESSURE_LONG_MS 900
typedef enum { PRESSURE_NONE, PRESSURE_DOWN, PRESSURE_SHORT, PRESSURE_LONG } PressureEvent;
typedef struct {
bool valid, zeroing, pressed;
long offset, magnitude, rawMagnitude;
long signedDelta, filteredDelta, minimumDelta, maximumDelta;
long lastRaw, observedDelta, intervalMin, intervalMax;
uint32_t totalSamples, intervalSamples;
uint32_t motionCancels, downEvents, shortEvents, longEvents;
uint32_t releases, blockedPresses;
bool intervalValid;
unsigned count, zeroCount;
uint32_t lastData;
const char* fault;
int pressDirection;
bool startAllowed, blocked;
long values[4], zeroLow, zeroHigh;
int64_t sum, zeroSum;
unsigned filterIndex;
bool armed, longSent, releaseTiming, downTiming, blank;
int candidateSign;
uint32_t zeroAt, releaseAt, downAt, pressedAt, blankAt;
} PressureState;
void pressure_set_start_allowed(PressureState* s, bool allowed);
bool pressure_is_blocked(const PressureState* s);
void pressure_clear_interval(PressureState* s);
const char* pressure_diagnostic_state(const PressureState* s, uint32_t now);
void pressure_tare(PressureState* s, uint32_t now);
void pressure_after_click(PressureState* s, uint32_t now);
void pressure_reset_gesture(PressureState* s);
bool pressure_released_ready(const PressureState* s, uint32_t now);
PressureEvent pressure_sample_gesture(PressureState* s, long filtered, long instant, uint32_t now, bool enabled);
void pressure_reset_filter(PressureState* s);
void pressure_fail(PressureState* s, const char* reason);
void pressure_init(PressureState* s);
void pressure_precheck(PressureState* s, uint32_t now);
PressureEvent pressure_poll(PressureState* s, uint32_t now, bool ready, long raw, uint32_t readAt, bool gestures);
#ifdef __cplusplus
}
#endif
