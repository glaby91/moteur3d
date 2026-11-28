#pragma once
#include "object_chart.h"
namespace m3d::escargot::geometry_mask {
struct Box {
    V lo{INF,INF,INF},hi{-INF,-INF,-INF};
    void add(V p){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
    bool overlaps(V a,V b)const{
        constexpr float pad=.008f;
        return lo.x<=b.x+pad&&hi.x>=a.x-pad&&lo.y<=b.y+pad&&hi.y>=a.y-pad&&lo.z<=b.z+pad&&hi.z>=a.z-pad;
    }
};
struct Stats{uint64_t bundles=0,nodes=0,lists=0,overflow=0,triangleTests=0,blocked=0,clear=0,unknown=0;};
struct List {std::vector<uint32_t> ids;bool complete=true;};
inline List gather(const Scene& s,const LumaTraceBvh4& b,const Box& box,Stats& stats,size_t cap){
    List list;std::vector<uint32_t> stack{0};
    while(!stack.empty()){
        uint32_t i=stack.back();stack.pop_back();++stats.nodes;
        if(i>=b.nodes.size()){list.complete=false;break;}
        const auto& n=b.nodes[i];if(!box.overlaps({n.minX,n.minY,n.minZ},{n.maxX,n.maxY,n.maxZ}))continue;
        if(n.primitiveCount){
            for(uint32_t j=0;j<n.primitiveCount;++j){
                auto id=s.order[n.firstPrimitive+j];const auto& t=s.obj[id];
                if(!box.overlaps(t.mn,t.mx))continue;
                if(list.ids.size()>=cap){list.complete=false;break;}list.ids.push_back(id);
            }
            if(!list.complete)break;
        }else{uint32_t kids[]={n.child0,n.child1,n.child2,n.child3};for(uint32_t j=0;j<n.childCount;++j)stack.push_back(kids[j]);}
    }
    ++stats.lists;if(!list.complete)++stats.overflow;
    return list;
}
// Match the exact triangle arithmetic of Traversal; ties must match its
// stable primitive order. The envelope is expanded past the expected end,
// so a closer occluder cannot be discarded by the broad phase.
inline bool hit(const Obj& o,const Ray& r,float low,float high,float& t){
    const V e=o.b-o.a,f=o.c-o.a,p=cross(r.d,f);float det=dot(e,p);
    if(std::abs(det)<1e-10f)return false;
    V q=r.o-o.a;float u=dot(q,p)/det;if(u<0||u>1)return false;
    V z=cross(q,e);float v=dot(r.d,z)/det;if(v<0||u+v>1)return false;
    t=dot(f,z)/det;return std::isfinite(t)&&t>low&&t<=high;
}
inline int segment(const Scene& s,const std::vector<uint32_t>& owners,const List& list,
                   const Ray& r,float low,float expected,uint32_t owner,uint32_t mat,bool checkMaterial,Stats& stats){
    if(!list.complete)return -1;
    float best=expected+.0031f;uint32_t id=0xffffffffu;
    for(auto i:list.ids){
        ++stats.triangleTests;float t;
        if(hit(s.obj[i],r,low,best,t)&&(t<best||i<id)){best=t;id=i;}
    }
    if(id==0xffffffffu)return 0;
    return owners[id]==owner&&std::abs(best-expected)<=.003f&&
        (!checkMaterial||s.obj[id].mat==mat)?1:0;
}
inline Stats classify(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owners,
                      const Chart& receiver,const Chart& source,const Camera& cam,
                      const std::vector<Projection>& proj,int w,int h,std::vector<float>& mask,int tile=8,size_t cap=96){
    Stats stats;
    for(int y=0;y<h;y+=tile)for(int x=0;x<w;x+=tile){
        std::vector<int> pixels;Box primary,secondary;
        for(int yy=y;yy<std::min(y+tile,h);++yy)for(int xx=x;xx<std::min(x+tile,w);++xx){
            int i=yy*w+xx;const auto& p=proj[i];if(!p.valid)continue;
            pixels.push_back(i);primary.add(cam.eye);primary.add(p.water+norm(p.water-cam.eye)*.004f);
            secondary.add(p.water);secondary.add(p.source+p.direction*.004f);
        }
        if(pixels.empty())continue;++stats.bundles;
        auto A=gather(s,b,primary,stats,cap),B=gather(s,b,secondary,stats,cap);
        for(int i:pixels){
            const auto& p=proj[i];V d=norm(p.water-cam.eye);
            int a=segment(s,owners,A,{cam.eye,d},.08f/std::max(1e-6f,dot(d,cam.f)),p.primary,receiver.owner,receiver.mat,false,stats);
            int v=a==0?0:segment(s,owners,B,{p.water,p.direction},eps(p.water),p.secondary,source.owner,source.mat,true,stats);
            if(a<0&&v!=0)v=-1;
            if(v<0){++stats.unknown;continue;}mask[i]=float(v);if(v)++stats.clear;else ++stats.blocked;
        }
    }return stats;
}
}
