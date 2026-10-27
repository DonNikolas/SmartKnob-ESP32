"""Compile actual detent/gesture/control functions with hardware mocks.
Run from firmware: python docs/tests/press-turn-integration.py
Requires g++; no device/network access. Test build goes to a temporary directory.
"""
from pathlib import Path
import os, subprocess, tempfile
firmware=Path(__file__).resolve().parents[2]
source=(firmware/'src/app_gui_main.cpp').read_text()
def function(signature):
    start=source.index(signature)
    opening=source.index('{',start); depth=1; end=opening+1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[start:end]
head=r'''
#include <cassert>
#include <iostream>
#include <cmath>
#include "pressure.h"
#include "press_turn_policy.h"
using E=KnobPressure::Event;
using S=KnobApp::Screen;
namespace KnobHaptics { inline float hysteresis(float){return .03f;} inline float demand(float e,float,float){return e;} }
namespace KnobLight {enum class Action{Brighter,Dimmer,Toggle};int requests=0;void request(Action){++requests;}}
namespace KnobBackground {void suppressClick(){}}
KnobApp::Model app;KnobPressure::Input pressure;
constexpr float TAU_F=6.28318530718f,PI_F=TAU_F/2,DEG_TO_RAD_F=PI_F/180,CLICK_VOLTAGE=3;
constexpr uint32_t CLICK_HALF_US=5000,STALE_US=5000,HOLD_US=2000;
constexpr int SENSOR_SIGN=-1,DRIVER_DIAG=17;
float angleCache=0,activeCentre=0,angularSpeed=0;
bool latchedStop=false,running=true,consumeGesture=false,suspended=false,heldOutput=false,lost=false;
uint32_t clickAt=0,completedClicks=0,resumedUs=0,startedMs=0,lastGoodUs=0,suspensionUs=0;
uint32_t recoveries=0,heldTicks=0,zeroedTicks=0,clickGapTicks=0;
uint8_t clickStage=0;
const char* problem="test";
struct MockMotor {float value=0;void move(float v){value=v;} void loopFOC(){}}motor;
int digitalRead(int){return 0;}
void logPress(const char*){}
void anchor(){activeCentre=angleCache;}
void changedView(S before){if(before!=app.state.screen)anchor();}
void resetSpring(){}
float springCommand(float x){return x;}
void stop(const char*){latchedStop=true;running=false;clickStage=0;motor.move(0);}
'''
tail=r'''
void arm(){pressure.sampleGesture(0,0,0,true);pressure.sampleGesture(0,0,60,true);}
int main(){
    app.state.screen=S::Home;app.state.selected=1;arm();
    pressure.sampleGesture(25000,25000,100,true);
    auto e=pressure.sampleGesture(25000,25000,150,true);assert(e==E::Down);handleGesture(e);
    assert(clickStage==1 && app.state.selected==1 && app.state.screen==S::Home);
    // Full detent movement must advance motor centre but retain app identity.
    angleCache=TAU_F/8;detentError();
    assert(fabsf(activeCentre-angleCache)<.001f && app.state.selected==1 && pressure.pressed);
    pressure.setStartAllowed(false);pressure.sampleGesture(0,0,200,true);
    e=pressure.sampleGesture(0,0,260,true);assert(e==E::Short);handleGesture(e);
    assert(app.state.screen==S::Value && app.state.selected==1);
    // Normal released navigation remains available; no queued movement replay.
    app.state.screen=S::Home;app.state.selected=1;activeCentre=angleCache=0;
    angleCache=TAU_F/8;detentError();assert(app.state.selected==2);
    // Light: holding causes return Home, without a short-press toggle.
    pressure=KnobPressure::Input();arm();app.state.screen=S::Light;consumeGesture=false;
    pressure.sampleGesture(25000,25000,100,true);handleGesture(pressure.sampleGesture(25000,25000,150,true));
    int prior=KnobLight::requests;handleGesture(pressure.sampleGesture(25000,25000,1050,true));
    assert(app.state.screen==S::Home && consumeGesture && KnobLight::requests==prior);
    int selected=app.state.selected;angleCache+=TAU_F/8;detentError();assert(app.state.selected==selected);
    pressure.sampleGesture(0,0,1100,true);e=pressure.sampleGesture(0,0,1160,true);
    assert(e==E::None && !pressure.pressed); // loop clears consumed gesture after release
    // User can still wake from standby without firing a click.
    app.state.screen=S::Standby;consumeGesture=false;clickStage=0;handleGesture(E::Down);
    assert(app.state.screen==S::Home && consumeGesture && clickStage==0);
    // Click envelope unchanged: 5ms positive + 5ms negative, never extended.
    clickStage=1;float voltage=0;fakeUs=10000;
    assert(clickVoltage(voltage)&&voltage==3);
    fakeUs=15000;assert(clickVoltage(voltage)&&voltage==-3);
    fakeUs=20000;assert(clickVoltage(voltage)&&voltage==0 && clickStage==0);
    // Lost sensor ticks zero click output and keep its original clock.
    pressure.valid=true;pressure.lastData=fakeMs;running=true;latchedStop=false;
    consumeGesture=false;clickStage=2;clickAt=30000;fakeUs=31000;lastGoodUs=30000;
    suspended=false;lost=true;motor.value=3;control(false);
    assert(motor.value==0 && clickStage==2 && clickAt==30000);
    fakeUs=32000;control(true);assert(motor.value==3 && clickAt==30000);
    // Sustained dropout still stops the motor.
    suspended=false;fakeUs=36000;lastGoodUs=30000;control(false);
    assert(latchedStop && !running && motor.value==0);
    std::cout<<"Detent/gesture/click/dropout integration tests passed\n";
}
'''
functions=['float wrapAngle(', 'float positiveAngle(', 'float detentError(', 'bool clickVoltage(', 'void control(', 'void handleGesture(']
with tempfile.TemporaryDirectory() as temp:
    cpp=Path(temp)/'integration.cpp';exe=Path(temp)/'integration'
    cpp.write_text(head+'\n'.join(function(f) for f in functions)+tail)
    includes=[firmware/'docs/tests/press-turn-stubs',firmware/'src/app_gui']
    # Optional fallback for unchanged headers when validating a patch-only tree.
    if os.environ.get('KNOB_BASE_HEADERS'):includes.append(Path(os.environ['KNOB_BASE_HEADERS']))
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',*[arg for p in includes for arg in ('-I',str(p))],str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
