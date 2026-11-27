#pragma once
#include "m3d_engine.h"
#include "m3d_zreflect_visibility.h"
#include "m3d_zreflect_policy.h"
#include <map>
#include <array>
#include <vector>
namespace m3d::zreflect {
using PlaneKey=std::array<int64_t,4>;
struct Patch {
    V normal{}; double offset=0,area=0; Bounds bounds;
    std::vector<uint32_t> triangles;
    int side=0; // -1/+1: outward face of thin slab, 0: genuinely two-sided plane
};
inline bool trianglePlane(const Obj& tri,V& n,double& d,double& area){
    if(tri.kind!=Kind::Triangle)return false;
    V raw=cross(tri.b-tri.a,tri.c-tri.a);float len=length(raw);
    if(!std::isfinite(len)||len<=1.e-7f)return false;
    n=raw/len;float a[3]={std::abs(n.x),std::abs(n.y),std::abs(n.z)};
    if((a[0]>=a[1]&&a[0]>=a[2]?n.x:(a[1]>=a[2]?n.y:n.z))<0)n=n*-1.f;
    d=dot(n,tri.a);area=.5*len;return std::isfinite(d)&&std::abs(d)<1.e9;
}
inline PlaneKey key(V n,double d){
    return {std::llround(n.x*10000.),std::llround(n.y*10000.),std::llround(n.z*10000.),std::llround(d*1000.)};
}
inline bool samePlane(const Patch& p,const Obj& tri,V n){
    if(dot(p.normal,n)<.999999f)return false;
    for(V v:{tri.a,tri.b,tri.c})if(std::abs(double(dot(p.normal,v))-p.offset)>.001)return false;
    return true;
}
inline void addTriangle(std::map<PlaneKey,std::vector<Patch>>& groups,const Obj& tri,uint32_t id){
    V n;double d,area;if(!trianglePlane(tri,n,d,area))return;
    auto& bucket=groups[key(n,d)];Patch* p=nullptr;
    for(auto& candidate:bucket)if(samePlane(candidate,tri,n)){p=&candidate;break;}
    if(!p){bucket.emplace_back();p=&bucket.back();p->normal=n;p->offset=d;}
    p->area+=area;p->triangles.push_back(id);
    p->bounds.add(tri.a);p->bounds.add(tri.b);p->bounds.add(tri.c);
}
inline std::vector<Patch> extractSlab(const Scene& scene,int first,int count,bool water,
                                      bool explicitlyEnabled,const Policy& policy={}){
    if(first<0||count<=0||size_t(first)>scene.obj.size()||size_t(count)>scene.obj.size()-size_t(first))return {};
    std::map<PlaneKey,std::vector<Patch>> groups;
    for(int i=0;i<count;++i)addTriangle(groups,scene.obj[size_t(first+i)],uint32_t(first+i));
    const Patch* dominant=nullptr;double total=0;
    for(const auto& kv:groups)for(const auto& p:kv.second){
        total+=p.area;if(!dominant||p.area>dominant->area)dominant=&p;
    }
    if(!dominant||dominant->bounds.uncertain)return {};
    const V n=dominant->normal;
    // An entire object must be a thin slab or flat patch. A curved multi-facet
    // object cannot pass just because individual facets have a large area.
    V u=norm(cross(n,std::abs(n.y)<.9f?V{0,1,0}:V{1,0,0})),v=norm(cross(n,u));
    double lo=INFINITY,hi=-INFINITY,ulo=INFINITY,uhi=-INFINITY,vlo=INFINITY,vhi=-INFINITY,parallel=0;
    for(int i=0;i<count;++i){
        const auto& tri=scene.obj[size_t(first+i)];
        if(tri.kind!=Kind::Triangle)return {};
        for(V q:{tri.a,tri.b,tri.c}){
            double z=dot(n,q),x=dot(u,q),y=dot(v,q);
            if(!std::isfinite(z)||!std::isfinite(x)||!std::isfinite(y))return {};
            lo=std::min(lo,z);hi=std::max(hi,z);ulo=std::min(ulo,x);uhi=std::max(uhi,x);vlo=std::min(vlo,y);vhi=std::max(vhi,y);
        }
    }
    for(const auto& kv:groups)for(const auto& p:kv.second)
        if(dot(n,p.normal)>.999999f)parallel+=p.area;
    if(!slabEligible(hi-lo,std::min(uhi-ulo,vhi-vlo),parallel,total,policy))return {};
    if(water&&n.y<.999999f)return {}; // current water optics require a horizontal surface
    std::vector<Patch> result;
    for(const auto& kv:groups)for(auto p:kv.second){
        if(dot(n,p.normal)<.999999f||p.bounds.uncertain||(!explicitlyEnabled&&p.area<policy.minArea))continue;
        const bool low=std::abs(p.offset-lo)<=.001,high=std::abs(p.offset-hi)<=.001;
        if(!low&&!high)continue;
        if(water&&!high)continue;
        p.side=low&&high?0:(high?1:-1);
        result.push_back(std::move(p));
    }
    return result;
}
inline double planeDistance(V p,const Patch& patch){return double(dot(p,patch.normal))-patch.offset;}
inline bool frontFacing(const Patch& patch,const Camera& c){
    return patch.side==0||patch.side*planeDistance(c.eye,patch)>=-.001;
}
inline std::vector<V> clipPolygon(const std::vector<V>& p,V normal,float offset){
    std::vector<V> out;if(p.empty())return out;
    for(size_t i=0;i<p.size();++i){
        V a=p[i],b=p[(i+1)%p.size()];float da=dot(a,normal)-offset,db=dot(b,normal)-offset;
        if(da>=0)out.push_back(a);
        if((da>=0)!=(db>=0))out.push_back(a+(b-a)*(da/(da-db)));
    }
    return out;
}
inline double projectedTriangle(const Obj& tri,const Camera& c){
    if(c.tanHalf<=0||c.aspect<=0)return 0;
    const float w=c.tanHalf,h=w/c.aspect;
    std::vector<V> p;for(V v:{tri.a,tri.b,tri.c}){V q=v-c.eye;p.push_back({dot(q,c.r),dot(q,c.u),dot(q,c.f)});}
    p=clipPolygon(p,{0,0,1},.08f);
    for(V n:{V{1,0,w},V{-1,0,w},V{0,1,h},V{0,-1,h}}){p=clipPolygon(p,n,0);if(p.size()<3)return 0;}
    double area=0;
    for(size_t i=0;i<p.size();++i){
        V a=p[i],b=p[(i+1)%p.size()];
        area+=(double(a.x)*b.y-double(a.y)*b.x)/(double(a.z)*b.z*w*h);
    }
    return std::isfinite(area)?std::min(1.,std::abs(area)/8.):0.;
}
inline double projectedPatch(const Scene& scene,const Patch& p,const Camera& c){
    if(!frontFacing(p,c)||!potentiallyVisible(p.bounds,c))return 0;
    double area=0;
    for(auto id:p.triangles)if(id<scene.obj.size())area+=projectedTriangle(scene.obj[id],c);
    return std::min(1.,area); // overlaps may overestimate: diagnostic, no occlusion
}
}
