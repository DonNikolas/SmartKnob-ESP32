#pragma once
#include <stdint.h>
namespace KnobMQTT {
struct Snapshot {
    double value=0;
    uint32_t receivedAt=0,sequence=0;
    bool connected=false,valid=false;
    char status[40]="MQTT disabled";
};
void begin();
void temperature(float value,bool valid,uint32_t measuredAt);
void read(Snapshot& out);
}
