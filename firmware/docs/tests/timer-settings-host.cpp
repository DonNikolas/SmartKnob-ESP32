// Host-only state/record tests. Generated with ChatGPT assistance.
#include <cassert>
#include <cstdio>
#include "../../src/app_gui/settings_record.h"
using namespace KnobApp;
int main(){
 Model m;m.activate(0);m.shortPress(0);assert(m.state.screen==Screen::Value);
 m.state.draft=1;m.shortPress(100);assert(m.state.timerRunning&&m.state.screen==Screen::Timer);
 m.tick(10100);assert(m.state.timerRemaining==50000);
 m.shortPress(10100);assert(!m.state.timerRunning);m.tick(30000);assert(m.state.timerRemaining==50000);
 m.shortPress(30100);m.longPress(30200);assert(m.state.screen==Screen::Home);
 m.state.selected=RADAR_APP;m.shortPress(30300);m.tick(80100);
 assert(m.state.timerDone&&!m.state.timerRunning&&m.state.screen==Screen::Radar);
 m.longPress(80101);m.state.selected=0;m.shortPress(80102);assert(m.state.screen==Screen::Timer);
 m.shortPress(80103);assert(!m.state.timerDone&&m.state.screen==Screen::Value);
 m.state.draft=2;m.shortPress(80104);m.shortPress(80105);m.turn(1,80106);
 assert(m.state.screen==Screen::Value&&!m.state.timerRunning&&m.state.timerRemaining==0);
 Model rollover;rollover.activate(0);rollover.shortPress(0);rollover.state.draft=1;
 rollover.shortPress(0xfffffff0U);rollover.updateTimer(984);assert(rollover.state.timerRemaining==59000);
 auto r=KnobSettings::capture(m.state);assert(KnobSettings::valid(r));
 State restored;KnobSettings::apply(r,restored);assert(restored.values[0]==2&&!restored.timerRunning&&restored.timerRemaining==0);
 r.values[4]=0;r.checksum=KnobSettings::checksum(r);assert(!KnobSettings::valid(r));
 r=KnobSettings::capture(m.state);r.values[1]++;assert(!KnobSettings::valid(r));
 r=KnobSettings::capture(m.state);r.version=2;r.checksum=KnobSettings::checksum(r);assert(!KnobSettings::valid(r));
 r=KnobSettings::capture(m.state);r.values[3]=221;r.checksum=KnobSettings::checksum(r);assert(!KnobSettings::valid(r));
 // Every editable app must persist a turned value without a confirming press.
 for(int app=0;app<5;++app){
     if(app==2)continue; // IR brightness is not an absolute saved percentage.
     Model edit;edit.activate(0);edit.state.selected=app;edit.shortPress(1);
     int expected=edit.state.values[app]+APPS[app].step;edit.turn(1,2);
     assert(edit.state.values[app]==expected);
     auto record=KnobSettings::capture(edit.state);assert(KnobSettings::valid(record));
     State boot;KnobSettings::apply(record,boot);assert(boot.values[app]==expected);
     edit.longPress(3);assert(edit.state.values[app]==expected);
     if(app==0)assert(!edit.state.timerRunning);
 }
 Model radar;radar.activate(0);radar.state.selected=RADAR_APP;radar.shortPress(1);radar.turn(1,2);
 auto record=KnobSettings::capture(radar.state);State boot;KnobSettings::apply(record,boot);
 assert(boot.radarRange==0&&boot.selected==RADAR_APP);
 puts("PASS: countdown, pause/resume, background completion, reset, millis rollover, settings validation");
}
