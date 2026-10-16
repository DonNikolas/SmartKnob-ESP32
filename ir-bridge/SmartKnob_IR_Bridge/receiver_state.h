#pragma once
#include <stdint.h>
// The receiver acknowledges only after transmission. A repeated poll response
// must not repeat a relative IR command. A new server boot needs a handshake.
struct ReceiverState {
    uint32_t session=0, executed=0;
    bool accept(uint32_t incomingSession,uint32_t id){
        if(!incomingSession)return false;
        if(session!=incomingSession){session=incomingSession;executed=0;return false;}
        return id && id!=executed;
    }
    void sent(uint32_t id){executed=id;}
};
