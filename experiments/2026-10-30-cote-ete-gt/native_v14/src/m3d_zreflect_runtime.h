#pragma once
#include "m3d_zreflect_geometry.h"
#include "m3d_zreflect_index.h"
namespace m3d::zreflect {
constexpr uint32_t opticalTag=0x80000000u, noOptical=0xffffffffu;
inline Camera mirrorCamera(const Camera& c,V n,double d){
    const auto reflect=[&](V v){return v-n*(2.f*dot(v,n));};
    Camera m=c;m.eye=c.eye-n*float(2.*(double(dot(c.eye,n))-d));
    m.f=reflect(c.f);m.r=reflect(c.r);m.u=reflect(c.u)*-1.f;
    m.tanHalf=c.tanHalf*1.10f;return m;
}
struct ActivePlane { uint32_t id=0; double projected=0; };
class RuntimeOptics {
    SpatialIndex index_;
    std::vector<Patch> patches_;
    std::vector<uint8_t> forced_,water_;
    std::vector<uint32_t> shortlist_;
public:
    void build(const Scene& scene){
        patches_.clear();forced_.clear();water_.clear();std::vector<IndexedPlane> descriptors;
        for(const auto& p:scene.opticalPlanes){
            Patch patch;patch.normal=p.normal;patch.offset=p.offset;patch.area=p.area;patch.side=p.side;
            patch.bounds.add(p.lo);patch.bounds.add(p.hi);patch.triangles=p.triangles;
            descriptors.push_back({uint32_t(patches_.size()),patch.bounds});
            bool water=false;
            for(auto id:p.triangles)if(id<scene.obj.size()&&scene.obj[id].mat<scene.mat.size()){
                const auto& m=scene.mat[scene.obj[id].mat];
                water|=m.waterSurface||(!m.surfaceAuthored&&m.name=="cse_pool_water");
            }
            patches_.push_back(std::move(patch));forced_.push_back(p.forced);water_.push_back(water);
        }
        if(!index_.restore(descriptors,scene.opticalIndexNodes,scene.opticalIndexOrder))index_.build(descriptors);
    }
    QueryStats select(const Scene& scene,const Camera& c,std::vector<ActivePlane>& out,bool glassEnabled=true){
        out.clear();auto stats=index_.query(c,shortlist_);
        for(uint32_t id:shortlist_){
            if(!glassEnabled&&!water_[id])continue;
            const double area=projectedPatch(scene,patches_[id],c);
            if(area>0&&(area>=.02||forced_[id]))out.push_back({id,area});
        }
        std::sort(out.begin(),out.end(),[](const ActivePlane& a,const ActivePlane& b){
            return a.projected>b.projected||(a.projected==b.projected&&a.id<b.id);
        });return stats;
    }
    bool water(uint32_t id)const{return water_[id]!=0;}
    const Patch& patch(uint32_t id)const{return patches_[id];}
};
}
