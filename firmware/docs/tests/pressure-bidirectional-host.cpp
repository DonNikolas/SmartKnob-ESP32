#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../../src/app_gui/pressure.h"
uint32_t testNow = 0;
using KnobPressure::Input;
using KnobPressure::Event;
Event sample(Input& p, long v, uint32_t t) { return p.sampleGesture(v,v,t,true); }
void arm(Input& p, uint32_t t=0) { sample(p,0,t); sample(p,0,t+150); }
void down(Input& p, int sign, uint32_t t=200) {
    assert(sample(p,sign*50000,t)==Event::None);
    assert(sample(p,sign*50000,t+50)==Event::Down);
    assert(p.pressDirection==sign);
}
int main() {
    for (int sign : {-1,1}) {
        Input p; arm(p); down(p,sign);
        assert(sample(p,0,400)==Event::None);
        assert(sample(p,0,460)==Event::Short);
        // Opposite rebound cannot become a second click.
        sample(p,-sign*100000,470); sample(p,-sign*100000,700);
        assert(p.downEvents==1 && p.shortEvents==1);
        arm(p,800); down(p,-sign,1000);
        assert(sample(p,-sign*50000,1950)==Event::Long);
        assert(sample(p,-sign*50000,2200)==Event::None);
        sample(p,0,2300); assert(sample(p,0,2360)==Event::None);
        assert(p.longEvents==1 && p.releases==2 && p.shortEvents==1);

        Input b; arm(b); b.setStartAllowed(false);
        sample(b,sign*50000,200); assert(b.isBlocked());
        b.setStartAllowed(true); sample(b,sign*50000,400);
        assert(b.downEvents==0); arm(b,500); down(b,sign,700);

        Input cross; arm(cross); down(cross,sign);
        sample(cross,-sign*50000,1140);
        assert(sample(cross,-sign*50000,1200)==Event::Short);
        assert(cross.longEvents==0);
    }
    Input change; arm(change); sample(change,50000,200);
    assert(sample(change,-50000,240)==Event::None);
    assert(sample(change,-50000,280)==Event::None);
    assert(sample(change,-50000,290)==Event::Down);
    Input wrap; arm(wrap,0xffffff00U); down(wrap,1,0xfffffff0U);
    Input disabled; arm(disabled); down(disabled,1);
    disabled.sampleGesture(50000,50000,300,false); assert(!disabled.pressed);
    Input a; a.begin(); a.tare();
    for(int i=0;i<32;++i) { testNow+=13; a.poll(false); }
    assert(a.valid && a.offset==300000);
    a.afterClick(); a.adc.raw=300123; testNow+=1; a.poll(true);
    assert(a.observedDelta==123 && a.count==0);
    a.adc.raw=8388607; a.poll(true); assert(a.fault && !a.valid);
    Input timeout; timeout.begin(); timeout.adc.ready=false;
    testNow+=2000; timeout.poll(true); assert(timeout.fault);
    puts("PASS: both signs, short/long, rebound, boundary, sign change, timer wrap, tare, blanking, ADC fault and timeout");
}
