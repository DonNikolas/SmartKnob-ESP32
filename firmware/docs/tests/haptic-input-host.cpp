#include <cassert>
#include <cstdio>
#include <cmath>
#include <initializer_list>
#include "../../src/app_gui/pressure.h"
#include "../../src/app_gui/haptics.h"
uint32_t testMillis=0;
using E=KnobPressure::Event;using Input=KnobPressure::Input;
void arm(Input& p,uint32_t t=0){p.sampleGesture(0,0,t,true);p.sampleGesture(0,0,t+60,true);}
void press(Input& p,uint32_t t=100){assert(p.sampleGesture(24000,24000,t,true)==E::None);assert(p.sampleGesture(24000,24000,t+50,true)==E::Down);}
int main(){
 {Input p;arm(p);assert(p.sampleGesture(11999,11999,100,true)==E::None);
  assert(p.sampleGesture(11999,11999,200,true)==E::None);
  assert(p.sampleGesture(12000,12000,250,true)==E::None);
  assert(p.sampleGesture(12000,12000,300,true)==E::Down);
  assert(p.sampleGesture(13000,13000,350,true)==E::None);
  p.sampleGesture(5900,5900,400,true);assert(p.sampleGesture(5900,5900,460,true)==E::Short);}
 {Input p;arm(p);press(p);p.observeMotion(.02f,.6f,160,false);p.sampleGesture(0,0,200,true);assert(p.sampleGesture(0,0,260,true)==E::Short);}
 {Input p;arm(p);press(p);assert(p.sampleGesture(24000,24000,1050,true)==E::Long);
  assert(p.sampleGesture(24000,24000,1500,true)==E::None);p.sampleGesture(0,0,1600,true);assert(p.sampleGesture(0,0,1660,true)==E::None);}
 {Input p;arm(p);press(p);p.observeMotion(.12f,1,160,false);assert(!p.pressed && p.motionCancels==1);assert(p.sampleGesture(24000,24000,1000,true)==E::None);arm(p,1100);press(p,1200);}
 {Input p;arm(p);p.observeMotion(.01f,1,100,false);for(uint32_t t=100;t<2000;t+=10)assert(p.sampleGesture(24000,24000,t,true)==E::None);}
 {Input p;arm(p);for(uint32_t t=100;t<2000;t+=10)assert(p.sampleGesture(-30000,-30000,t,true)==E::None);}
 {Input p;arm(p);p.observeMotion(6.27f,0,70,false);press(p);p.observeMotion(.01f,.1f,160,false);assert(p.pressed);}
 {Input p;p.begin();p.tare();for(int i=0;i<36;++i){testMillis+=13;p.poll(true);}assert(p.valid && p.offset==300000 && p.count==4);p.afterClick();p.adc.raw=8388607;testMillis+=13;p.poll(true);assert(!p.valid && p.fault);}
 {Input p;p.begin();p.adc.ready=false;testMillis+=2000;p.poll(true);assert(p.fault);}
 for(unsigned slots:{8u,12u,50u}){float width=6.28318531f/slots;assert(KnobHaptics::demand(0,0,width)==0);assert(KnobHaptics::demand(width*.45f,0,width)>.4f);assert(KnobHaptics::demand(0,1,width)<0);
  for(int i=-1000;i<=1000;++i){float e=i*.003f,v=KnobHaptics::demand(e,2,width);assert(std::isfinite(v)&&fabsf(v)<=1.00001f);assert(fabsf(v+KnobHaptics::demand(-e,-2,width))<.00001f);}}
 puts("PASS: v3 threshold, gestures, motion rejection, ADC faults, bounded haptics");
}
