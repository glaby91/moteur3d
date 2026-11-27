#pragma once
// Z-Reflect R001: CPU-only conservative visibility helpers, no GPU dependency.
#include "m3d_engine.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace m3d::zreflect {
struct Bounds{
    V lo{},hi{};bool valid=false,uncertain=false;
    void add(V p){
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)){uncertain=true;return;}
        if(!valid){lo=hi=p;valid=true;return;}
        lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};
        hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};
    }
};
inline bool outside(const Bounds& b,V eye,V normal){
    // Support point of the AABB against an inward-facing plane. Double math
    // and positive padding prevent a false reject from float rounding.
    const double nx=normal.x,ny=normal.y,nz=normal.z;
    const double x=(nx>=0?b.hi.x:b.lo.x),y=(ny>=0?b.hi.y:b.lo.y),z=(nz>=0?b.hi.z:b.lo.z);
    const double d=(x-eye.x)*nx+(y-eye.y)*ny+(z-eye.z)*nz;
    const double scale=1.+(std::abs(x)+std::abs(eye.x))*std::abs(nx)
        +(std::abs(y)+std::abs(eye.y))*std::abs(ny)+(std::abs(z)+std::abs(eye.z))*std::abs(nz);
    return std::isfinite(d)&&std::isfinite(scale)&&d< -1.e-5*scale;
}
inline bool potentiallyVisible(const Bounds& b,const Camera& c){
    if(!b.valid||b.uncertain||!std::isfinite(c.tanHalf)||!std::isfinite(c.aspect)||
       c.tanHalf<=0||c.aspect<=0)return true;
    // Camera basis must match shader projection. Both sides expanded by 1%;
    // no near/far or occlusion rejection, no arbitrary optical threshold.
    const float h=c.tanHalf*1.01f,w=h*c.aspect;
    if(!std::isfinite(w))return true;
    return !(outside(b,c.eye,c.f*w+c.r)||outside(b,c.eye,c.f*w-c.r)||
             outside(b,c.eye,c.f*h+c.u)||outside(b,c.eye,c.f*h-c.u));
}
inline float dielectricF0(float ior){
    if(!std::isfinite(ior)||ior<=0)return .04f;
    const float q=(ior-1.f)/(ior+1.f);return q*q;
}
inline float fresnel(float f0,float cosTheta){
    const float t=1.f-std::clamp(cosTheta,0.f,1.f);
    return f0+(1.f-f0)*t*t*t*t*t;
}
} // namespace m3d::zreflect
