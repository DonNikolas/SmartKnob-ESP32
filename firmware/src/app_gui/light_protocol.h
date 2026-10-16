// Generated with ChatGPT assistance. Pure bounded command state machine.
#pragma once
#include <stdint.h>
namespace KnobLight {
enum class Action : uint8_t { Toggle, Brighter, Dimmer };
enum class Command : uint8_t { None, On, Off, Brighter, Dimmer };
inline const char* name(Command c) {
    switch(c){case Command::On:return "on";case Command::Off:return "off";
    case Command::Brighter:return "up";case Command::Dimmer:return "down";default:return "none";}
}
struct Item { uint32_t id=0, at=0; Command command=Command::None; };
class Protocol {
public:
    static constexpr unsigned CAPACITY=8;
    uint32_t boot=0, lastPoll=0, nextId=0, sent=0, dropped=0;
    bool connected=false, requestedOn=false, powerKnown=false;
    Command lastSent=Command::None;
    Item items[CAPACITY]; unsigned count=0;
    bool online(uint32_t now) const { return connected && now-lastPoll<1500; }
    void clear(){ if(count){dropped+=count;powerKnown=false;} count=0; }
    void expire(uint32_t now){
        // Discard the whole pending sequence if its head expires. Never replay
        // the tail of a relative brightness gesture after a communication gap.
        if(count && (now-items[0].at>=1200 || !online(now)))clear();
        if(!online(now))powerKnown=false;
    }
    bool request(Action a,uint32_t now){
        expire(now);
        if(!online(now)||count==CAPACITY){++dropped;return false;}
        Command c=a==Action::Brighter?Command::Brighter:a==Action::Dimmer?Command::Dimmer:
            (powerKnown && requestedOn?Command::Off:Command::On);
        if(a==Action::Toggle){requestedOn=c==Command::On;powerKnown=true;}
        Item item;if(++nextId==0)++nextId;item.id=nextId;item.at=now;item.command=c;items[count++]=item;return true;
    }
    Item poll(uint32_t receiver,uint32_t ack,uint32_t now){
        expire(now);
        if(!connected||receiver!=boot){clear();powerKnown=false;boot=receiver;}
        connected=true;lastPoll=now;
        if(count && ack==items[0].id){
            lastSent=items[0].command;++sent;
            for(unsigned i=1;i<count;++i)items[i-1]=items[i];--count;
        }
        return count?items[0]:Item{};
    }
};
}
