#pragma once
struct FakeWire {
    bool begin(int,int,int) { return true; }
    void setTimeOut(int) {}
};
extern FakeWire Wire;
