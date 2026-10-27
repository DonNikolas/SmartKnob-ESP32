#pragma once
class HX711 { public:
 bool ready=true; long raw=300000;
 void begin(int,int,int) {}
 bool is_ready() { return ready; }
 long read() { return raw; }
};
