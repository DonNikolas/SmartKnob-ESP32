// Versioned app preferences only. Never persist pressure tare or motor calibration.
// Generated with ChatGPT assistance.
#pragma once
#include "model.h"
#include <stddef.h>
namespace KnobSettings {
struct Record {
    uint32_t version = 1;
    int32_t values[5] = {15,75,60,220,80};
    int32_t radarRange = 3, selected = 0;
    uint32_t checksum = 0;
};
static_assert(sizeof(Record)==36,"Unexpected settings record layout");
inline uint32_t checksum(const Record& r) {
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(&r);
    uint32_t hash=2166136261U;
    for(size_t i=0;i<offsetof(Record,checksum);++i)hash=(hash^bytes[i])*16777619U;
    return hash;
}
inline Record capture(const KnobApp::State& s) {
    Record r;for(unsigned i=0;i<5;++i)r.values[i]=s.values[i];
    r.radarRange=s.radarRange;r.selected=s.selected;r.checksum=checksum(r);return r;
}
inline bool valid(const Record& r) {
    if(r.version!=1 || r.checksum!=checksum(r) || r.radarRange<0 || r.radarRange>3 ||
       r.selected<0 || r.selected>=KnobApp::APP_COUNT)return false;
    for(unsigned i=0;i<5;++i){const auto& a=KnobApp::APPS[i];
        if(r.values[i]<a.low || r.values[i]>a.high || (r.values[i]-a.low)%a.step)return false;
    }return true;
}
inline bool equal(const Record& a,const Record& b) {
    if(a.radarRange!=b.radarRange || a.selected!=b.selected)return false;
    for(unsigned i=0;i<5;++i)if(a.values[i]!=b.values[i])return false;
    return true;
}
inline void apply(const Record& r,KnobApp::State& s) {
    for(unsigned i=0;i<5;++i)s.values[i]=r.values[i];
    s.radarRange=r.radarRange;s.selected=r.selected;
}
}
