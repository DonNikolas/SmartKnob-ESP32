// Generated with ChatGPT assistance. Network snapshots never own motor state.
#pragma once
#include <stdint.h>
namespace KnobRadar {
constexpr int MAX_AIRCRAFT = 24;
constexpr int RANGES[4] = {5, 10, 15, 25};
struct Aircraft {
    char hex[9] = {}, callsign[17] = {}, type[9] = {};
    float east = 0, north = 0, distance = 0, track = 0;
    float altitude = 0, speed = 0;
    bool hasAltitude = false, hasSpeed = false, hasTrack = false;
};
struct Snapshot {
    Aircraft aircraft[MAX_AIRCRAFT];
    unsigned count = 0;
    uint32_t sequence = 0, receivedAt = 0;
    double sourceSeconds = 0;
    bool valid = false;
    char status[48] = "Starting network";
};
void begin();
void active(bool enabled);
void read(Snapshot& result);
}
