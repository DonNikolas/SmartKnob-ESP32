#pragma once
struct HX711 {
    bool ready=true; long value=0;
    void begin(int,int,int){} bool is_ready(){return ready;} long read(){return value;}
};
