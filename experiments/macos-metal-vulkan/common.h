#pragma once
#include <cstdint>
#include <cstdio>
#include <chrono>
constexpr unsigned W=256,H=256,N=W*H;
inline uint32_t expected(unsigned x,unsigned y){
 return x|(y<<8)|(((x/16+y/16)&1)*255u<<16)|0xff000000u;
}
inline int verify(const uint32_t* data,const char* file){
 unsigned errors=0;
 FILE* f=fopen(file,"wb");if(!f)return 2;
 fprintf(f,"P6\n%u %u\n255\n",W,H);
 for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
  auto p=data[y*W+x];errors+=p!=expected(x,y);
  unsigned char rgb[3]={(unsigned char)p,(unsigned char)(p>>8),(unsigned char)(p>>16)};
  fwrite(rgb,1,3,f);
 }
 fclose(f);printf("pixels=%u mismatches=%u\n",N,errors);return errors?3:0;
}
using Clock=std::chrono::steady_clock;
inline double ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
