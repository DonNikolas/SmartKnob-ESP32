// App preferences v2: preserve v1 values and add the auto-standby option.
#pragma once
#include "model.h"
#include <stddef.h>
namespace KnobSettings {
struct LegacyRecord {
    uint32_t version=1;
    int32_t values[5]={15,75,60,220,80};
    int32_t radarRange=3,selected=0;
    uint32_t checksum=0;
};
struct Record {
    uint32_t version=2;
    int32_t values[5]={15,75,60,220,80};
    int32_t radarRange=3,selected=0,standbyOption=2;
    uint32_t checksum=0;
};
static_assert(sizeof(LegacyRecord)==36,"Unexpected v1 record layout");
static_assert(sizeof(Record)==40,"Unexpected v2 record layout");
template<class T> inline uint32_t checksum(const T& r) {
    const uint8_t* bytes=reinterpret_cast<const uint8_t*>(&r);uint32_t hash=2166136261U;
    for(size_t i=0;i<offsetof(T,checksum);++i)hash=(hash^bytes[i])*16777619U;
    return hash;
}
template<class T> inline bool validValues(const T& r) {
    if(r.radarRange<0||r.radarRange>3||r.selected<0||r.selected>=KnobApp::APP_COUNT)return false;
    for(unsigned i=0;i<5;++i){const auto& a=KnobApp::APPS[i];
        if(r.values[i]<a.low||r.values[i]>a.high||(r.values[i]-a.low)%a.step)return false;
    }return true;
}
inline bool valid(const LegacyRecord& r){return r.version==1&&r.checksum==checksum(r)&&validValues(r);}
inline bool valid(const Record& r){return r.version==2&&r.checksum==checksum(r)&&validValues(r)&&KnobApp::validStandby(r.standbyOption);}
inline Record migrate(const LegacyRecord& old){
    Record r;for(unsigned i=0;i<5;++i)r.values[i]=old.values[i];
    r.radarRange=old.radarRange;r.selected=old.selected;r.checksum=checksum(r);return r;
}
inline Record capture(const KnobApp::State& s){
    Record r;for(unsigned i=0;i<5;++i)r.values[i]=s.values[i];
    r.radarRange=s.radarRange;r.selected=s.selected;r.standbyOption=s.standbyOption;r.checksum=checksum(r);return r;
}
inline bool equal(const Record& a,const Record& b){
    if(a.radarRange!=b.radarRange||a.selected!=b.selected||a.standbyOption!=b.standbyOption)return false;
    for(unsigned i=0;i<5;++i)if(a.values[i]!=b.values[i])return false;
    return true;
}
inline void apply(const Record& r,KnobApp::State& s){
    for(unsigned i=0;i<5;++i)s.values[i]=r.values[i];
    s.radarRange=r.radarRange;s.selected=r.selected;s.standbyOption=r.standbyOption;
}
}
