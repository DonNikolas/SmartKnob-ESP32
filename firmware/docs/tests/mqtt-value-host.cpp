#include "../../src/app_gui/mqtt_value.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
int main() {
    double v=99;
    for(const char* s:{"23.5","-12.25","0","1e2","  +5.0\n"})
        assert(KnobMQTT::parseValue(reinterpret_cast<const uint8_t*>(s),strlen(s),v));
    for(const char* s:{""," ","nan","inf","1e999","0x10","true","null","23C","1 2","1e","{}","\"23\"","1000000","-1000000"})
        assert(!KnobMQTT::parseValue(reinterpret_cast<const uint8_t*>(s),strlen(s),v));
    uint8_t nul[]={'1',0,'2'};assert(!KnobMQTT::parseValue(nul,3,v));
    uint8_t huge[49]={};assert(!KnobMQTT::parseValue(huge,49,v));
    assert(KnobMQTT::fresh(100,90,10));assert(!KnobMQTT::fresh(101,90,10));
    assert(KnobMQTT::fresh(5,0xfffffff0U,21));
    puts("PASS: numeric payload validation, range, embedded NUL, expiry and timer wrap");
}
