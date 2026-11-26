#pragma once
// LumaTrace adaptive budget 001: exact, conservative group-level 0/1 only.
#include "m3d_lumatrace_cpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace m3d::lumatrace_adaptive {
constexpr uint32_t groupSize=256, maskWords=512, maxGroups=maskWords*32;
using Mask=std::array<uint32_t,maskWords>;
struct Bounds {
    double lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    bool valid=true;
    void add(V v){
        const double a[3]={v.x,v.y,v.z};
        for(unsigned k=0;k<3;++k){
            if(!std::isfinite(a[k])||std::abs(a[k])>1.e15){valid=false;continue;}
            lo[k]=std::min(lo[k],a[k]);hi[k]=std::max(hi[k],a[k]);
        }
    }
};
inline bool mayContribute(const Bounds& b,const float* light,const float* color){
    if(!b.valid)return true;
    for(unsigned k=0;k<4;++k)if(!std::isfinite(light[k])||!std::isfinite(color[k]))return true;
    if(color[3]==0.f||(color[0]==0.f&&color[1]==0.f&&color[2]==0.f))return false;
    double d2=0.,scale=1.;
    for(unsigned k=0;k<3;++k){
        if(!std::isfinite(b.lo[k])||!std::isfinite(b.hi[k])||std::abs(light[k])>1.e15)return true;
        const double d=std::max({0.,b.lo[k]-light[k],double(light[k])-b.hi[k]});
        d2+=d*d;scale=std::max({scale,std::abs(b.lo[k]),std::abs(b.hi[k]),std::abs(double(light[k]))});
    }
    // Bounds enclose the three original vertices, hence their centroid.
    // Pad for float32 centroid, subtraction and length rounding on GPU.
    const double radius=std::max(.05,double(light[3]));
    const double padded=radius+1.e-5*(scale+radius)+1.e-6;
    return d2<=padded*padded;
}
inline std::vector<Bounds> prepare(const Scene& scene,const LumaTraceCpuScene& cpu){
    if(!cpu.triangleCount||cpu.triangleCount>maxGroups*groupSize||cpu.objectToTriangle.size()!=scene.obj.size())return {};
    std::vector<Bounds> bounds((cpu.triangleCount+groupSize-1)/groupSize);
    for(size_t i=0;i<scene.obj.size();++i){
        const uint32_t id=cpu.objectToTriangle[i];
        if(id==0xffffffffu)continue;
        if(id>=cpu.triangleCount)return {};
        auto& b=bounds[id/groupSize];const Obj& o=scene.obj[i];
        b.add(o.a);b.add(o.b);b.add(o.c);
    }
    return bounds;
}
inline uint32_t classify(const std::vector<Bounds>& bounds,uint32_t count,
                         const float (&lights)[64][4],const float (&colors)[64][4],
                         uint32_t lightCount,Mask& mask){
    mask.fill(0);uint32_t zero=0;
    if(bounds.size()!=(count+groupSize-1)/groupSize||bounds.size()>maxGroups||lightCount>64)return 0;
    for(uint32_t g=0;g<bounds.size();++g){
        bool active=!bounds[g].valid;
        for(uint32_t i=0;i<lightCount&&!active;++i)active=mayContribute(bounds[g],lights[i],colors[i]);
        if(!active){mask[g>>5]|=1u<<(g&31);zero+=std::min(groupSize,count-g*groupSize);}
    }
    return zero;
}
} // namespace m3d::lumatrace_adaptive
