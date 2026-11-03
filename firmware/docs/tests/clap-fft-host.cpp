#include "../../src/clap_fft_features.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
    using namespace ClapFFT;
    Analyzer a;int16_t samples[N];
    for(auto& v:samples)v=1234;
    auto f=a.analyze(samples);
    assert(f.rms==0 && f.flatness==0 && f.flux==0);
    for(unsigned frequency : {250U,1000U,4000U}) {
        for(unsigned i=0;i<N;++i)samples[i]=static_cast<int16_t>(1000+10000*sin(2*FFT_PI*frequency*i/RATE));
        a.reset();f=a.analyze(samples);
        assert(fabs(f.rms-7071)<3);
        assert(fabs(f.centroid-frequency)<3);
        assert(fabs(f.low+f.mid+f.high-1)<0.001);
        if(frequency==250)assert(f.low>0.99);
        if(frequency==1000)assert(f.mid>0.99);
        if(frequency==4000)assert(f.high>0.99);
        assert(f.flatness<0.01 && f.flux==0 && f.clipped==0);
        assert(a.analyze(samples).flux<0.00001);
    }
    for(auto& v:samples)v=0;
    samples[N/2]=20000;a.reset();f=a.analyze(samples);
    assert(f.flatness>0.99 && f.crest>15);
    samples[0]=32767;samples[1]=-32768;assert(a.analyze(samples).clipped==2);
    puts("PASS: DC removal, RMS, band energy, centroid, flatness, flux, impulse, clipping");
}

