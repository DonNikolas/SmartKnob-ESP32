#pragma once
#include <stdint.h>
#include <stdlib.h>
extern uint32_t testNow;
inline uint32_t millis() { return testNow; }
