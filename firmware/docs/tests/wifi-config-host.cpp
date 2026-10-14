// Native validation tests. No ESP32 hardware or radio is exercised.
#include <cassert>
#include <cstring>
#include "../../src/network_setup/config.h"
int main() {
 using namespace KnobNetwork;
 double v=0;
 assert(coordinate("47.696",90,v) && v>47 && v<48);
 assert(coordinate("-180",180,v));assert(coordinate("90",90,v));
 assert(!coordinate("",90,v));assert(!coordinate("nan",90,v));
 assert(!coordinate("inf",90,v));assert(!coordinate("47,696",90,v));
 assert(!coordinate("47.6junk",90,v));assert(!coordinate("90.001",90,v));
 assert(!coordinate("-180.001",180,v));
 Config c;assert(!valid(c));strcpy(c.ssid,"Test & <WiFi>");assert(valid(c));
 strcpy(c.password,"1234567");assert(!valid(c));strcpy(c.password,"12345678");assert(valid(c));
 memset(c.password,'a',63);c.password[63]=0;assert(valid(c));
 memset(c.password,'a',64);c.password[64]=0;assert(!valid(c));
 c.password[0]=0;c.latitude=NAN;assert(!valid(c));c.latitude=47.696;
 memset(c.ssid,'X',sizeof(c.ssid));assert(!valid(c));
 strcpy(c.ssid,"Test");c.format=0;assert(!valid(c));
}
