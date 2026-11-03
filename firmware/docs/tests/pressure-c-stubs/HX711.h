#pragma once
extern bool testReady;
extern long testRaw;
extern unsigned testReadDelay;
extern uint32_t testNow;
class HX711 {
public:
 void begin(int,int,int){}
 bool is_ready(){return testReady;}
 long read(){testNow+=testReadDelay;return testRaw;}
};
