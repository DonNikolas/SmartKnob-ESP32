#include <cassert>
#include <cstdio>
#include "../../src/app_gui/light_protocol.h"
#include "../../src/app_gui/model.h"
using namespace KnobLight;
int main(){
 Protocol p;assert(!p.request(Action::Toggle,0));assert(p.poll(42,0,1).id==0);
 assert(p.request(Action::Toggle,2));auto a=p.poll(42,0,3);assert(a.command==Command::On);
 // Lost response/ack retries return the SAME ID, allowing receiver suppression.
 assert(p.poll(42,0,100).id==a.id);assert(p.sent==0);
 assert(p.request(Action::Brighter,101));assert(p.request(Action::Dimmer,102));
 auto b=p.poll(42,a.id,150);assert(b.command==Command::Brighter && b.id!=a.id && p.sent==1);
 assert(p.poll(42,a.id,160).id==b.id && p.sent==1);
 auto c=p.poll(42,b.id,300);assert(c.command==Command::Dimmer);
 assert(!p.poll(42,c.id,450).id && p.sent==3);
 assert(p.request(Action::Toggle,460));assert(p.poll(42,0,470).command==Command::Off);
 // Reboot does not replay a pending command; next press explicitly requests ON.
 assert(p.poll(99,0,480).id==0 && !p.powerKnown);
 assert(p.request(Action::Toggle,490));assert(p.poll(99,0,500).command==Command::On);
 p.expire(1700);assert(p.count==0 && !p.powerKnown);
 assert(!p.request(Action::Toggle,2501));
 p.poll(99,0,2600);for(unsigned i=0;i<Protocol::CAPACITY;++i)assert(p.request(Action::Brighter,2601+i));
 assert(!p.request(Action::Dimmer,2610));assert(p.count==Protocol::CAPACITY);
 p.expire(3802);assert(!p.count);
 Protocol roll;roll.poll(7,0,0xfffffff0U);assert(roll.request(Action::Brighter,0xfffffff5U));
 assert(roll.poll(7,0,20).id);roll.expire(1300);assert(!roll.count);
 KnobApp::Model m;m.activate(0);m.state.selected=2;m.shortPress(1);
 assert(m.state.screen==KnobApp::Screen::Light && m.slotsPerTurn()==12);
 int previous=m.state.values[2];m.turn(1,2);assert(m.state.values[2]==previous);
 m.shortPress(3);assert(m.state.screen==KnobApp::Screen::Light);m.longPress(4);assert(m.state.screen==KnobApp::Screen::Home);
 puts("PASS: bounded queue, ACK retry, command order, reboot, expiry, rollover, Light UI model");
}
