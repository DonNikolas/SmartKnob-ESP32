// Only the HX711/Arduino adapter remains C++. Decisions live in pressure_logic.c.
#pragma once
#include <Arduino.h>
#include <HX711.h>
#include "pressure_logic.h"
namespace KnobPressure {
enum class Event { None, Down, Short, Long };
// Inheritance exposes the existing diagnostic fields without changing GUI callers.
class Input : public PressureState {
public:
    HX711 adc;
    Input() { pressure_init(this); }
    static constexpr uint32_t TIMEOUT_MS = PRESSURE_TIMEOUT_MS;
    static constexpr uint32_t FILTER_SIZE = PRESSURE_FILTER_SIZE;
    static constexpr long PRESS_COUNTS = PRESSURE_PRESS_COUNTS;
    static constexpr long RELEASE_COUNTS = PRESSURE_RELEASE_COUNTS;
    static constexpr uint32_t PRESS_SIGN = PRESSURE_PRESS_SIGN;
    static constexpr uint32_t PRESS_MS = PRESSURE_PRESS_MS;
    static constexpr uint32_t RELEASE_MS = PRESSURE_RELEASE_MS;
    static constexpr uint32_t REARM_MS = PRESSURE_REARM_MS;
    static constexpr uint32_t LONG_MS = PRESSURE_LONG_MS;

    void begin(){adc.begin(12,13,128);lastData=millis();}
    void tare(){pressure_tare(this,millis());}
    void afterClick(){pressure_after_click(this,millis());}
    void resetGesture(){pressure_reset_gesture(this);}
    void clearInterval(){pressure_clear_interval(this);}
    void setStartAllowed(bool allowed){pressure_set_start_allowed(this,allowed);}
    bool isBlocked() const{return pressure_is_blocked(this);}
    bool releasedReady(uint32_t now) const{return pressure_released_ready(this,now);}
    const char* diagnosticState(uint32_t now) const{return pressure_diagnostic_state(this,now);}
    Event sampleGesture(long filtered,long instant,uint32_t now,bool enabled){
        return static_cast<Event>(pressure_sample_gesture(this,filtered,instant,now,enabled));
    }
    Event poll(bool gestures){
        const uint32_t now=millis();
        pressure_precheck(this,now);
        if(!adc.is_ready())return static_cast<Event>(pressure_poll(this,now,false,0,now,gestures));
        const long raw=adc.read();
        return static_cast<Event>(pressure_poll(this,now,true,raw,millis(),gestures));
    }
};
}
