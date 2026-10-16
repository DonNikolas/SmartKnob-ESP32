#pragma once
#include <ArduinoJson.h>
#include <math.h>
#include "home_feeds.h"
namespace KnobHomeFeeds {
inline bool parse(const char* data,size_t length,Reading& out){
    StaticJsonDocument<256> doc;
    if(deserializeJson(doc,data,length)||!doc.is<JsonObject>())return false;
    // Numeric strings/null/errors are not measurements. In particular null must
    // never silently become zero, and a successful fetch is not a fresh sample.
    if(!doc["value"].is<double>()||!doc["time"].is<uint32_t>())return false;
    double value=doc["value"].as<double>();uint32_t time=doc["time"];
    if(!isfinite(value)||time<1577836800UL)return false;
    out.value=value;out.sourceTime=time;out.valid=true;return true;
}
}
