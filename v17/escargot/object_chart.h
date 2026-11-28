#pragma once
#include "runtime.h"
namespace e=m3d::escargot;
using namespace m3d;
struct Chart {
    uint32_t owner=0,mat=0; V o{},n{},du{},dv{};float u=0,v=0;
    float u0=INF,v0=INF,u1=-INF,v1=-INF;
    std::vector<uint32_t> tris;
    V uv(V p)const{return {u+dot(du,p-o),v+dot(dv,p-o),0};}
    bool intersect(const Scene& s,V from,V d,V& p,V& tex,float& t)const{
        float den=dot(n,d);if(std::abs(den)<1e-7f)return false;
        t=dot(n,o-from)/den;if(t<=1e-5f||!std::isfinite(t))return false;
        p=from+d*t;tex=uv(p);
        for(auto id:tris){const auto& a=s.obj[id];V x=a.b-a.a,y=a.c-a.a,z=p-a.a;
            float xx=dot(x,x),xy=dot(x,y),yy=dot(y,y),det=xx*yy-xy*xy;if(det<=1e-12f)continue;
            float k=(dot(z,x)*yy-dot(z,y)*xy)/det,l=(dot(z,y)*xx-dot(z,x)*xy)/det;
            if(k>=-1e-4f&&l>=-1e-4f&&k+l<=1.0001f)return true;
        }return false;
    }
};
static Chart chart(const Scene& s,const std::vector<uint32_t>& owner,uint32_t id){
    Chart a;if(id>=s.obj.size())return a;const auto& t=s.obj[id];V x=t.b-t.a,y=t.c-t.a;
    float xx=dot(x,x),xy=dot(x,y),yy=dot(y,y),det=xx*yy-xy*xy;if(det<=1e-12f)return a;
    V gx=(x*yy-y*xy)/det,gy=(y*xx-x*xy)/det;
    a.owner=owner[id];a.mat=t.mat;a.o=t.a;a.n=norm(cross(x,y));a.u=t.uv0.x;a.v=t.uv0.y;
    a.du=gx*(t.uv1.x-a.u)+gy*(t.uv2.x-a.u);a.dv=gx*(t.uv1.y-a.v)+gy*(t.uv2.y-a.v);
    if(!e::finite(a.du)||!e::finite(a.dv))return {};
    for(uint32_t j=0;j<s.obj.size();j++){const auto& q=s.obj[j];if(owner[j]!=a.owner||q.mat!=a.mat)continue;
        bool ok=true;for(auto pair:{std::pair<V,V>{q.a,q.uv0},{q.b,q.uv1},{q.c,q.uv2}}){auto z=a.uv(pair.first);
            if(std::abs(dot(pair.first-a.o,a.n))>.002f||std::abs(z.x-pair.second.x)>.002f||std::abs(z.y-pair.second.y)>.002f)ok=false;}
        if(ok){a.tris.push_back(j);for(V p:{q.a,q.b,q.c}){auto z=a.uv(p);a.u0=std::min(a.u0,z.x);a.u1=std::max(a.u1,z.x);a.v0=std::min(a.v0,z.y);a.v1=std::max(a.v1,z.y);}}
    }return a;
}
struct Projection {V water{},source{},uv{},direction{};float primary=0,secondary=0;bool valid=false;};
static Projection project(const Scene& s,const Chart& water,const Chart& src,const Camera& cam,int x,int y,int w,int h){
    Projection p;auto r=e::screenRay(cam,x,y,w,h);V unused;
    if(!water.intersect(s,r.o,r.d,p.water,unused,p.primary))return p;
    p.direction=e::reflect(r.d,water.n);
    p.valid=src.intersect(s,p.water,p.direction,p.source,p.uv,p.secondary);return p;
}
static float eps(V p){return std::max(1e-5f,8.f*std::numeric_limits<float>::epsilon()*std::max(1.f,length(p)));}
static int visibility(e::Traversal& b,const Scene& s,const Chart& water,const Chart& src,const Camera& cam,const Projection& p,uint64_t& rays){
    e::HitSample hit;const V d=norm(p.water-cam.eye);++rays;
    if(!b.closest({cam.eye,d},.08f/std::max(1e-6f,dot(d,cam.f)),hit)||hit.object!=water.owner||std::abs(hit.depth-p.primary)>.003f)return 0;
    ++rays;if(!b.closest({p.water,p.direction},eps(p.water),hit))return 0;
    if(hit.object==src.owner&&std::abs(hit.depth-p.secondary)<.003f&&s.obj[hit.primitive].mat==src.mat)return 1;
    return 0; // transparent crossings deliberately excluded in single-layer test
}
static int channel(float x){x=e::sat(x);return int(std::round(255*(x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f)));}
