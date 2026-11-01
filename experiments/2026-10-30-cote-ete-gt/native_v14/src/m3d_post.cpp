#include "m3d_engine.h"

namespace m3d {
struct AxisSample{int i0,i1;float w0,w1;};
std::vector<uint32_t> upscaleBilinear(const std::vector<uint32_t>&src,int sw,int sh,int dw,int dh,unsigned threads){
 std::vector<uint32_t>out(size_t(dw)*dh);std::vector<AxisSample>xs(dw),ys(dh);
 for(int x=0;x<dw;x++){float f=(x+.5f)*sw/dw-.5f,q=std::floor(f);int i0=std::clamp((int)q,0,sw-1),i1=std::min(i0+1,sw-1);float a=clampf(f-q,0,1);xs[x]={i0,i1,1-a,a};}
 for(int y=0;y<dh;y++){float f=(y+.5f)*sh/dh-.5f,q=std::floor(f);int i0=std::clamp((int)q,0,sh-1),i1=std::min(i0+1,sh-1);float a=clampf(f-q,0,1);ys[y]={i0,i1,1-a,a};}
 std::atomic<int>next{0};auto work=[&]{for(;;){int by=next.fetch_add(4);if(by>=dh)break;for(int y=by;y<std::min(dh,by+4);y++){const auto Y=ys[y];for(int x=0;x<dw;x++){const auto X=xs[x];uint32_t a=src[size_t(Y.i0)*sw+X.i0],b=src[size_t(Y.i0)*sw+X.i1],c=src[size_t(Y.i1)*sw+X.i0],d=src[size_t(Y.i1)*sw+X.i1],v=0xff000000;for(int shift:{0,8,16}){float p=((a>>shift)&255)*X.w0*Y.w0+((b>>shift)&255)*X.w1*Y.w0+((c>>shift)&255)*X.w0*Y.w1+((d>>shift)&255)*X.w1*Y.w1;v|=(uint32_t)std::lround(p)<<shift;}out[size_t(y)*dw+x]=v;}}}};
 threads=std::max(1u,std::min(threads,(unsigned)dh));if(threads==1)work();else{std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}return out;
}
std::vector<uint32_t> downsample2x(const std::vector<uint32_t>&src,int sw,int sh,unsigned threads){int dw=sw/2,dh=sh/2;std::vector<uint32_t>out(size_t(dw)*dh);std::atomic<int>next{0};auto work=[&]{for(;;){int by=next.fetch_add(8);if(by>=dh)break;for(int y=by;y<std::min(dh,by+8);y++)for(int x=0;x<dw;x++){uint32_t a=src[size_t(y*2)*sw+x*2],b=src[size_t(y*2)*sw+x*2+1],c=src[size_t(y*2+1)*sw+x*2],d=src[size_t(y*2+1)*sw+x*2+1],v=0xff000000;for(int shf:{0,8,16})v|=(uint32_t)((((a>>shf)&255)+((b>>shf)&255)+((c>>shf)&255)+((d>>shf)&255)+2)/4)<<shf;out[size_t(y)*dw+x]=v;}}};threads=std::max(1u,std::min(threads,(unsigned)dh));if(threads==1)work();else{std::vector<std::thread>pool;pool.reserve(threads-1);for(unsigned i=1;i<threads;i++)pool.emplace_back(work);work();for(auto&t:pool)t.join();}return out;}
}
