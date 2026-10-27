// Independent spring/damper profile. Voltage demand, not measured torque.
// Generated with ChatGPT assistance; assembled hardware validation pending.
#pragma once
#include <math.h>
namespace KnobHaptics {
inline float clamp(float v, float lo, float hi) { return fminf(hi, fmaxf(lo, v)); }
inline float hysteresis(float width) { return fminf(0.0349066f, width * 0.07f); }
inline float demand(float error, float speed, float width) {
    const float dead = fminf(0.0174533f, width * 0.06f);
    const float edge = width * 0.5f;
    const float peak = width < 0.2f ? 0.50f : 1.00f;
    const float gain = fmaxf(3.8f, peak / (edge - dead));
    const float spring = copysignf(fmaxf(0, fabsf(error) - dead), error) * gain;
    const float damping = fabsf(speed) < 0.05f ? 0 : clamp(0.025f * speed, -0.15f, 0.15f);
    return clamp(spring - damping, -1.0f, 1.0f);
}
}
