#pragma once
#include "object_compact.h"
#include <set>
#include <stdexcept>
namespace m3d::escargot::object_global {
struct WorkRay{Ray ray{};float low=0;};
struct Counters{uint64_t nodeLoads=0,boxTests=0,triangleTests=0,seedTests=0,seedHits=0,packets=0;};
struct Slabs {
    float o[3],inv[3];bool parallel[3];
    explicit Slabs(const Ray& r):o{r.o.x,r.o.y,r.o.z}{
        const float d[]={r.d.x,r.d.y,r.d.z};
        for(int a=0;a<3;++a){parallel[a]=std::abs(d[a])<1e-12f;inv[a]=parallel[a]?0.f:1.f/d[a];}
    }
};
inline bool nodeBox(const LumaTraceBvh4Node& n,const Slabs& r,float low,float high,float& near){
    const float lo[]={n.minX,n.minY,n.minZ},hi[]={n.maxX,n.maxY,n.maxZ};
    for(int a=0;a<3;++a){
        if(r.parallel[a]){if(r.o[a]<lo[a]||r.o[a]>hi[a])return false;continue;}
        float l=(lo[a]-r.o[a])*r.inv[a],h=(hi[a]-r.o[a])*r.inv[a];if(l>h)std::swap(l,h);
        l-=4.f*std::numeric_limits<float>::epsilon()*std::max(1.f,std::abs(l));
        h+=4.f*std::numeric_limits<float>::epsilon()*std::max(1.f,std::abs(h));
        low=std::max(low,l);high=std::min(high,h);if(high<low)return false;
    }near=low;return true;
}
inline void test(const Scene& s,const std::vector<uint32_t>& owners,uint32_t id,const WorkRay& r,HitSample& hit){
    const auto& o=s.obj[id];if(o.kind!=Kind::Triangle)return;
    const V e=o.b-o.a,f=o.c-o.a,p=cross(r.ray.d,f);float det=dot(e,p);if(std::abs(det)<1e-10f)return;
    const V q=r.ray.o-o.a;float u=dot(q,p)/det;if(u<0||u>1)return;
    const V z=cross(q,e);float v=dot(r.ray.d,z)/det;if(v<0||u+v>1)return;
    float t=dot(f,z)/det;
    if(!std::isfinite(t)||t<=r.low||t>hit.depth||(hit.object&&t==hit.depth&&id>=hit.primitive))return;
    const float w=1-u-v;V nn=o.n0*w+o.n1*u+o.n2*v,geom=norm(cross(o.b-o.a,o.c-o.a));
    hit={id,owners[id],t,r.ray.o+r.ray.d*t,length(nn)>1e-8f?norm(nn):geom,o.uv0*w+o.uv1*u+o.uv2*v,geom};hit.uv.z=0;
}
// Packet traversal is exhaustive for every lane: seeds only establish an
// actual geometric upper distance, never an exclusive candidate list.
// No interpolation of UVs, receiver normals or object IDs.
class Packet {
    const Scene& s_;const LumaTraceBvh4& b_;const std::vector<uint32_t>& owners_;
    struct Entry{uint32_t node,lanes;float near;};
    std::vector<Entry> stack_;
public:
    Packet(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owners):s_(s),b_(b),owners_(owners){stack_.reserve(128);}
    void trace(const std::vector<WorkRay>& rays,const std::vector<uint32_t>& seeds,std::vector<HitSample>& hits,Counters& st){
        hits.assign(rays.size(),{});for(auto& h:hits)h.depth=INF;if(rays.empty())return;
        // caller partitions tiles into at most 32 lanes.
        if(rays.size()>32)throw std::runtime_error("packet lane overflow");
        ++st.packets;std::vector<Slabs> slabs;slabs.reserve(rays.size());
        for(const auto& ray:rays)slabs.emplace_back(ray.ray);
        for(size_t i=0;i<rays.size();++i){
            for(auto id:seeds){++st.seedTests;test(s_,owners_,id,rays[i],hits[i]);}
            if(hits[i].object)++st.seedHits;
        }
        auto select=[&](uint32_t node,uint32_t mask){
            Entry e{node,0,INF};++st.nodeLoads;const auto& n=b_.nodes[node];
            for(size_t i=0;i<rays.size();++i)if(mask&(uint32_t(1)<<i)){
                float near=0;++st.boxTests;
                if(nodeBox(n,slabs[i],rays[i].low,hits[i].depth,near)){e.lanes|=uint32_t(1)<<i;e.near=std::min(e.near,near);}
            }return e;
        };
        const uint32_t all=rays.size()==32?0xffffffffu:(uint32_t(1)<<rays.size())-1;
        stack_.clear();auto root=select(0,all);if(root.lanes)stack_.push_back(root);
        while(!stack_.empty()){
            auto e=stack_.back();stack_.pop_back();
            // Recheck against the now tighter nearest hit distances.
            e=select(e.node,e.lanes);if(!e.lanes)continue;
            const auto& n=b_.nodes[e.node];
            if(n.primitiveCount){
                for(uint32_t k=0;k<n.primitiveCount;++k){
                    auto id=s_.order[n.firstPrimitive+k];
                    for(size_t i=0;i<rays.size();++i)if(e.lanes&(uint32_t(1)<<i)){++st.triangleTests;test(s_,owners_,id,rays[i],hits[i]);}
                }
            }else{
                const uint32_t kids[]={n.child0,n.child1,n.child2,n.child3};
                std::array<Entry,4> next{};size_t count=0;
                for(uint32_t k=0;k<n.childCount;++k){auto p=select(kids[k],e.lanes);if(p.lanes)next[count++]=p;}
                std::sort(next.begin(),next.begin()+count,[](const Entry& a,const Entry& b){return a.near>b.near;});
                for(size_t k=0;k<count;++k)stack_.push_back(next[k]);
            }
        }
    }
};
}
