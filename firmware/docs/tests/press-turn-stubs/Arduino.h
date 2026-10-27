#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
inline uint32_t fakeMs=0,fakeUs=0;
inline uint32_t millis(){return fakeMs;}
inline uint32_t micros(){return fakeUs;}
using std::min;
template<class T> T constrain(T x,T lo,T hi){return std::max(lo,std::min(hi,x));}
