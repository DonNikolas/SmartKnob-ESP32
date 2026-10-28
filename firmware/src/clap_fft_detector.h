// Experimental rules fitted to the supplied recordings, not a trained classifier.
#pragma once
#include "clap_fft_features.h"
namespace ClapFFT {
struct Detection { unsigned event=0; uint32_t duration=0,gap=0; }; // 1 clap, 2 pair, 3 rejected
class Detector {
    bool active=false, waiting=false, locked=false, haveTick=false;
    uint32_t onset=0, first=0, lockAt=0, lastTick=0;
    float peak=0;
    unsigned quietFrames=0;
public:
    void reset(){active=waiting=locked=haveTick=false;quietFrames=0;peak=0;}
    Detection update(const Features& f,float background,float rise,uint32_t now){
        Detection result;
        // A missing block breaks temporal evidence. Time is the audio sample clock.
        if(haveTick && now-lastTick!=16)reset();
        haveTick=true;lastTick=now;
        if(waiting && !active && now-first>600)waiting=false;
        if(locked){if(now-lockAt<700)return result;locked=false;}
        if(active){
            peak=fmaxf(peak,f.rms);
            uint32_t duration=now-onset;
            if(f.clipped || !isfinite(f.rms)){
                active=false;waiting=false;result.event=3;result.duration=duration;return result;
            }
            // Require a rapid decay lasting two blocks, rather than an isolated peak.
            if(f.rms<peak*0.45f)++quietFrames;else quietFrames=0;
            if(duration>112){active=false;waiting=false;result.event=3;result.duration=duration;return result;}
            if(quietFrames>=2){
                active=false;result.event=1;result.duration=duration;
                if(waiting){
                    result.gap=onset-first;
                    if(result.gap>=160 && result.gap<=600){
                        result.event=2;waiting=false;locked=true;lockAt=now;return result;
                    }
                    // Too-close impacts are not a new first clap.
                    if(result.gap<160){waiting=false;result.event=3;return result;}
                }
                first=onset;waiting=true;
            }
            return result;
        }
        const float level=fmaxf(1500.0f,6.0f*background);
        if(!f.clipped && isfinite(f.rms) && f.rms>=level && rise>=4.0f &&
           f.crest>=3.0f && f.low<=0.25f && f.mid>=0.50f && f.high<=0.40f){
            active=true;onset=now;peak=f.rms;quietFrames=0;
        }
        return result;
    }
};
}
