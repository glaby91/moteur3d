#pragma once
#include "object_opt.h"
namespace m3d::escargot::object_compact {
struct TriangleTerms {V a,x,y;float xx,xy,yy,det;};
struct PreparedChart{
    const Chart* c=nullptr;
    std::vector<TriangleTerms> triangles;
    PreparedChart()=default;
    PreparedChart(const Scene& s,const Chart& chart):c(&chart){
        triangles.reserve(chart.tris.size());
        for(auto id:chart.tris){
            const auto& t=s.obj[id];V x=t.b-t.a,y=t.c-t.a;
            float xx=dot(x,x),xy=dot(x,y),yy=dot(y,y),det=xx*yy-xy*xy;
            if(det>1e-12f)triangles.push_back({t.a,x,y,xx,xy,yy,det});
        }
    }
    bool intersect(V from,V d,V& p,V& uv,float& t)const{
        float den=dot(c->n,d);if(std::abs(den)<1e-7f)return false;
        t=dot(c->n,c->o-from)/den;if(t<=1e-5f||!std::isfinite(t))return false;
        p=from+d*t;uv=c->uv(p);
        for(const auto& q:triangles){
            V z=p-q.a;
            float k=(dot(z,q.x)*q.yy-dot(z,q.y)*q.xy)/q.det;
            float l=(dot(z,q.y)*q.xx-dot(z,q.x)*q.xy)/q.det;
            if(k>=-1e-4f&&l>=-1e-4f&&k+l<=1.0001f)return true;
        }return false;
    }
};
inline Projection projectPrepared(const PreparedChart& receiver,const PreparedChart& source,const Camera& cam,int x,int y,int w,int h){
    Projection p;auto ray=screenRay(cam,x,y,w,h);V unused;
    if(!receiver.intersect(ray.o,ray.d,p.water,unused,p.primary))return p;
    p.direction=reflect(ray.d,receiver.c->n);
    p.valid=source.intersect(p.water,p.direction,p.source,p.uv,p.secondary);return p;
}
// Expand to existing tile boundaries, preserving the same bundles as the
// validated global implementation. Padding is a scheduling detail only.
inline Rect aligned(Rect r,int w,int h,int tile=16){
    int x=r.x/tile*tile,y=r.y/tile*tile;
    int x1=std::min(w,(r.x+r.w+tile-1)/tile*tile),y1=std::min(h,(r.y+r.h+tile-1)/tile*tile);
    return {x,y,x1-x,y1-y};
}
}
