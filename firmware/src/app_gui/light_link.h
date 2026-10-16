#pragma once
#include "light_protocol.h"
namespace KnobLight {
struct Snapshot {
    bool online=false, powerKnown=false, requestedOn=false, ready=false;
    uint32_t sent=0,dropped=0,pending=0;
    Command lastSent=Command::None;
};
bool begin();
// Nonblocking; safe from motor and GUI tasks. No network or flash work here.
bool request(Action action);
void read(Snapshot& snapshot);
}
