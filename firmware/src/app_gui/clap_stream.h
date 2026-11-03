// Integrated wrapper around the unchanged, hardware-tested FFT v2 rules.
#pragma once
#include "../clap_fft_detector.h"
namespace KnobClap {
enum class State { Warming, Calibrating, Ready, Muted, NoSignal };
class Stream {
    ClapFFT::Detector detector;
    unsigned warmup = 20, calibration = 0, flatFrames = 0;
    double sum = 0;
    float background = 0, previous = 0;
    uint32_t audioMs = 0;
    bool wasMuted = false, noSignal = false;
public:
    State state = State::Warming;
    void reset() {
        detector.reset(); warmup = 20; calibration = flatFrames = 0;
        sum = background = previous = 0; audioMs = 0;
        wasMuted = noSignal = false; state = State::Warming;
    }
    // Reset only timing evidence after a local haptic click; keep the noise floor.
    void cancelPair() { detector.reset(); wasMuted = true; }
    ClapFFT::Detection update(const ClapFFT::Features& f, bool muted) {
        audioMs += 16;
        if (!isfinite(f.rms) || !isfinite(f.peak)) { reset(); return {}; }
        if (f.rms == 0 && f.peak == 0) {
            if (flatFrames < 32) ++flatFrames;
            if (flatFrames >= 32) {
                noSignal = true; state = State::NoSignal; detector.reset(); return {};
            }
        } else {
            flatFrames = 0;
            // Do not classify signal-start transients after a flat input.
            if (noSignal) { reset(); return {}; }
        }
        if (warmup) { --warmup; previous = f.rms; state = State::Warming; return {}; }
        if (muted) {
            cancelPair(); previous = f.rms; state = State::Muted;
            if (calibration < 125) { calibration = 0; sum = 0; }
            return {};
        }
        if (wasMuted) { wasMuted = false; previous = f.rms; state = State::Ready; return {}; }
        if (calibration < 125) {
            sum += f.rms; ++calibration; previous = f.rms;
            if (calibration == 125) background = sum / 125;
            state = calibration == 125 ? State::Ready : State::Calibrating;
            return {};
        }
        state = State::Ready;
        const float oldBackground = background;
        const float rise = f.rms / fmaxf(1, previous);
        // Identical floor adaptation and feature thresholds to clap_fft_test v2.
        if (f.rms < fmaxf(600, 4 * background)) background += 0.01f * (f.rms - background);
        previous = f.rms;
        return detector.update(f, oldBackground, rise, audioMs);
    }
};
// Consume each sequence once, even while offline/stopped/muted. Never replay it.
class ActionGate {
    uint32_t consumed = 0;
public:
    bool take(uint32_t pairs, uint32_t at, uint32_t now, bool allowed, bool muted) {
        if (pairs == consumed) return false;
        const uint32_t delta = pairs - consumed;
        consumed = pairs;
        return delta == 1 && now - at <= 200 && allowed && !muted;
    }
};
}
