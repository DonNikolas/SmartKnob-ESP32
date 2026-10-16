#include <cassert>
#include <cstdio>
#include "../../../ir-bridge/SmartKnob_IR_Bridge/receiver_state.h"
int main(){
 ReceiverState r;assert(!r.accept(0,1));assert(!r.accept(55,1));
 assert(r.accept(55,1));r.sent(1);
 for(int i=0;i<10;++i)assert(!r.accept(55,1));
 assert(r.accept(55,2));r.sent(2);assert(!r.accept(55,2));
 assert(!r.accept(56,1));assert(r.accept(56,1));r.sent(1);
 assert(!r.accept(56,0));assert(!r.accept(56,1));
 puts("PASS: receiver handshake and duplicate suppression after lost ACK");
}
