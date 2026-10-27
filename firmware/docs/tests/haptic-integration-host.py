"""Run after merging the patch into the project; requires g++ and Python."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/app_gui_main.cpp').read_text()
def function(name):
 a=s.index(name);end=s.index('{',a)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
h=r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include "app_gui/pressure.h"
#include "app_gui/haptics.h"
#include "app_gui/light_link.h"
uint32_t testMillis=0,testUs=0;
uint32_t micros(){return testUs;} int digitalRead(int){return 0;}
KnobApp::Model app;KnobPressure::Input pressure;
bool latchedStop=false,running=true,consumeGesture=false,lost=false,suspended=false,heldOutput=false;
unsigned clickStage=0,lightRequests=0;
constexpr unsigned DRIVER_DIAG=17;
constexpr uint32_t STALE_US=5000,HOLD_US=2000,CLICK_HALF_US=5000;
constexpr float CLICK_VOLTAGE=3,DEG_TO_RAD_F=.017453293f,TAU_F=6.28318531f,PI_F=TAU_F/2;
constexpr int SENSOR_SIGN=-1;
uint32_t lastGoodUs=0,suspensionUs=0,resumedUs=0,startedMs=0,clickAt=0,clickGapTicks=0,heldTicks=0,zeroedTicks=0,recoveries=0,completedClicks=0,cancelledClicks=0;
float angularSpeed=0,activeCentre=0,angleCache=0;
const char* problem="test";
struct FakeMotor{float output=0;void move(float v){output=v;}void loopFOC(){}}motor;
void resetSpring(){} float springCommand(float v){return v;}
void stop(const char*){running=false;latchedStop=true;motor.move(0);clickStage=0;}
using std::min;
namespace KnobBackground{void suppressClick(){}}
namespace KnobLight{bool request(Action){++lightRequests;return true;}}
void anchor(){activeCentre=angleCache;}
void changedView(KnobApp::Screen before){if(before!=app.state.screen)anchor();if(app.state.screen==KnobApp::Screen::Standby)running=false;}
'''
h+='\n'.join(function(n) for n in ['float wrapAngle(','float positiveAngle(','float detentError(','void handleGesture(','bool clickVoltage(','void control('])
h+=r'''
int main(){
 using E=KnobPressure::Event;using S=KnobApp::Screen;
 app.activate(0);app.state.selected=1;handleGesture(E::Down);assert(clickStage==1 && app.state.screen==S::Home);
 clickStage=0;handleGesture(E::Short);assert(clickStage==0 && app.state.screen==S::Value);
 handleGesture(E::Long);assert(app.state.screen==S::Home && clickStage==0);
 handleGesture(E::Long);assert(app.state.screen==S::Standby && !running);
 handleGesture(E::Down);assert(app.state.screen==S::Home && consumeGesture);
 running=true;handleGesture(E::Short);assert(app.state.screen==S::Home && clickStage==0);
 app.state.selected=2;handleGesture(E::Down);handleGesture(E::Short);assert(app.state.screen==S::Light && lightRequests==0);
 clickStage=0;handleGesture(E::Down);assert(clickStage==1 && lightRequests==0);
 clickStage=0;handleGesture(E::Short);assert(clickStage==0 && lightRequests==1);
 handleGesture(E::Down);clickStage=0;handleGesture(E::Long);assert(app.state.screen==S::Home && lightRequests==1 && clickStage==0);
 app.activate(0);app.state.selected=0;activeCentre=0;angleCache=1;
 pressure.sampleGesture(0,0,0,true);pressure.sampleGesture(0,0,60,true);pressure.sampleGesture(24000,24000,100,true);pressure.sampleGesture(24000,24000,150,true);
 assert(pressure.pressed);float e=detentError();assert(app.state.selected==1 && !pressure.pressed && fabsf(e)<.4f);
 pressure.valid=true;pressure.lastData=testMillis=1000;testUs=1000000;lastGoodUs=testUs;clickStage=1;running=true;lost=false;
 control(true);assert(motor.output==3 && clickStage==2);
 testUs+=1000;lost=true;control(false);assert(motor.output==0 && clickStage==2);
 testUs+=1000;lost=false;lastGoodUs=testUs;control(true);assert(motor.output==3);
 testUs=1005000;lastGoodUs=testUs;control(true);assert(motor.output==-3);
 testUs=1010000;lastGoodUs=testUs;control(true);assert(motor.output==0 && clickStage==0 && completedClicks==1);
 clickStage=1;testUs=1020000;lastGoodUs=testUs;control(true);testUs+=1000;lost=true;control(false);testUs+=4000;control(false);assert(latchedStop && !running && motor.output==0);
 puts("PASS: press-edge click, no release click, long-press navigation, detents, pulse recovery and dropout stop");
}
'''
with tempfile.TemporaryDirectory(dir=root/'docs/tests') as d:
 cpp=Path(d)/'test.cpp';exe=Path(d)/'test';cpp.write_text(h)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-error=misleading-indentation','-I',str(root/'docs/tests/haptic-stubs'),'-I',str(root/'src'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
