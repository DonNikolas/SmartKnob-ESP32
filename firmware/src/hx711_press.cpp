// SmartKnob pressure diagnostic v2. Created with ChatGPT assistance.
// MOTOR OFF. No movement veto, motor pulse, GUI or persistent setting changes.
#include <Arduino.h>
#include <HX711.h>
#include <Adafruit_NeoPixel.h>
#include "pressure_diagnostic_logic.h"
using namespace PressureDiagnostic;
HX711 adc;
Adafruit_NeoPixel ring(36,11,NEO_GRB+NEO_KHZ800);
Button button;
constexpr uint8_t motorPins[]={4,7,5,16,6,15};
long values[8]={},offset=0,raw=0,delta=0,filtered=0;
long zeroLow=0,zeroHigh=0,minimum=0,maximum=0;
int64_t sum=0,zeroSum=0;
unsigned window=4,filterIndex=0,count=0,zeroCount=0,direction=1;
uint32_t lastData=0,reportAt=0,zeroAt=0,downs=0,shorts=0,longs=0,releases=0,samples=0;
uint32_t maxGap=0;
bool valid=false,zeroing=false,stopped=false,rawRelease=true;
const char* fault="NOT TARED";
void colour(uint32_t c) { ring.fill(c); ring.show(); }
void resetFilter() { sum=0;filterIndex=count=0; for(auto &v:values)v=0; }
void resetDetection() { button.reset(); resetFilter(); colour(0); }
void fail(const char* why) {
    if(valid || zeroing || fault!=why) Serial.printf("FAULT: %s | send t after checking\n",why);
    fault=why;valid=false;zeroing=false;resetDetection();
}
void settings() {
    Serial.printf("SETTINGS | Press: %ld | Release: %ld | Filter: %u | Release source: %s | Direction: %s\n",
      button.press,button.release,window,rawRelease?"RAW":"FILTERED",direction==1?"POSITIVE":"NEGATIVE");
}
void help() {
    Serial.println("\n=== PRESSURE DIAGNOSTIC v2 - MOTOR OFF ===");
    Serial.println("t tare untouched assembly (32 samples); x stop detection; i help/settings");
    Serial.println("+/- press threshold +/-1000; ]/[ release threshold +/-1000");
    Serial.println("4/8 filter samples; r raw release; f filtered release; p reverse pressure polarity");
    Serial.println("m mark test phase; c clear counters/range. Settings are RAM-only.");
    Serial.println("DOWN = LED white immediately; RELEASE = LED off; SHORT/LONG are separate events.");
    Serial.println("Hold 900ms => LONG once. Release confirmation 60ms; press confirmation 50ms.");
    Serial.println("No automatic tare, no baseline tracking, no rotation rejection in this test.");
    settings();
}
void command(char c) {
    bool change=false;
    switch(c) {
    case 't':
      stopped=false;valid=false;zeroing=true;zeroSum=zeroCount=0;zeroAt=millis();
      delta=filtered=0;resetDetection(); Serial.println("ZERO: do not touch or turn the assembled knob."); break;
    case 'x': stopped=true;zeroing=false;resetDetection();Serial.println("STOPPED: t to restart/tare.");break;
    case '+': if(button.press<1000000){button.press+=1000;change=true;}break;
    case '-': if(button.press>button.release+1000){button.press-=1000;change=true;}break;
    case ']': if(button.release+1000<button.press){button.release+=1000;change=true;}break;
    case '[': if(button.release>1000){button.release-=1000;change=true;}break;
    case '4': window=4;change=true;break;
    case '8': window=8;change=true;break;
    case 'r': rawRelease=true;change=true;break;
    case 'f': rawRelease=false;change=true;break;
    case 'p': direction=direction==1?0:1;change=true;break;
    case 'c': downs=shorts=longs=releases=0;minimum=maximum=0;Serial.println("COUNTERS CLEARED");break;
    case 'm': Serial.printf("MARK | Time: %lu ms\n",(unsigned long)millis());break;
    case 'i': help();break;
    default: break;
    }
    if(change){resetDetection();Serial.println("CONFIG CHANGED: release knob to rearm.");settings();}
}
void setup() {
    for(auto pin:motorPins){pinMode(pin,OUTPUT);digitalWrite(pin,LOW);}
    pinMode(9,OUTPUT);digitalWrite(9,LOW);
    Serial.begin(115200);delay(2500);ring.begin();ring.setBrightness(12);colour(0);
    adc.begin(12,13,128);adc.power_down();delay(1);adc.power_up();delay(500);
    lastData=millis();help();
}
void loop() {
    // Bound command handling so serial input cannot starve sampling.
    for(unsigned n=0;n<16 && Serial.available();++n)command(Serial.read());
    uint32_t now=millis();
    if(zeroing && now-zeroAt>6000)fail("TARE TIMEOUT");
    if(adc.is_ready()) {
      raw=adc.read();now=millis();
      uint32_t gap=now-lastData;if(gap>maxGap)maxGap=gap;
      lastData=now;++samples;
      if(raw<=-8388608L || raw>=8388607L)fail("ADC SATURATED");
      else if(zeroing) {
        if(!zeroCount)zeroLow=zeroHigh=raw;
        if(raw<zeroLow)zeroLow=raw;
        if(raw>zeroHigh)zeroHigh=raw;
        zeroSum+=raw;++zeroCount;
        if(zeroCount==32) {
          if(zeroHigh-zeroLow>5000)fail("TARE UNSTABLE (span >5000)");
          else {offset=long(zeroSum/32);valid=true;zeroing=false;fault="OK";
            minimum=maximum=0;resetDetection();
            Serial.printf("ZERO SET | Offset: %ld | Span: %ld\n",offset,zeroHigh-zeroLow);}
        }
      } else if(valid && !stopped) {
        delta=(raw-offset)*(direction==1?1:-1);
        if(delta<minimum)minimum=delta;
        if(delta>maximum)maximum=delta;
        sum-=values[filterIndex];values[filterIndex]=delta;sum+=delta;filterIndex=(filterIndex+1)%window;
        if(count<window)++count;
        filtered=long(sum/count);
        if(count==window) {
          const unsigned ev=button.sample(filtered,delta,rawRelease?delta:filtered,now);
          if(ev & ARMED)Serial.printf("ARMED | Time: %lu\n",(unsigned long)now);
          if(ev & DOWN){++downs;colour(ring.Color(255,255,255));}
          if(ev & RELEASE){++releases;colour(0);}
          if(ev & SHORT)++shorts;
          if(ev & LONG)++longs;
          if(ev & (DOWN|RELEASE|SHORT|LONG))Serial.printf(
            "EVENT | Time: %lu | %s%s%s%s | Raw delta: %ld | Filtered delta: %ld | Held ms: %lu\n",
            (unsigned long)now,ev&DOWN?"DOWN ":"",ev&RELEASE?"RELEASE ":"",
            ev&SHORT?"SHORT ":"",ev&LONG?"LONG ":"",delta,filtered,(unsigned long)(now-button.downAt));
        }
      }
    } else if(now-lastData>=2000)fail("ADC TIMEOUT");
    if(now-reportAt>=250) {
      Serial.printf("Time: %lu | Raw: %ld | Delta: %ld | Filtered: %ld | State: %s | P/R: %ld/%ld | D/S/L/R: %lu/%lu/%lu/%lu | Range: %ld..%ld | Samples: %lu | MaxGap: %lu ms\n",
        (unsigned long)now,raw,delta,filtered,stopped?"STOPPED":zeroing?"TARING":!valid?fault:count<window?"FILTER WARMUP":button.held?"HELD":button.armed?"ARMED":"WAIT RELEASE",
        button.press,button.release,(unsigned long)downs,(unsigned long)shorts,(unsigned long)longs,(unsigned long)releases,
        minimum,maximum,(unsigned long)samples,(unsigned long)maxGap);
      samples=0;maxGap=0;reportAt=now;
    }
    delay(1);
}
