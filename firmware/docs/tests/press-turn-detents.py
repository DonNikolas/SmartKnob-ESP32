"""Compile the actual detentError() function with host adapters.
This tests navigation coupling, not the PWM driver or ESP32 timing.
Run from any directory: python firmware/docs/tests/press-turn-detents.py
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
main = (root/'src/app_gui_main.cpp').read_text()
function = main[main.index('float detentError() {'):main.index('uint8_t crc6(')]
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp)/'detents.cpp'
    source.write_text('''
#include <cassert>
#include <cmath>
#include <cstdio>
#include "pressure.h"
uint32_t testMillis=0;
constexpr float TAU_F=6.28318530718f, PI_F=TAU_F/2, DEG_TO_RAD_F=PI_F/180;
KnobApp::Model app;
KnobPressure::Input pressure;
float activeCentre=0, angleCache=0;
bool consumeGesture=false;
float wrapAngle(float a){while(a>PI_F)a-=TAU_F;while(a<-PI_F)a+=TAU_F;return a;}
float positiveAngle(float a){a=fmodf(a,TAU_F);return a<0?a+TAU_F:a;}
void changedView(KnobApp::Screen before){if(before!=app.state.screen)activeCentre=angleCache;}
namespace KnobLight {
enum class Action{Brighter,Dimmer};
int requests=0;
void request(Action){++requests;}
}
''' + function + '''
int main(){
    app.state.screen=KnobApp::Screen::Home;app.state.selected=0;
    pressure.sampleGesture(0,0,0,true);pressure.sampleGesture(0,0,151,true);
    pressure.sampleGesture(25000,25000,200,true);
    assert(pressure.sampleGesture(25000,25000,251,true)==KnobPressure::Event::Down);
    consumeGesture=true;angleCache=50*DEG_TO_RAD_F;testMillis=300;
    float error=detentError();
    assert(app.state.selected==1 && !pressure.pressed && !consumeGesture);
    assert(fabsf(error)<10*DEG_TO_RAD_F);
    assert(pressure.sampleGesture(0,0,500,true)==KnobPressure::Event::None);
    // Reverse through zero: navigation still advances to the correct app.
    angleCache=0;testMillis=600;detentError();assert(app.state.selected==0);
    // Multiple detents in one update remain bounded around the current rotor.
    angleCache=150*DEG_TO_RAD_F;testMillis=700;error=detentError();
    assert(app.state.selected==3 && fabsf(error)<25*DEG_TO_RAD_F);
    // Genuine value end stops must not be removed by this fix.
    app.state.screen=KnobApp::Screen::Value;app.state.selected=1;app.state.draft=100;
    activeCentre=0;angleCache=50*DEG_TO_RAD_F;
    error=detentError();assert(app.state.draft==100 && fabsf(error)>45*DEG_TO_RAD_F);
    // Relative light commands remain one per crossed detent.
    app.state.screen=KnobApp::Screen::Light;activeCentre=0;angleCache=65*DEG_TO_RAD_F;
    detentError();assert(KnobLight::requests==2);
    puts("PASS: actual detent function advances while pressed, cancels gesture, preserves end stops/light steps");
}
''')
    exe=Path(temp)/'detents'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',
                    '-I',str(root/'docs/tests/press-turn-stubs'),
                    '-I',str(root/'src/app_gui'),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
