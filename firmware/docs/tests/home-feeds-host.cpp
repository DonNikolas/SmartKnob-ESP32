// Host tests use ArduinoJson 6.x and the actual app model/settings record.
#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../src/app_gui/home_feeds_parse.h"
#include "../../src/app_gui/settings_record.h"
int main(){
 using namespace KnobHomeFeeds;
 Reading r;
 const char* good="{\"time\":1792140147,\"value\":-22.93}";
 assert(parse(good,strlen(good),r));assert(r.valid && r.value==-22.93 && r.sourceTime==1792140147);
 const char* bad[]={"null","0","{\"time\":1792140147,\"value\":null}","{\"value\":12}","{\"time\":0,\"value\":12}","{\"time\":1792140147,\"value\":\"12\"}","<html>Error</html>"};
 for(auto text:bad){assert(!parse(text,strlen(text),r));assert(r.value==-22.93);}
 const char* raw="{\"time\":1792140221,\"value\":117}";assert(parse(raw,strlen(raw),r)&&r.value==117);
 KnobApp::Model m;m.activate(0);m.state.selected=KnobApp::HOME_FEEDS_APP;m.shortPress(1);
 assert(m.state.screen==KnobApp::Screen::HomeFeeds && m.slotsPerTurn()==12);
 m.turn(-1,2);assert(m.state.homeFeed==4);m.turn(1,3);assert(m.state.homeFeed==0);
 m.longPress(4);assert(m.state.screen==KnobApp::Screen::Home);
 auto record=KnobSettings::capture(m.state);assert(KnobSettings::valid(record));
 KnobApp::State boot;KnobSettings::apply(record,boot);assert(boot.selected==KnobApp::HOME_FEEDS_APP);
 m.turn(1,5);assert(m.state.selected==0);m.turn(-1,6);assert(m.state.selected==KnobApp::HOME_FEEDS_APP);
 m.shortPress(7);m.shortPress(8);assert(m.state.screen==KnobApp::Screen::Home);
 puts("PASS: typed values/source timestamps, bad-response preservation, raw garage signal, navigation and saved app selection");
}
