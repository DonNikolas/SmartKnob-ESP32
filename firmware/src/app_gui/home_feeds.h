// Boxtec read-only home measurements. Generated with ChatGPT assistance.
#pragma once
#include <stdint.h>
namespace KnobHomeFeeds {
constexpr int COUNT=5;
constexpr uint32_t STALE_SECONDS=300;
struct Feed {unsigned id;const char* label;const char* unit;unsigned decimals;};
static constexpr Feed FEEDS[COUNT]={
    {264,"Freezer","°C",1}, {428,"Humidity","%RH",1},
    {298,"Garage temperature","°C",1}, {300,"Garage Light","",0},
    {267,"Air pressure","hPa",1}
};
struct Reading {double value=0;uint32_t sourceTime=0,receivedAt=0;bool valid=false;char status[40]="Not loaded";};
struct Snapshot {Reading readings[COUNT];uint32_t sequence=0;};
void begin();
void select(bool active,int index);
void read(Snapshot& result);
}
