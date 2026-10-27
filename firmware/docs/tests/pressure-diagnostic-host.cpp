#include "../../src/pressure_diagnostic_logic.h"
#include <cassert>
#include <iostream>
using namespace PressureDiagnostic;
int main(){
 Button b;
 assert(!b.sample(0,0,0,0));assert(b.sample(0,0,0,60)==ARMED);
 assert(!b.sample(20000,20000,20000,100));assert(b.sample(20000,20000,20000,150)==DOWN);
 assert(!b.sample(25000,25000,25000,300));
 assert(!b.sample(-7000,-7000,-7000,400));
 assert(b.sample(-7000,-7000,-7000,460)==(RELEASE|SHORT));
 assert(!b.sample(-25000,-25000,-25000,500)); // negative movement cannot press
 assert(!b.sample(25000,25000,25000,600));assert(b.sample(25000,25000,25000,650)==DOWN);
 assert(b.sample(25000,25000,25000,1550)==LONG);
 assert(!b.sample(25000,25000,25000,2000));
 assert(!b.sample(0,0,0,2100));assert(b.sample(0,0,0,2160)==RELEASE);
 b.reset();b.release=6000;
 assert(!b.sample(-7000,-7000,-7000,2200));assert(!b.sample(-7000,-7000,-7000,2300));
 assert(!b.armed); // demonstrates rearming blocked by narrow release band
 b.release=10000;assert(!b.sample(-7000,-7000,-7000,2400));assert(b.sample(-7000,-7000,-7000,2460)==ARMED);
 assert(!b.sample(22000,22000,22000,2500));assert(!b.sample(0,0,0,2520));
 assert(!b.sample(22000,22000,22000,2530));assert(!b.sample(22000,22000,22000,2570));
 assert(b.sample(22000,22000,22000,2580)==DOWN);
 b.reset();assert(!b.sample(0,0,0,0xfffffff0u));assert(b.sample(0,0,0,50)==ARMED);
 std::cout<<"Pressure event tests passed\n";
}
