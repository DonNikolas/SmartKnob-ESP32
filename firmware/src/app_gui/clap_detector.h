// Adapted with ChatGPT assistance from the tested clap_detection.cpp.
// Digital RMS thresholds and timing are retained; no serial or LED ownership.
#pragma once
#include <stdint.h>
#include <math.h>
namespace KnobAudio {
class Detector {
    enum Phase { Calibrating, Ready, Candidate, Recovery } phase = Calibrating;
    unsigned frames = 0, lowFrames = 0;
    double sum = 0;
    float background = 0, previous = 0, peak = 0;
    uint32_t onset = 0, recovery = 0, first = 0, lockedAt = 0;
    bool waiting = false, locked = false;
    bool clap(uint32_t now) {
        if (locked) return false;
        if (!waiting) { first = onset; waiting = true; return false; }
        uint32_t gap = onset - first;
        if (gap < 120) { waiting = false; return false; }
        if (gap > 550) { first = onset; return false; }
        waiting = false; locked = true; lockedAt = now; return true;
    }
public:
    bool ready() const { return phase != Calibrating; }
    void cancel(uint32_t now) {
        waiting = false; lowFrames = 0; previous = 0;
        if (ready()) { phase = Recovery; recovery = now; }
        else { frames = 0; sum = 0; }
    }
    bool update(float level, uint32_t now) {
        if (locked && now - lockedAt >= 1000) locked = false;
        if (waiting && now - first > 710) waiting = false;
        if (phase == Calibrating) {
            sum += level;
            if (++frames >= 125) { background = sum / frames; phase = Ready; }
            previous = level; return false;
        }
        bool detected = false;
        float trigger = fmaxf(600, background * 4);
        if (phase == Ready) {
            if (level >= trigger && level >= fmaxf(previous, 1) * 1.8f) {
                onset = now; peak = level; lowFrames = 0; phase = Candidate;
            } else if (level < trigger) background += 0.01f * (level - background);
        } else if (phase == Candidate) {
            peak = fmaxf(peak, level);
            if (level <= fmaxf(background * 1.8f, peak * 0.35f)) ++lowFrames;
            else lowFrames = 0;
            if (now - onset > 160) {
                waiting = false; phase = Recovery; recovery = now; lowFrames = 0;
            } else if (lowFrames >= 2) {
                detected = clap(now); phase = Recovery; recovery = now; lowFrames = 0;
            }
        } else if (phase == Recovery) {
            if (level < fmaxf(300, background * 1.8f)) { if (lowFrames < 3) ++lowFrames; }
            else lowFrames = 0;
            if (now - recovery >= 60 && lowFrames >= 3) phase = Ready;
        }
        previous = level; return detected;
    }
};
}
