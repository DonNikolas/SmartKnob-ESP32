#pragma once
#include <stdint.h>
#include <stdlib.h>
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
