#pragma once
struct HX711 {bool ready=true;long raw=0;void begin(int,int,int){}bool is_ready(){return ready;}long read(){return raw;}};
