// Generated with ChatGPT assistance. Host tests; not an ESP32 hardware test.
#include <cassert>
#include "../../src/app_gui/radar_parse.h"
#include "../../src/app_gui/model.h"
int main(){
 using namespace KnobRadar;
 Snapshot s;
 const char* test=R"({"ac":[{"hex":"123456","flight":"ABC   ","lat":47.696,"lon":8.635,"seen_pos":0,"alt_baro":1000,"gs":100},{"hex":"old","lat":47.696,"lon":8.635,"seen_pos":20},{"hex":"bad","lat":null,"lon":8.635,"seen_pos":0}]})";
 assert(parse(test,strlen(test),47.696,8.635,s)&&s.count==1);
 assert(!strcmp(s.aircraft[0].callsign,"ABC"));assert(fabs(s.aircraft[0].altitude-304.8)<.01);
 const char* sorted=R"({"ac":[{"hex":"far","lat":47.80,"lon":8.635,"seen_pos":0},{"hex":"near","lat":47.70,"lon":8.635,"seen_pos":0}]})";
 assert(parse(sorted,strlen(sorted),47.696,8.635,s)&&s.count==2);
 assert(!strcmp(s.aircraft[0].hex,"near"));
 assert(!parse("{}",2,47.696,8.635,s));assert(!parse("{",1,47.696,8.635,s));
 KnobApp::Model m;m.activate(0);m.state.selected=KnobApp::RADAR_APP;m.shortPress(1);
 assert(m.state.screen==KnobApp::Screen::Radar && m.interactive());
 m.turn(1,2);assert(m.state.radarRange==0);m.shortPress(3);assert(m.state.radarDetails);
 m.turn(-1,4);assert(m.state.radarAircraft==-1);m.longPress(5);assert(m.state.screen==KnobApp::Screen::Home);
 for(int i=0;i<KnobApp::APP_COUNT;++i){m.state.selected=i;m.shortPress(6);m.longPress(7);assert(m.state.screen==KnobApp::Screen::Home);}
 puts("Radar parser, units, stale filtering, sorting and navigation: PASS");
}
