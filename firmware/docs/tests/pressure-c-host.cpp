#include <cassert>
#include <cstring>
#include <cstdio>
#include "pressure-original.h"
#include "../../src/app_gui/pressure.h"
uint32_t testNow=0;bool testReady=true;long testRaw=0;unsigned testReadDelay=0;
OriginalPressure::Input old;
KnobPressure::Input converted;
void compare(){
    assert(old.valid==converted.valid);
    assert(old.zeroing==converted.zeroing);
    assert(old.pressed==converted.pressed);
    assert(old.offset==converted.offset);
    assert(old.magnitude==converted.magnitude);
    assert(old.rawMagnitude==converted.rawMagnitude);
    assert(old.signedDelta==converted.signedDelta);
    assert(old.filteredDelta==converted.filteredDelta);
    assert(old.minimumDelta==converted.minimumDelta);
    assert(old.maximumDelta==converted.maximumDelta);
    assert(old.lastRaw==converted.lastRaw);
    assert(old.observedDelta==converted.observedDelta);
    assert(old.intervalMin==converted.intervalMin);
    assert(old.intervalMax==converted.intervalMax);
    assert(old.totalSamples==converted.totalSamples);
    assert(old.intervalSamples==converted.intervalSamples);
    assert(old.motionCancels==converted.motionCancels);
    assert(old.downEvents==converted.downEvents);
    assert(old.shortEvents==converted.shortEvents);
    assert(old.longEvents==converted.longEvents);
    assert(old.releases==converted.releases);
    assert(old.blockedPresses==converted.blockedPresses);
    assert(old.intervalValid==converted.intervalValid);
    assert(old.count==converted.count);
    assert(old.zeroCount==converted.zeroCount);
    assert(old.lastData==converted.lastData);
    assert(old.pressDirection==converted.pressDirection);
 assert((old.fault==nullptr)==(converted.fault==nullptr));
 if(old.fault)assert(!strcmp(old.fault,converted.fault));
 assert(old.isBlocked()==converted.isBlocked());
 assert(old.releasedReady(testNow)==converted.releasedReady(testNow));
 assert(!strcmp(old.diagnosticState(testNow),converted.diagnosticState(testNow)));
}
int main(){
 old.begin();converted.begin();old.tare();converted.tare();
 unsigned events[4]={};uint32_t random=1234567;
 for(unsigned i=0;i<240000;++i){
  unsigned phase=i%800;
  // Start close to millis rollover during the second half.
  uint32_t now=(i<120000?0U:0xffff0000U)+(i%120000)*25U;
  testNow=now;
  if(phase==0){old.tare();converted.tare();}
  bool allowed= !((i/800)%5==1 && phase>50 && phase<500);
  old.setStartAllowed(allowed);converted.setStartAllowed(allowed);
  random=random*1664525U+1013904223U;
  long noise=long((random>>16)%500)-250;
  long delta=0;
  if(phase>=60&&phase<70)delta=60000;
  if(phase>=110&&phase<170)delta=-70000;
  if(phase>=210&&phase<270)delta=70000;
  if(phase>=270&&phase<285)delta=-90000; // release by opposite rebound
  if(phase>=320&&phase<450)delta=long(random%240001)-120000;
  testRaw=300000+delta+noise;
  if(phase==510)testRaw=8388607;
  if(phase==610)testRaw=-8388608;
  testReady=!(phase>=650&&phase<750);
  testReadDelay=i%4;
  bool enabled=phase<470 || phase>490;
  auto a=old.poll(enabled);uint32_t oldTime=testNow;
  testNow=now;auto b=converted.poll(enabled);
  assert(oldTime==testNow);assert(int(a)==int(b));++events[int(b)];compare();
  if(a==OriginalPressure::Event::Down){old.afterClick();converted.afterClick();compare();}
  if(phase==480){old.clearInterval();converted.clearInterval();compare();}
 }
 assert(events[1]>0&&events[2]>0&&events[3]>0);
 printf("PASS: 240000 identical poll results and public states. Down=%u Short=%u Long=%u\n",events[1],events[2],events[3]);
 // Zero timeout with no samples.
 old.tare();converted.tare();testNow+=6001;testReady=false;
 assert(int(old.poll(true))==int(converted.poll(true)));compare();assert(converted.fault);
 puts("PASS: zero timeout and unavailable ADC.");
}
