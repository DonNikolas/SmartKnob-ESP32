// Call load before enabling the motor; call save only with the driver disabled.
#pragma once
#include <Preferences.h>
#include "settings_record.h"
namespace KnobSettings {
inline bool load(KnobApp::State& state) {
    Preferences p;if(!p.begin("knob-app",true))return false;
    Record r;bool ok=false;
    const size_t length=p.getBytesLength("settings");
    if(length==sizeof(r))ok=p.getBytes("settings",&r,sizeof(r))==sizeof(r)&&valid(r);
    else if(length==sizeof(LegacyRecord)){
        LegacyRecord old;
        ok=p.getBytes("settings",&old,sizeof(old))==sizeof(old)&&valid(old);
        if(ok)r=migrate(old);
    }
    p.end();if(ok)apply(r,state);return ok;
}
inline bool save(const Record& record) {
    if(!valid(record))return false;
    Preferences p;if(!p.begin("knob-app",false))return false;
    bool ok=p.putBytes("settings",&record,sizeof(record))==sizeof(record);
    Record check;
    ok=ok && p.getBytes("settings",&check,sizeof(check))==sizeof(check) && valid(check) && equal(record,check);
    p.end();return ok;
}
}
