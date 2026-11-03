#pragma once
#include <stdint.h>
#include <stddef.h>
extern uint32_t fakeMs;
inline uint32_t millis() { return fakeMs; }
enum { INPUT, OUTPUT, LOW };
inline void pinMode(int,int) {}
inline void digitalWrite(int,int) {}
