#pragma once
#include "object_geometry_mask.h"
namespace m3d::escargot::object_opt {
namespace g=geometry_mask;
struct Segment {Ray ray;float low,high;};
inline bool overlaps(const Segment& r,V lo,V hi){
    const float o[]={r.ray.o.x,r.ray.o.y,r.ray.o.z},d[]={r.ray.d.x,r.ray.d.y,r.ray.d.z};
    float l[]={lo.x,lo.y,lo.z},h[]={hi.x,hi.y,hi.z},a=r.low,b=r.high;
    for(int k=0;k<3;++k){
        l[k]-=.008f;h[k]+=.008f;
        if(std::abs(d[k])<1e-12f){if(o[k]<l[k]||o[k]>h[k])return false;continue;}
        float inv=1.f/d[k],t0=(l[k]-o[k])*inv,t1=(h[k]-o[k])*inv;if(t0>t1)std::swap(t0,t1);
        a=std::max(a,t0);b=std::min(b,t1);if(a>b)return false;
    }return true;
}
struct NarrowStats{g::Stats base;uint64_t segmentBoxTests=0;};
inline g::List gather(const Scene& s,const LumaTraceBvh4& b,const g::Box& box,const std::vector<Segment>& segments,NarrowStats& st,size_t cap){
    g::List out;std::vector<uint32_t> stack{0};auto& stats=st.base;
    auto may=[&](V lo,V hi){if(!box.overlaps(lo,hi))return false;
        for(const auto& r:segments){++st.segmentBoxTests;if(overlaps(r,lo,hi))return true;}return false;};
    while(!stack.empty()){
        uint32_t id=stack.back();stack.pop_back();++stats.nodes;
        const auto& n=b.nodes[id];if(!may({n.minX,n.minY,n.minZ},{n.maxX,n.maxY,n.maxZ}))continue;
        if(n.primitiveCount){for(uint32_t i=0;i<n.primitiveCount;++i){auto j=s.order[n.firstPrimitive+i];const auto& t=s.obj[j];
            if(!may(t.mn,t.mx))continue;if(out.ids.size()>=cap){out.complete=false;break;}out.ids.push_back(j);}
            if(!out.complete)break;
        }else{const uint32_t kids[]={n.child0,n.child1,n.child2,n.child3};for(uint32_t i=0;i<n.childCount;++i)stack.push_back(kids[i]);}
    }
    ++stats.lists;if(!out.complete)++stats.overflow;return out;
}
inline NarrowStats narrow(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owners,const Chart& rec,const Chart& src,
                         const Camera& cam,const std::vector<Projection>& proj,int w,int h,std::vector<float>& mask,int tile,size_t cap){
    NarrowStats st;auto& stats=st.base;
    for(int y=0;y<h;y+=tile)for(int x=0;x<w;x+=tile){
        std::vector<int> ids;std::vector<Segment> a,bseg;g::Box A,B;
        for(int yy=y;yy<std::min(h,y+tile);++yy)for(int xx=x;xx<std::min(w,x+tile);++xx){
            int i=yy*w+xx;const auto& p=proj[i];if(!p.valid)continue;ids.push_back(i);
            V d=norm(p.water-cam.eye);float low=.08f/std::max(1e-6f,dot(d,cam.f));
            a.push_back({{cam.eye,d},low,p.primary+.004f});bseg.push_back({{p.water,p.direction},eps(p.water),p.secondary+.004f});
            A.add(cam.eye);A.add(p.water+d*.004f);B.add(p.water);B.add(p.source+p.direction*.004f);
        }
        if(ids.empty())continue;++stats.bundles;
        auto la=gather(s,b,A,a,st,cap),lb=gather(s,b,B,bseg,st,cap);
        for(size_t j=0;j<ids.size();++j){
            int i=ids[j];const auto& p=proj[i];
            int v=g::segment(s,owners,la,a[j].ray,a[j].low,p.primary,rec.owner,rec.mat,false,stats);
            int q=v==0?0:g::segment(s,owners,lb,bseg[j].ray,bseg[j].low,p.secondary,src.owner,src.mat,true,stats);
            if(v<0&&q!=0)q=-1;
            if(q<0){++stats.unknown;continue;}mask[i]=float(q);if(q)++stats.clear;else ++stats.blocked;
        }
    }return st;
}
inline Rect intersectRect(Rect a,Rect b,int w,int h){
    int x=std::max(0,std::max(a.x,b.x)-2),y=std::max(0,std::max(a.y,b.y)-2);
    int x1=std::min(w,std::min(a.x+a.w,b.x+b.w)+2),y1=std::min(h,std::min(a.y+a.h,b.y+b.h)+2);
    return {x,y,std::max(0,x1-x),std::max(0,y1-y)};
}
inline Rect crop(const Scene& s,const Chart& rec,const Chart& src,const Camera& cam,int w,int h){
    g::Box a,b;for(auto id:rec.tris){const auto& t=s.obj[id];for(V p:{t.a,t.b,t.c})a.add(p);}
    for(auto id:src.tris){const auto& t=s.obj[id];for(V p:{t.a,t.b,t.c})b.add(p-rec.n*(2.f*dot(p-rec.o,rec.n)));}
    V pad{.008f,.008f,.008f};auto A=projectBounds(a.lo-pad,a.hi+pad,cam,w,h),B=projectBounds(b.lo-pad,b.hi+pad,cam,w,h);
    if(!A.valid()||!B.valid())return {0,0,w,h}; // conservative fallback near clipping
    auto r=intersectRect(A,B,w,h);return r.valid()?r:Rect{0,0,w,h};
}
// Exact projection within ALL triangles of a discovered object, independent
// of whether an external occluder hides it. No affine chart restriction.
struct MeshHit{uint32_t primitive=0xffffffffu;float depth=INF;V uv{};};
inline MeshHit mesh(const Scene& s,const v16::ObjectInfo& obj,Ray ray,float low,uint64_t& tests){
    MeshHit h;
    for(int j=0;j<obj.triangleCount;++j){uint32_t id=uint32_t(obj.firstTriangle+j);const auto& t=s.obj[id];
        if(!overlaps({ray,low,h.depth},t.mn,t.mx))continue;++tests;
        const V e=t.b-t.a,f=t.c-t.a,p=cross(ray.d,f);float det=dot(e,p);if(std::abs(det)<1e-10f)continue;
        V q=ray.o-t.a;float u=dot(q,p)/det;if(u<0||u>1)continue;
        V z=cross(q,e);float v=dot(ray.d,z)/det;if(v<0||u+v>1)continue;
        float d=dot(f,z)/det;if(!std::isfinite(d)||d<=low||d>h.depth||(d==h.depth&&id>=h.primitive))continue;
        h={id,d,t.uv0*(1-u-v)+t.uv1*u+t.uv2*v};h.uv.z=0;
    }return h;
}
}
