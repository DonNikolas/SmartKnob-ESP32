#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <map>
#include "../../src/app_gui/clap_stream.h"
using namespace KnobClap;
ClapFFT::Features noise() {
    ClapFFT::Features f; f.rms = 4; f.peak = 12; f.crest = 3;
    f.low = .1f; f.mid = .7f; f.high = .2f; return f;
}
ClapFFT::Features hit() {
    auto f = noise(); f.rms = 3000; f.peak = 12000; f.crest = 4; return f;
}
void quiet(Stream& s, unsigned frames) { while (frames--) assert(s.update(noise(), false).event == 0); }
void ready(Stream& s) { quiet(s, 145); assert(s.state == State::Ready); }
unsigned clap(Stream& s) {
    assert(s.update(hit(), false).event == 0);
    assert(s.update(noise(), false).event == 0);
    return s.update(noise(), false).event;
}
void unitTests() {
    Stream s; ready(s);
    assert(clap(s) == 1); quiet(s, 13); assert(clap(s) == 2); // 256 ms apart
    assert(clap(s) == 0); quiet(s, 45); // cooldown
    assert(clap(s) == 1); s.cancelPair(); quiet(s, 13); assert(clap(s) == 1);
    s.reset(); ready(s);
    for (unsigned i=0; i<30; ++i) assert(s.update(hit(), true).event == 0);
    quiet(s, 2); assert(clap(s) == 1); // Click cannot seed a pair.
    s.reset(); ready(s); assert(clap(s) == 1);
    s.reset(); ready(s); assert(clap(s) == 1); // Gap cannot bridge two claps.
    ClapFFT::Features flat;
    for (int i=0;i<32;++i) assert(s.update(flat,false).event == 0);
    assert(s.state == State::NoSignal);
    assert(s.update(hit(),false).event == 0); // Resume transient is ignored.
    assert(s.state == State::Warming); ready(s);
    auto tap = hit(); tap.low=.8f; tap.mid=.15f; tap.high=.05f;
    for (int i=0;i<30;++i) { assert(s.update(tap,false).event==0); quiet(s,15); }
    auto clipped = hit(); clipped.clipped=1;
    assert(s.update(clipped,false).event==0);
    ActionGate gate;
    assert(gate.take(1,100,120,true,false));
    assert(!gate.take(1,100,121,true,false)); // Exactly once
    assert(!gate.take(2,200,201,false,false)); // Stopped/boot
    assert(!gate.take(2,200,202,true,false)); // No delayed replay
    assert(!gate.take(3,300,301,true,true)); // Local click
    assert(!gate.take(4,400,601,true,false)); // Expired
    assert(!gate.take(6,700,701,true,false)); // Lost UI interval
    assert(gate.take(7,0xfffffff0u,0x20u,true,false)); // millis wrap
    std::cout << "PASS stream, suppression, signal recovery, stale/duplicate action gates\n";
}
void replay(const char* path) {
    std::ifstream input(path); assert(input.good()); Stream stream;
    std::string line; unsigned expected=0,actual=0,rows=0;
    std::map<unsigned,unsigned> phases;
    while(std::getline(input,line)) {
        if(line.find("# DOUBLE_CLAP ")==0) { ++expected; continue; }
        if(line.empty() || line[0]=='#') continue;
        std::vector<double> v; std::stringstream ss(line); std::string token;
        try { while(std::getline(ss,token,','))v.push_back(std::stod(token)); } catch(...) { continue; }
        if(v.size()!=22)continue;
        ClapFFT::Features f; f.rms=v[5]; f.peak=v[7]; f.crest=v[9];
        f.low=v[10]; f.mid=v[11]; f.high=v[12]; f.clipped=unsigned(v[19]);
        if(stream.update(f,false).event==2) { ++actual; ++phases[unsigned(v[2])]; }
        ++rows;
    }
    assert(rows); assert(actual==expected);
    std::cout << "PASS replay " << path << ": " << actual << " pairs, " << rows << " blocks";
    for(const auto& p:phases)std::cout << " | phase " << p.first << ": " << p.second;
    std::cout << "\n";
}
int main(int argc,char** argv) {
    unitTests(); for(int i=1;i<argc;++i)replay(argv[i]);
}
