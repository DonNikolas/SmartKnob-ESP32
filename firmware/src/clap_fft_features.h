// Spectral measurements only; these features do not classify a sound as a clap.
#pragma once
#include <stdint.h>
#include <math.h>
namespace ClapFFT {
constexpr unsigned N = 256;
constexpr float RATE = 16000.0f;
constexpr float FFT_PI = 3.14159265358979323846f;
struct Features {
    float rms = 0, peak = 0, crest = 0;
    float low = 0, mid = 0, high = 0, centroid = 0, flatness = 0, flux = 0;
    unsigned clipped = 0;
};
class Analyzer {
    float re[N] = {}, im[N] = {}, window[N] = {}, previous[N/2+1] = {};
    bool history = false;
public:
    Analyzer() {
        for (unsigned i=0;i<N;++i) window[i]=0.5f-0.5f*cosf(2*FFT_PI*i/(N-1));
    }
    void reset() { history=false; }
    Features analyze(const int16_t* samples) {
        Features f;
        double sum=0, squares=0;
        for(unsigned i=0;i<N;++i) sum+=samples[i];
        const float mean=static_cast<float>(sum/N);
        for(unsigned i=0;i<N;++i) {
            const float x=samples[i]-mean;
            squares+=static_cast<double>(x)*x;
            f.peak=fmaxf(f.peak,fabsf(x));
            if(samples[i]==32767 || samples[i]==-32768) ++f.clipped;
            re[i]=x*window[i]; im[i]=0;
        }
        f.rms=sqrtf(static_cast<float>(squares/N));
        f.crest=f.rms>0 ? f.peak/f.rms : 0;
        // In-place radix-2 FFT, negative exponent, no amplitude normalisation.
        for(unsigned i=1,j=0;i<N;++i) {
            unsigned bit=N>>1;
            for(;j&bit;bit>>=1) j^=bit;
            j^=bit;
            if(i<j) { float t=re[i];re[i]=re[j];re[j]=t;t=im[i];im[i]=im[j];im[j]=t; }
        }
        for(unsigned len=2;len<=N;len<<=1) {
            const float angle=-2*FFT_PI/len, wr=cosf(angle), wi=sinf(angle);
            for(unsigned start=0;start<N;start+=len) {
                float ur=1,ui=0;
                for(unsigned j=0;j<len/2;++j) {
                    unsigned a=start+j,b=a+len/2;
                    float vr=re[b]*ur-im[b]*ui, vi=re[b]*ui+im[b]*ur;
                    re[b]=re[a]-vr;im[b]=im[a]-vi;re[a]+=vr;im[a]+=vi;
                    float next=ur*wr-ui*wi;ui=ur*wi+ui*wr;ur=next;
                }
            }
        }
        // Bins 2..120: 125..7500 Hz; 62.5-Hz bin spacing.
        // Low <500 Hz, mid 500..<2000 Hz, high >=2000 Hz.
        double energy=0, weighted=0, logs=0;
        for(unsigned k=2;k<=120;++k) {
            float p=re[k]*re[k]+im[k]*im[k]; re[k]=p;
            energy+=p; weighted+=p*(k*RATE/N);
            logs+=log(static_cast<double>(p)+1e-20);
            if(k<8)f.low+=p;else if(k<32)f.mid+=p;else f.high+=p;
        }
        if(energy>1e-12) {
            f.low/=energy;f.mid/=energy;f.high/=energy;
            f.centroid=weighted/energy;
            f.flatness=static_cast<float>(exp(logs/119)/(energy/119));
            for(unsigned k=2;k<=120;++k) {
                float p=re[k]/energy;
                if(history) f.flux+=fmaxf(0,p-previous[k]);
                previous[k]=p;
            }
            history=true;
        } else { history=false; }
        return f;
    }
};
}

