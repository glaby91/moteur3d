#pragma once
#include "types.h"
namespace m3d::escargot {
// Read-only adapter to the existing LumaTrace BVH4. No second BVH/geometry copy.
// The leaf order is Scene::order, NOT the opaque GPU remapping (glass retained).
class Traversal {
    const Scene& s_;const LumaTraceBvh4& b_;
    const std::vector<uint32_t>& owner_;
    struct Entry{uint32_t node;float near;};
    std::vector<Entry> stack_;
    bool valid_=false;
    static bool box(const LumaTraceBvh4Node& n,const Ray& r,float tmin,float tmax,float& near){
        const float o[]={r.o.x,r.o.y,r.o.z},d[]={r.d.x,r.d.y,r.d.z};
        const float lo[]={n.minX,n.minY,n.minZ},hi[]={n.maxX,n.maxY,n.maxZ};
        for(int a=0;a<3;++a){
            if(std::abs(d[a])<1e-12f){if(o[a]<lo[a]||o[a]>hi[a])return false;continue;}
            float l=(lo[a]-o[a])/d[a],h=(hi[a]-o[a])/d[a];
            if(l>h)std::swap(l,h);
            tmin=std::max(tmin,l);tmax=std::min(tmax,h);if(tmax<tmin)return false;
        }near=tmin;return true;
    }
    static bool tri(const Obj& o,const Ray& ray,float low,float high,float& t,float& u,float& v){
        if(o.kind!=Kind::Triangle)return false;
        const V e=o.b-o.a,f=o.c-o.a,p=cross(ray.d,f);
        const float det=dot(e,p);if(std::abs(det)<1e-10f)return false;
        const V q=ray.o-o.a;u=dot(q,p)/det;if(u<0||u>1)return false;
        const V z=cross(q,e);v=dot(ray.d,z)/det;if(v<0||u+v>1)return false;
        t=dot(f,z)/det;return std::isfinite(t)&&t>low&&t<=high;
    }
public:
    Traversal(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owner):s_(s),b_(b),owner_(owner){stack_.reserve(128);valid_=validate();}
    bool valid()const{return valid_;}
    bool validate()const{
        if(b_.nodes.empty()||b_.primitiveCount!=s_.order.size()||owner_.size()!=s_.obj.size())return false;
        std::vector<uint8_t> visited(b_.nodes.size(),0),slots(s_.order.size(),0),primitives(s_.obj.size(),0);
        std::vector<uint32_t> pending{0};
        while(!pending.empty()){
            const auto id=pending.back();pending.pop_back();
            if(id>=b_.nodes.size()||visited[id]++)return false;
            const auto& n=b_.nodes[id];const V lo{n.minX,n.minY,n.minZ},hi{n.maxX,n.maxY,n.maxZ};
            if(!finite(lo)||!finite(hi)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z)return false;
            if(n.primitiveCount){
                if(n.childCount||n.firstPrimitive>s_.order.size()||n.primitiveCount>s_.order.size()-n.firstPrimitive)return false;
                for(uint32_t i=0;i<n.primitiveCount;++i){
                    const auto at=n.firstPrimitive+i,id=s_.order[at];
                    if(slots[at]++||id>=s_.obj.size()||primitives[id]++||!owner_[id])return false;
                    const auto& o=s_.obj[id];if(o.kind!=Kind::Triangle||o.mat>=s_.mat.size()||!finite(o.a)||!finite(o.b)||!finite(o.c))return false;
                }
            }else{
                if(n.childCount<2||n.childCount>4)return false;
                const uint32_t kids[]={n.child0,n.child1,n.child2,n.child3};
                for(uint32_t i=0;i<n.childCount;++i)pending.push_back(kids[i]);
            }
        }
        return std::find(visited.begin(),visited.end(),0)==visited.end()&&std::find(slots.begin(),slots.end(),0)==slots.end()&&std::find(primitives.begin(),primitives.end(),0)==primitives.end();
    }
    bool closest(const Ray& ray,float low,HitSample& hit){
        hit={};hit.depth=INF;stack_.clear();
        if(!valid_||!finite(ray.o)||!finite(ray.d)||!std::isfinite(low))return false;
        float near=0;if(!box(b_.nodes.front(),ray,low,hit.depth,near))return false;
        stack_.push_back({0,near});size_t visited=0;
        while(!stack_.empty()){
            auto e=stack_.back();stack_.pop_back();if(e.near>hit.depth||e.node>=b_.nodes.size())continue;
            if(++visited>b_.nodes.size())return false; // malformed hierarchy, not a silent stack overflow
            const auto& n=b_.nodes[e.node];
            if(n.primitiveCount){
                if(n.firstPrimitive>s_.order.size()||n.primitiveCount>s_.order.size()-n.firstPrimitive)return false;
                for(uint32_t i=0;i<n.primitiveCount;++i){
                    const uint32_t id=s_.order[n.firstPrimitive+i];if(id>=s_.obj.size()||id>=owner_.size()||!owner_[id])continue;
                    const auto& o=s_.obj[id];float t,u,v;
                    if(!tri(o,ray,low,hit.depth,t,u,v)||(hit.object&&t==hit.depth&&id>=hit.primitive))continue;
                    const float w=1-u-v;
                    const V nn=o.n0*w+o.n1*u+o.n2*v;
                    const V geom=norm(cross(o.b-o.a,o.c-o.a));
                    hit={id,owner_[id],t,ray.o+ray.d*t,length(nn)>1e-8f?norm(nn):geom,o.uv0*w+o.uv1*u+o.uv2*v,geom};
                    hit.uv.z=0; // bridge reuses z for barycentric coefficients
                }
            }else{
                const uint32_t kids[]={n.child0,n.child1,n.child2,n.child3};
                std::array<Entry,4> hits{};size_t count=0;
                for(uint32_t i=0;i<std::min(n.childCount,4u);++i){
                    const auto k=kids[i];if(k>=b_.nodes.size()||k==e.node)return false;
                    if(box(b_.nodes[k],ray,low,hit.depth,near))hits[count++]={k,near};
                }
                std::sort(hits.begin(),hits.begin()+count,[](Entry a,Entry b){return a.near>b.near;});
                for(size_t i=0;i<count;++i)stack_.push_back(hits[i]);
            }
        }return hit.object!=0;
    }
};
}
