#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Conservative camera-space plane tests, independent of D3D11 and scene types.
// Invalid inputs fail open. No approximation of visible geometry.
namespace m3d::dx11::r001 {
struct Plane { double x,y,z,d; };
class Frustum {
    std::array<Plane,6> planes_{};
    double ex_=0,ey_=0,ez_=0;
    bool valid_=false;
public:
    template<class Camera> explicit Frustum(const Camera& c) noexcept {
        ex_=c.eye.x;ey_=c.eye.y;ez_=c.eye.z;
        const double h=c.tanHalf,v=h/c.aspect;
        const auto& f=c.f;const auto& r=c.r;const auto& u=c.u;
        if(!(h>0&&v>0&&std::isfinite(h)&&std::isfinite(v)))return;
        planes_={{{f.x,f.y,f.z,-.08},{-f.x,-f.y,-f.z,10000.},
            {h*f.x+r.x,h*f.y+r.y,h*f.z+r.z,0.},
            {h*f.x-r.x,h*f.y-r.y,h*f.z-r.z,0.},
            {v*f.x+u.x,v*f.y+u.y,v*f.z+u.z,0.},
            {v*f.x-u.x,v*f.y-u.y,v*f.z-u.z,0.}}};
        if(!std::isfinite(ex_)||!std::isfinite(ey_)||!std::isfinite(ez_))return;
        for(const auto& p:planes_)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))return;
        valid_=true;
    }
    template<class V> bool visible(const V& lo,const V& hi)const noexcept {
        if(!valid_||!(lo.x<=hi.x&&lo.y<=hi.y&&lo.z<=hi.z))return true;
        const double lx=double(lo.x)-ex_,ly=double(lo.y)-ey_,lz=double(lo.z)-ez_;
        const double hx=double(hi.x)-ex_,hy=double(hi.y)-ey_,hz=double(hi.z)-ez_;
        if(!std::isfinite(lx)||!std::isfinite(ly)||!std::isfinite(lz)||
           !std::isfinite(hx)||!std::isfinite(hy)||!std::isfinite(hz))return true;
        for(const auto& p:planes_){
            const double x=p.x*(p.x>=0?hx:lx),y=p.y*(p.y>=0?hy:ly),z=p.z*(p.z>=0?hz:lz);
            // Margin for the float vertex shader, including world/eye subtraction.
            const double margin=.01+1.e-5*(std::abs(x)+std::abs(y)+std::abs(z)+std::abs(p.d)
                +std::abs(p.x*ex_)+std::abs(p.y*ey_)+std::abs(p.z*ez_));
            if(x+y+z+p.d < -margin)return false;
        }
        return true;
    }
};
template<class V> bool lightMayContribute(const Frustum& f,const V& p,float radius,float intensity,const V& color) noexcept {
    if(!std::isfinite(radius)||!std::isfinite(intensity)||!std::isfinite(p.x)||
       !std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(color.x)||
       !std::isfinite(color.y)||!std::isfinite(color.z))return true;
    if(radius<=0.f||intensity==0.f)return false;
    const float d=radius*1.000002f+.01f;
    return f.visible(V{p.x-d,p.y-d,p.z-d},V{p.x+d,p.y+d,p.z+d});
}
template<class Chunks,class Predicate>
void fillVisibilityMask(const Chunks& chunks,std::vector<uint8_t>& mask,Predicate visible){
    mask.resize(chunks.size());
    for(size_t i=0;i<chunks.size();++i)mask[i]=uint8_t(visible(chunks[i]));
}
}
