#pragma once
class HX711 {
public:
    long raw = 300000;
    bool ready = true;
    void begin(int, int, int) {}
    bool is_ready() { return ready; }
    long read() { return raw; }
};
