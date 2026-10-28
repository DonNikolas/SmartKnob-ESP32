#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../src/app_gui/settings_record.h"
#include "../../src/app_gui/clock_text.h"
using namespace KnobApp;
time_t utc(int month,int day,int hour,int minute){tm t={};t.tm_year=126;t.tm_mon=month-1;t.tm_mday=day;t.tm_hour=hour;t.tm_min=minute;return timegm(&t);}
int main(){
 Model m;m.activate(10);m.state.selected=SETTINGS_APP;m.shortPress(20);assert(m.state.screen==Screen::Menu);
 m.shortPress(30);assert(m.state.screen==Screen::WifiConfirm&&m.state.wifiChoice==1);
 m.shortPress(40);assert(m.state.screen==Screen::Menu);
 m.shortPress(50);m.turn(-1,60);m.shortPress(70);assert(m.state.screen==Screen::WifiSetup&&!m.interactive());
 m.activate(80);m.shortPress(90);m.turn(1,100);m.shortPress(110);assert(m.state.screen==Screen::StandbySettings);
 for(int i=0;i<20;++i){m.turn(-1,120);}assert(m.state.standbyOption==0&&!m.canTurn(-1));
 m.longPress(130);m.longPress(140);m.tick(900000);assert(m.state.screen==Screen::Home);
 m.state.standbyOption=1;m.touch(1000);m.tick(30999);assert(m.state.screen==Screen::Home);m.tick(31000);assert(m.state.screen==Screen::Standby);
 m.turn(1,31001);assert(m.state.screen==Screen::Home);
 m.state.activityAt=0xfffffff0U;m.tick(29983);assert(m.state.screen==Screen::Home);m.tick(29984);assert(m.state.screen==Screen::Standby);
 m.activate(0);m.state.timerRunning=true;m.state.timerRemaining=60000;m.state.timerUpdatedAt=0;m.tick(40000);assert(m.state.screen==Screen::Home);
 m.tick(60000);assert(m.state.timerDone&&m.state.screen==Screen::Home);m.tick(69999);assert(m.state.screen==Screen::Home);m.tick(70000);assert(m.state.screen==Screen::Standby);
 KnobSettings::LegacyRecord old;old.values[0]=23;old.values[1]=31;old.selected=8;old.checksum=KnobSettings::checksum(old);assert(KnobSettings::valid(old));
 auto current=KnobSettings::migrate(old);assert(KnobSettings::valid(current)&&current.values[0]==23&&current.selected==8&&current.standbyOption==2);
 current.standbyOption=5;current.checksum=KnobSettings::checksum(current);assert(KnobSettings::valid(current));KnobSettings::apply(current,m.state);assert(m.state.standbyOption==5);
 current.values[0]++;assert(!KnobSettings::valid(current));current.standbyOption=6;current.checksum=KnobSettings::checksum(current);assert(!KnobSettings::valid(current));
 char text[8];KnobClock::format(text,sizeof text,0);assert(!strcmp(text,"--:--"));
 KnobClock::format(text,sizeof text,utc(3,29,0,59));assert(!strcmp(text,"01:59"));KnobClock::format(text,sizeof text,utc(3,29,1,0));assert(!strcmp(text,"03:00"));
 KnobClock::format(text,sizeof text,utc(10,25,0,59));assert(!strcmp(text,"02:59"));KnobClock::format(text,sizeof text,utc(10,25,1,0));assert(!strcmp(text,"02:00"));
 KnobClock::format(text,sizeof text,utc(12,31,23,5));assert(!strcmp(text,"00:05"));
 puts("PASS: Settings navigation, auto-standby, timer protection, millis wrap, v1 migration, invalid records and Zurich clock transitions.");
}
