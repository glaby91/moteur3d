#include "m3d_engine.h"

namespace m3d {
struct AxisSample{int i0,i1;float w0,w1;};
std::vector<uint32_t> upscaleBilinear(const std::vector<uint32_t>&src,int sw,int sh,int dw,int dh,unsigned threads){
 std::vector<uint32_t>out(size_t(dw)*dh);std::vector<AxisSample>xs(dw),ys(dh);
 for(int x=0;x<dw;x++){float f=clampf((x+.5f)*sw/dw-.5f,0.f,float(sw-1)),q=std::floor(f);int i0=(int)q,i1=std::min(i0+1,sw-1);float a=f-q;xs[x]={i0,i1,1-a,a};}
 for(int y=0;y<dh;y++){float f=clampf((y+.5f)*sh/dh-.5f,0.f,float(sh-1)),q=std::floor(f);int i0=(int)q,i1=std::min(i0+1,sh-1);float a=f-q;ys[y]={i0,i1,1-a,a};}
 std::atomic<int>next{0};auto work=[&]{for(;;){int by=next.fetch_add(4);if(by>=dh)break;for(int y=by;y<std::min(dh,by+4);y++){const auto Y=ys[y];for(int x=0;x<dw;x++){const auto X=xs[x];uint32_t a=src[size_t(Y.i0)*sw+X.i0],b=src[size_t(Y.i0)*sw+X.i1],c=src[size_t(Y.i1)*sw+X.i0],d=src[size_t(Y.i1)*sw+X.i1],v=0xff000000;for(int shift:{0,8,16}){float p=((a>>shift)&255)*X.w0*Y.w0+((b>>shift)&255)*X.w1*Y.w0+((c>>shift)&255)*X.w0*Y.w1+((d>>shift)&255)*X.w1*Y.w1;v|=(uint32_t)std::lround(p)<<shift;}out[size_t(y)*dw+x]=v;}}}};
 threads=std::max(1u,std::min(threads,(unsigned)dh));if(threads==1)work();else{std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}return out;
}
std::vector<uint32_t> downsample2x(const std::vector<uint32_t>&src,int sw,int sh,unsigned threads){int dw=sw/2,dh=sh/2;std::vector<uint32_t>out(size_t(dw)*dh);std::atomic<int>next{0};auto work=[&]{for(;;){int by=next.fetch_add(8);if(by>=dh)break;for(int y=by;y<std::min(dh,by+8);y++)for(int x=0;x<dw;x++){uint32_t a=src[size_t(y*2)*sw+x*2],b=src[size_t(y*2)*sw+x*2+1],c=src[size_t(y*2+1)*sw+x*2],d=src[size_t(y*2+1)*sw+x*2+1],v=0xff000000;for(int shf:{0,8,16})v|=(uint32_t)((((a>>shf)&255)+((b>>shf)&255)+((c>>shf)&255)+((d>>shf)&255)+2)/4)<<shf;out[size_t(y)*dw+x]=v;}}};threads=std::max(1u,std::min(threads,(unsigned)dh));if(threads==1)work();else{std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}return out;}

std::vector<uint32_t> antialiasEdges4(const std::vector<uint32_t>&src,int w,int h,unsigned threads){
 if(w<3||h<3||src.size()<size_t(w)*h)return src;
 std::vector<uint32_t>out=src;std::atomic<int>next{1};
 auto luminance=[](uint32_t p){return .299f*(p&255)+.587f*((p>>8)&255)+.114f*((p>>16)&255);};
 auto work=[&]{for(;;){int y=next.fetch_add(1);if(y>=h-1)break;for(int x=1;x<w-1;x++){
  size_t i=size_t(y)*w+x;uint32_t c=src[i],n=src[i-w],s=src[i+w],e=src[i+1],west=src[i-1];
  float lc=luminance(c),ln=luminance(n),ls=luminance(s),le=luminance(e),lw=luminance(west);
  float lo=std::min({lc,ln,ls,le,lw}),hi=std::max({lc,ln,ls,le,lw}),range=hi-lo;
  float threshold=std::max(20.f,hi*.15f);if(range<=threshold)continue;
  // Edge pixels are softened toward the mean of their four cardinal neighbours.
  // The contrast gate leaves flat regions and low-contrast texture detail intact.
  constexpr float mix=.45f;uint32_t result=0xff000000;
  for(int shift:{0,8,16}){float center=(c>>shift)&255,neighborMean=(((n>>shift)&255)+((s>>shift)&255)+((e>>shift)&255)+((west>>shift)&255))*.25f;float value=center*(1.f-mix)+neighborMean*mix;result|=(uint32_t)std::lround(value)<<shift;}
  out[i]=result;
 }} };
 threads=std::max(1u,std::min(threads,(unsigned)(h-2)));if(threads==1)work();else{std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}return out;
}

std::vector<uint32_t> bloomGlow(const std::vector<uint32_t>&src,int w,int h,unsigned threads,float strength){
 if(w<1||h<1||src.size()<size_t(w)*h||strength<=0)return src;
 struct RGB{float r=0,g=0,b=0;};
 const size_t count=size_t(w)*h;std::vector<RGB> bright(count),horizontal(count);std::vector<uint32_t>out=src;
 constexpr int radius=5,weightSum=(radius+1)*(radius+1);
 auto parallel=[&](auto&&fn,int rows){std::atomic<int>next{0};auto work=[&]{for(;;){int y0=next.fetch_add(8);if(y0>=rows)break;for(int y=y0;y<std::min(rows,y0+8);y++)fn(y);}};threads=std::max(1u,std::min(threads,(unsigned)rows));if(threads==1)work();else{std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}};
 parallel([&](int y){for(int x=0;x<w;x++){uint32_t p=src[size_t(y)*w+x];bright[size_t(y)*w+x]={std::max(0.f,float((p>>16)&255)-185.f),std::max(0.f,float((p>>8)&255)-185.f),std::max(0.f,float(p&255)-185.f)};}},h);
 parallel([&](int y){for(int x=0;x<w;x++){RGB sum;for(int k=-radius;k<=radius;k++){int sx=std::clamp(x+k,0,w-1),weight=radius+1-std::abs(k);const RGB q=bright[size_t(y)*w+sx];sum.r+=q.r*weight;sum.g+=q.g*weight;sum.b+=q.b*weight;}horizontal[size_t(y)*w+x]={sum.r/weightSum,sum.g/weightSum,sum.b/weightSum};}},h);
 parallel([&](int y){for(int x=0;x<w;x++){RGB sum;for(int k=-radius;k<=radius;k++){int sy=std::clamp(y+k,0,h-1),weight=radius+1-std::abs(k);const RGB q=horizontal[size_t(sy)*w+x];sum.r+=q.r*weight;sum.g+=q.g*weight;sum.b+=q.b*weight;}const RGB q={sum.r/weightSum,sum.g/weightSum,sum.b/weightSum};const uint32_t p=src[size_t(y)*w+x];auto add=[&](int shift,float glow){return std::min(255.f,float((p>>shift)&255)+glow*strength);};out[size_t(y)*w+x]=0xff000000u|(uint32_t(add(16,q.r))<<16)|(uint32_t(add(8,q.g))<<8)|uint32_t(add(0,q.b));}},h);
 return out;
}
}
