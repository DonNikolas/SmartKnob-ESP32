// MQTT_PREP_V1
// Boxtec read-only home measurements. Generated with ChatGPT assistance.
#pragma once
#include <stdint.h>
#include "mqtt_config.h"
namespace KnobHomeFeeds {
constexpr int HTTP_COUNT=5;
constexpr int MQTT_INDEX=5;
constexpr int COUNT=6;
constexpr uint32_t STALE_SECONDS=300;
struct Feed {unsigned id;const char* label;const char* unit;unsigned decimals;};
static constexpr Feed FEEDS[COUNT]={
    {264,"Freezer","°C",1}, {428,"Humidity","%RH",1},
    {298,"Garage temperature","°C",1}, {300,"Garage Light","",0},
    {267,"Air pressure","hPa",1},
    {0,KnobMQTTConfig::INPUT_LABEL,KnobMQTTConfig::INPUT_UNIT,KnobMQTTConfig::INPUT_DECIMALS}
};
struct Reading {double value=0;uint32_t sourceTime=0,receivedAt=0;bool valid=false;char status[40]="Not loaded";};
struct Snapshot {Reading readings[COUNT];uint32_t sequence=0;};
void begin();
void select(bool active,int index);
void read(Snapshot& result);
}
