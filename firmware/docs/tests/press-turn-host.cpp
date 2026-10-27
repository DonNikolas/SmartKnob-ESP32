// g++ -std=c++17 -I docs/tests/press-turn-stubs -I src/app_gui docs/tests/press-turn-host.cpp -o press-turn-host
#include "../../src/app_gui/pressure.h"
#include "../../src/app_gui/press_turn_policy.h"
#include <cassert>
#include <iostream>
using E=KnobPressure::Event;
using KnobPressure::Input;
void arm(Input& p,uint32_t at=0){assert(p.sampleGesture(0,0,at,true)==E::None);assert(p.sampleGesture(0,0,at+60,true)==E::None);}
void down(Input& p,uint32_t at){assert(p.sampleGesture(25000,25000,at,true)==E::None);assert(p.sampleGesture(25000,25000,at+50,true)==E::Down);}
int main(){
    assert(KnobPressTurn::allowStart(true,true,.2f));
    assert(!KnobPressTurn::allowStart(true,true,.45f));
    assert(!KnobPressTurn::allowStart(true,true,-.45f));
    assert(KnobPressTurn::allowStart(false,true,.45f));
    assert(KnobPressTurn::allowStart(true,false,.45f));
    Input p;arm(p);down(p,100);
    p.setStartAllowed(false); // accepted press must survive leaving start window
    assert(p.sampleGesture(25000,25000,200,true)==E::None && p.pressed);
    assert(p.sampleGesture(-20000,-20000,250,true)==E::None && p.pressed); // no silent cancellation
    assert(p.sampleGesture(-10000,-10000,300,true)==E::None);
    assert(p.sampleGesture(-10000,-10000,360,true)==E::Short);
    assert(p.releases==1 && p.shortEvents==1);
    // Boundary rejection is latched until release, including in no-guard mode.
    assert(p.sampleGesture(25000,25000,400,true)==E::None && p.isBlocked());
    p.setStartAllowed(true);
    assert(p.sampleGesture(25000,25000,700,true)==E::None && p.downEvents==1);
    p.sampleGesture(0,0,800,true);p.sampleGesture(0,0,860,true);
    assert(!p.isBlocked());down(p,900);
    assert(p.sampleGesture(25000,25000,1850,true)==E::Long);
    assert(p.sampleGesture(25000,25000,2000,true)==E::None);
    p.sampleGesture(0,0,2100,true);assert(p.sampleGesture(0,0,2160,true)==E::None);
    assert(p.releases==2 && p.shortEvents==1 && p.longEvents==1);
    // Abort if boundary reached during 50ms confirmation, before DOWN.
    Input q;arm(q);q.sampleGesture(25000,25000,100,true);q.setStartAllowed(false);
    assert(q.sampleGesture(25000,25000,150,true)==E::None && q.isBlocked());
    // Brief pressure spikes and negative force cannot start presses.
    Input r;arm(r);r.sampleGesture(25000,25000,100,true);r.sampleGesture(0,0,120,true);
    assert(r.sampleGesture(-25000,-25000,200,true)==E::None && r.downEvents==0);
    // Acquisition: saturation still faults during the click blanking period.
    r.valid=true;fakeMs=300;r.afterClick();r.adc.value=8388607;
    r.poll(true);assert(!r.valid && r.fault);
    Input t;t.begin();t.valid=true;t.adc.ready=false;fakeMs+=2000;t.poll(true);assert(t.fault && !t.valid);
    // Fresh tare and four-sample filter; thresholds independent of model.h.
    Input u;u.begin();u.tare();u.adc.value=300000;
    for(int n=0;n<32;++n){fakeMs+=13;u.poll(true);}
    assert(u.valid && u.offset==300000);
    for(int n=0;n<12;++n){fakeMs+=13;u.poll(true);}
    u.adc.value=320000;unsigned events=0;
    for(int n=0;n<20;++n){fakeMs+=13;if(u.poll(true)==E::Down)++events;}
    assert(events==1 && u.pressed);
    std::cout<<"Pressure/gate/acquisition tests passed\n";
}
