"""Validate the real snapshot logger against queue/serial mocks. Requires g++."""
from pathlib import Path
import os, subprocess, tempfile
fw=Path(__file__).resolve().parents[2]
s=(fw/'src/app_gui_main.cpp').read_text()
logger=s[s.index('struct PressLog {'):s.index('\nfloat wrapAngle(')]
head=r'''
#include "pressure.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <iostream>
constexpr float TAU_F=6.2831853f;
constexpr int pdTRUE=1;
constexpr unsigned portMAX_DELAY=0xffffffff;
using QueueHandle_t=void*;
unsigned char queueBytes[2048];size_t queueSize=0;bool queueFull=false;
int xQueueSend(void*,const void* data,unsigned wait){assert(wait==0);if(queueFull)return 0;memcpy(queueBytes,data,queueSize);return 1;}
int xQueueReceive(void*,void*,unsigned){return 0;}
struct SerialType {
    void printf(const char* fmt,...) __attribute__((format(printf,2,3))) {va_list ap;va_start(ap,fmt);vprintf(fmt,ap);va_end(ap);}
}Serial;
KnobApp::Model app;
KnobPressure::Input pressure;
float angleCache=0,activeCentre=0,angularSpeed=0,springOutput=0;
unsigned clickStage=0;
bool running=true;
uint32_t completedClicks=0,cancelledClicks=0,trackLost=0;
'''
tail=r'''
int main(){
    queueSize=sizeof(PressLog);assert(queueSize<sizeof(queueBytes));pressLogQueue=queueBytes;
    testMs=1000;pressure.lastData=987;pressure.valid=true;pressure.lastRaw=315000;
    pressure.offset=300000;pressure.observedDelta=15000;pressure.filteredDelta=14000;
    pressure.intervalMin=12000;pressure.intervalMax=40000;pressure.intervalSamples=40;pressure.intervalValid=true;
    angularSpeed=1.5f;serviceDiagnostic();PressLog captured;memcpy(&captured,queueBytes,sizeof(captured));
    assert(captured.raw==315000 && captured.zero==300000 && captured.delta==15000);
    assert(captured.age==13 && captured.speed==1.5f && captured.samples==40 && captured.high==40000);
    assert(pressure.intervalSamples==0 && !pressure.intervalValid && pressure.offset==300000);
    queueFull=true;logPress("TEST DROP");assert(droppedPressLogs==1);
    queueFull=false;diagnosticPhase=2;logPress("MARK");memcpy(&captured,queueBytes,sizeof(captured));
    assert(captured.phase==2 && captured.dropped==1 && captured.samples==0);
    std::cout<<"Snapshot, interval reset, phase and nonblocking queue tests passed\n";
}
'''
with tempfile.TemporaryDirectory() as td:
    source=Path(td)/'test.cpp';binary=Path(td)/'test';source.write_text(head+logger+tail)
    includes=[fw/'docs/tests/drift-stubs',fw/'src/app_gui']
    if os.environ.get('KNOB_BASE_HEADERS'):includes.append(Path(os.environ['KNOB_BASE_HEADERS']))
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wformat=2',*[a for p in includes for a in ('-I',str(p))],str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
