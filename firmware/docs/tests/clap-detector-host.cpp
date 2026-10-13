// Synthetic input tests; no claim of real acoustic accuracy.
#include <cassert>
#include <iostream>
#include "../../src/app_gui/clap_detector.h"
#include "../../src/app_gui/model.h"
struct Test {
 KnobAudio::Detector d;uint32_t now=0;int hits=0;
 void step(float level){now+=16;if(d.update(level,now))++hits;}
 void quiet(int n){while(n--)step(10);}
 void clap(){step(1600);quiet(2);}
 void learn(){quiet(125);assert(d.ready());}
};
int main(){
 Test t;t.learn();t.clap();t.quiet(30);assert(t.hits==0); // Single clap.
 Test pair;pair.learn();pair.clap();pair.quiet(10);pair.clap();assert(pair.hits==1);
 pair.quiet(10);pair.clap();pair.quiet(10);pair.clap();assert(pair.hits==1); // Lockout.
 pair.quiet(70);pair.clap();pair.quiet(10);pair.clap();assert(pair.hits==2);
 Test late;late.learn();late.clap();late.quiet(40);late.clap();assert(late.hits==0);
 Test noise;noise.learn();for(int i=0;i<20;++i)noise.step(1600);noise.quiet(12);noise.clap();assert(noise.hits==0);
 Test muted;muted.learn();muted.clap();muted.d.cancel(muted.now);muted.quiet(12);muted.clap();assert(muted.hits==0);
 Test roll;roll.now=UINT32_MAX-3000;roll.learn();roll.clap();roll.quiet(10);roll.clap();assert(roll.hits==1);
 KnobApp::Model m;m.activate(0);m.state.selected=KnobApp::SENSOR_APP;m.shortPress(1);
 assert(m.state.screen==KnobApp::Screen::Sensors);m.turn(1,2);assert(m.state.screen==KnobApp::Screen::Sensors);
 m.longPress(3);assert(m.state.screen==KnobApp::Screen::Home);m.shortPress(4);m.shortPress(5);assert(m.state.screen==KnobApp::Screen::Home);
 m.state.selected=KnobApp::MORE_APP;m.shortPress(6);assert(m.state.screen==KnobApp::Screen::Menu);
 std::cout<<"PASS: clap pairing, single/long-noise rejection, lockout, suppression, rollover; Sensors and More navigation.\n";
}
