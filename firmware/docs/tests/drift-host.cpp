// Run from firmware with g++ (host only, no ESP32 claim).
// g++ -std=c++17 -I docs/tests/drift-stubs -I src/app_gui docs/tests/drift-host.cpp -o drift-host
#include "../../src/app_gui/pressure.h"
#include <cassert>
#include <cstring>
#include <iostream>
using KnobPressure::Input;
using E=KnobPressure::Event;
int main(){
    static_assert(Input::PRESS_COUNTS==25000 && Input::RELEASE_COUNTS==10000,"Test configuration");
    Input p;p.begin();p.tare();p.adc.raw=300000;
    for(int n=0;n<32;++n){testMs+=13;p.poll(true);}
    assert(p.valid && p.offset==300000);
    for(int n=0;n<12;++n){testMs+=13;p.poll(true);}
    assert(!strcmp(p.diagnosticState(testMs),"ARMED"));
    const long zero=p.offset;p.clearInterval();p.adc.raw=312000;
    for(int n=0;n<100;++n){testMs+=13;p.poll(true);}
    assert(p.offset==zero && p.observedDelta==12000 && p.filteredDelta==12000);
    assert(p.intervalSamples==100 && p.intervalMin==12000 && p.intervalMax==12000 && p.downEvents==0);
    p.adc.raw=340000;
    unsigned down=0;
    for(int n=0;n<12;++n){testMs+=13;if(p.poll(true)==E::Down)++down;}
    assert(down==1 && p.pressed);
    const long filtered=p.filteredDelta;p.afterClick();p.clearInterval();p.adc.raw=350000;
    testMs+=13;p.poll(true);
    assert(p.lastRaw==350000 && p.observedDelta==50000 && p.filteredDelta==filtered);
    assert(!strcmp(p.diagnosticState(testMs),"CLICK_BLANK"));
    assert(p.intervalSamples==1 && p.intervalMin==50000 && p.offset==zero);
    // ADC fault detection still active during blanking.
    p.adc.raw=8388607;testMs+=13;p.poll(true);
    assert(!p.valid && p.fault && !strcmp(p.diagnosticState(testMs),"FAULT"));
    Input q;q.begin();q.valid=true;q.offset=100;q.adc.ready=false;
    testMs+=2000;q.poll(true);assert(q.fault && !q.valid);
    // Current long/short and boundary policies preserved.
    Input r;r.sampleGesture(0,0,0,true);r.sampleGesture(0,0,60,true);
    r.setStartAllowed(false);r.sampleGesture(30000,30000,100,true);
    assert(r.blockedPresses==1 && r.isBlocked());
    r.setStartAllowed(true);assert(r.sampleGesture(30000,30000,500,true)==E::None);
    r.sampleGesture(0,0,600,true);r.sampleGesture(0,0,660,true);
    r.sampleGesture(30000,30000,700,true);assert(r.sampleGesture(30000,30000,750,true)==E::Down);
    assert(r.sampleGesture(30000,30000,1650,true)==E::Long);
    r.sampleGesture(0,0,1700,true);assert(r.sampleGesture(0,0,1760,true)==E::None);
    assert(r.releases==1 && r.shortEvents==0);
    std::cout<<"Diagnostic acquisition, fixed zero, blanking and gesture tests passed\n";
}
