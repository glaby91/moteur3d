#include "m3d_engine.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

namespace m3d {
bool avx2FmaAvailable() noexcept {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    int regs[4]{};
    __cpuidex(regs, 1, 0);
    constexpr int osxsave = 1 << 27, avx = 1 << 28, fma = 1 << 12;
    if ((regs[2] & (osxsave | avx | fma)) != (osxsave | avx | fma)) return false;
    if ((_xgetbv(0) & 0x6) != 0x6) return false;
    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 5)) != 0;
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#else
    return false;
#endif
}
}

#if !defined(__x86_64__) && !defined(__i386__) && !defined(_M_X64) && !defined(_M_IX86)
namespace m3d {
// The dispatcher reports AVX2 unavailable on ARM. These scalar equivalents
// keep all symbols linkable, even if a caller explicitly requests a packet.
void primaryRays8Avx2(const Camera& c,int y,int x,int w,int h,float ox,float oy,Ray* out) noexcept {
    for(int k=0;k<8;++k){
        float rx=(2.f*(x+k+ox)/w-1.f)*c.tanHalf,ry=(1.f-2.f*(y+oy)/h)*c.tanHalf/c.aspect;
        out[k]={c.eye,norm(c.f+c.r*rx+c.u*ry)};
    }
}
bool intersectTriangles8Avx2(const Scene&,const Ray&,const uint32_t*,float,float*) noexcept {
    // The caller retries the scalar BVH intersection when a packet returns false.
    return false;
}
V accumulatePointLightsAvx2(const Light* lights,size_t count,V p,V normal,float scale) noexcept {
    V sum{0,0,0};
    for(size_t i=0;i<count;++i){const Light& l=lights[i];V q=l.p-p;float d2=dot(q,q);
        if(d2>=l.radius*l.radius||d2<1e-5f)continue;
        float d=std::sqrt(d2),edge=1-d/l.radius,energy=edge*edge*l.intensity*scale,lam=std::max(0.f,dot(normal,q/d));
        sum=sum+l.color*(energy*lam*.27f);
    }
    return sum;
}
V sampleRgb565Avx2(uint16_t a,uint16_t b,uint16_t c,uint16_t d,float w0,float w1,float w2,float w3) noexcept {
    float r=((a>>11)*w0+(b>>11)*w1+(c>>11)*w2+(d>>11)*w3)*255/31;
    float g=((a>>5&63)*w0+(b>>5&63)*w1+(c>>5&63)*w2+(d>>5&63)*w3)*255/63;
    float bl=((a&31)*w0+(b&31)*w1+(c&31)*w2+(d&31)*w3)*255/31;
    return {std::floor(r+.5f),std::floor(g+.5f),std::floor(bl+.5f)};
}
std::vector<uint32_t> antialiasEdges4Avx2(const std::vector<uint32_t>& src,int w,int h,unsigned threads) {
    return antialiasEdges4(src,w,h,threads,false);
}
}
#endif
