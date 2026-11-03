#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdio>
#include "../../src/clap_fft_detector.h"
int main(int argc,char** argv){
 for(int a=1;a<argc;++a){
  std::ifstream file(argv[a]);assert(file);std::string line;ClapFFT::Detector detector;unsigned candidates=0,pairs=0,rejected=0,rows=0;
  while(std::getline(file,line)){
   std::stringstream s(line);std::string cell;std::vector<double> v;
   try{while(std::getline(s,cell,','))v.push_back(std::stod(cell));}catch(...){continue;}
   if(v.size()!=22)continue;
   ++rows;
   ClapFFT::Features f;f.rms=v[5];f.peak=v[7];f.crest=v[9];f.low=v[10];f.mid=v[11];f.high=v[12];f.clipped=v[19];
   if(v[4]){detector.reset();continue;}
   auto r=detector.update(f,v[8],v[17],uint32_t(v[0])*16U);
   candidates+=r.event==1;pairs+=r.event==2;rejected+=r.event==3;
  }
  if(std::string(argv[a]).find("log-klatschen")!=std::string::npos)assert(pairs==5);
  if(std::string(argv[a]).find("log-reden")!=std::string::npos || std::string(argv[a]).find("log-mechanisch")!=std::string::npos)assert(pairs==0);
  printf("%s: rows=%u candidates=%u pairs=%u rejected=%u\n",argv[a],rows,candidates,pairs,rejected);
 }
 // Synthetic sustained noise, clipped impulse and sample gap cannot form pairs.
 ClapFFT::Features hit;hit.rms=4000;hit.crest=4;hit.low=.1;hit.mid=.7;hit.high=.2;
 ClapFFT::Features quiet;quiet.rms=10;
 ClapFFT::Detector d;
 for(unsigned t=0;t<=128;t+=16){auto r=d.update(hit,10,5,t);assert(r.event!=2);if(t==128)assert(r.event==3);}
 d.reset();d.update(hit,10,5,0);d.update(quiet,10,.1,16);assert(d.update(quiet,10,1,32).event==1);
 for(unsigned t=48;t<320;t+=16)d.update(quiet,10,1,t);
 d.update(hit,10,5,320);d.update(quiet,10,.1,336);assert(d.update(quiet,10,1,352).event==2);
 assert(d.update(hit,10,5,368).event==0);
 d.reset();d.update(hit,10,5,0);assert(d.update(quiet,10,.1,32).event==0);assert(d.update(quiet,10,1,48).event==0);
 d.reset();hit.clipped=1;assert(d.update(hit,10,5,0).event==0);
 puts("Synthetic temporal checks passed.");
}
