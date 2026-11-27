#pragma once
#include "sampling.h"
namespace m3d::escargot {
inline void makeLayers(Surface& sf){
    sf.layers.clear();sf.completeIndices.clear();sf.receiverIndices.clear();
    for(size_t i=0;i<sf.samples.size();++i){
        const auto& s=sf.samples[i];
        if(s.complete)sf.completeIndices.push_back(i);
        if(s.receiver!=State::Unknown)sf.receiverIndices.push_back(i);
        for(const auto& kv:s.values){
            auto& l=sf.layers[kv.first];l.key=kv.first;l.measured.emplace(i,kv.second);if(!s.complete)l.partial.push_back(i);
            if(!l.bounds.valid())l.bounds={s.x,s.y,1,1};
            else{const int x=std::min(l.bounds.x,s.x),y=std::min(l.bounds.y,s.y);
                const int right=std::max(l.bounds.x+l.bounds.w,s.x+1),bottom=std::max(l.bounds.y+l.bounds.h,s.y+1);
                l.bounds={x,y,right-x,bottom-y};}
        }
    }
    sf.sameKnownSet=sf.completeIndices==sf.receiverIndices;
    const auto r=sf.candidate.bounds;
    for(auto& kv:sf.layers){auto& b=kv.second.bounds;const int x=std::max(r.x,b.x-sf.step),y=std::max(r.y,b.y-sf.step);
        const int right=std::min(r.x+r.w,b.x+b.w+sf.step),bottom=std::min(r.y+r.h,b.y+b.h+sf.step);b={x,y,right-x,bottom-y};}
}
// Exactly four nearest original known cells (or fewer if insufficient).
// Unknown is not Zero. Neither interpolated cells nor unknown observations
// participate. ID zero is never assigned a source UV or interpolated away.
struct Neighbors {
    std::array<size_t,4> ids{};
    std::array<int64_t,4> distance{};
    size_t count=0;
};
inline void insertNeighbor(Neighbors& out,const Surface& sf,int x,int y,size_t i){
    const auto& s=sf.samples[i];const int64_t dx=int64_t(s.x)-x,dy=int64_t(s.y)-y,d=dx*dx+dy*dy;
    size_t k=0;
    while(k<out.count&&std::pair<int64_t,size_t>{out.distance[k],out.ids[k]}<std::pair<int64_t,size_t>{d,i})++k;
    if(k<out.count&&out.ids[k]==i)return;
    if(k==4)return;
    if(out.count<4)++out.count;
    for(size_t j=out.count-1;j>k;--j){out.ids[j]=out.ids[j-1];out.distance[j]=out.distance[j-1];}
    out.ids[k]=i;out.distance[k]=d;
}
template<class Known> inline Neighbors nearest(const Surface& sf,int x,int y,Known known,const std::vector<size_t>& indices){
    Neighbors out;
    if(sf.cols<=0||sf.rows<=0||sf.step<=0||indices.empty())return out;
    // Sparse observations: scan only valid IDs; dense: exact bounded ring.
    if(indices.size()*4<sf.samples.size()){
        for(size_t i:indices)insertNeighbor(out,sf,x,y,i);return out;
    }
    const int gx=std::clamp((x-sf.candidate.bounds.x)/sf.step,0,sf.cols-1);
    const int gy=std::clamp((y-sf.candidate.bounds.y)/sf.step,0,sf.rows-1);
    auto visit=[&](int a,int b){
        if(a<0||b<0||a>=sf.cols||b>=sf.rows)return;
        const size_t i=size_t(b)*sf.cols+a;if(known(i,sf.samples[i]))insertNeighbor(out,sf,x,y,i);
    };
    for(int radius=0;radius<std::max(sf.cols,sf.rows);++radius){
        if(radius==0)visit(gx,gy);
        else{for(int a=-radius;a<=radius;++a){visit(gx+a,gy-radius);visit(gx+a,gy+radius);}
            for(int b=-radius+1;b<radius;++b){visit(gx-radius,gy+b);visit(gx+radius,gy+b);}}
        // Use strict inequality to retain deterministic ties. The last
        // shortened cell still has its center inside that cell.
        const int64_t lower=int64_t(radius)*sf.step;
        if(out.count==4&&out.distance[3]<lower*lower)break;
    }return out;
}
inline Neighbors nearestComplete(const Surface& sf,int x,int y){
    return nearest(sf,x,y,[](size_t,const Sample& s){return s.complete;},sf.completeIndices);
}
inline Neighbors nearestReceiver(const Surface& sf,int x,int y){
    return nearest(sf,x,y,[](size_t,const Sample& s){return s.receiver!=State::Unknown;},sf.receiverIndices);
}
inline Value reconstruct(const Surface& sf,const Layer& l,int x,int y,const Neighbors& complete,Result* stats=nullptr){
    if(!l.bounds.contains(x,y))return {};
    // Complete observations are common to ALL layers. Incomplete paths
    // are known only for layers that actually had a hit. Merge their sparse
    // positive IDs into the same exact top four, never invent an absence.
    auto n=complete;
    for(size_t i:l.partial){if(stats)++stats->partialTests;insertNeighbor(n,sf,x,y,i);}
    if(!n.count)return {};
    const auto exact=n.ids[0];const auto& s0=sf.samples[exact];
    if(s0.x==x&&s0.y==y){
        const auto it=l.measured.find(exact);if(it!=l.measured.end())return it->second;
        Value zero;if(s0.complete)zero.state=State::Zero;return zero;
    }
    Value v;v.state=State::Reconstructed;v.object=l.key.object;v.material=l.key.material;
    const Value* seed=nullptr;size_t positive=0,uvCount=0;
    for(size_t j=0;j<n.count;++j){
        auto it=l.measured.find(n.ids[j]);if(it==l.measured.end())continue;
        const auto& a=it->second;
        if(!seed){seed=&a;v.primitive=a.primitive;}
        ++positive;v.depth+=a.depth;v.normal=v.normal+a.normal;v.opacity+=a.opacity;v.weight+=a.weight;
        // Never mix UV charts separated by a jump; average compatible known
        // UVs only. At a seam use the closest original value.
        if(a.primitive==seed->primitive||(std::abs(a.uv.x-seed->uv.x)<.5f&&std::abs(a.uv.y-seed->uv.y)<.5f)){v.uv=v.uv+a.uv;++uvCount;}
    }
    if(!positive){v={};v.state=State::Zero;return v;}
    const float k=1.f/float(positive);
    v.depth*=k;v.normal=norm(v.normal);v.opacity*=k;v.weight*=k;v.uv=v.uv/float(std::max<size_t>(1,uvCount));
    v.visibility=float(positive)/float(n.count); // observed zeros affect only visibility
    return v;
}
inline Value reconstruct(const Surface& sf,const Layer& l,int x,int y){
    return reconstruct(sf,l,x,y,nearestComplete(sf,x,y));
}
inline bool receiverAt(const Surface& sf,int x,int y,float& depth,float& weight,const Neighbors& n){
    if(!n.count)return false;
    const auto& first=sf.samples[n.ids[0]];
    if(first.x==x&&first.y==y){if(first.receiver==State::Zero)return false;depth=first.receiverDepth;weight=first.receiverWeight;return true;}
    depth=weight=0;size_t positive=0;
    for(size_t i=0;i<n.count;++i){const auto& s=sf.samples[n.ids[i]];if(s.receiver==State::Measured){depth+=s.receiverDepth;weight+=s.receiverWeight;++positive;}}
    if(!positive)return false;
    depth/=float(positive);weight/=float(positive);return true; // mask/alpha already accounts for absent receiver observations
}
inline bool receiverAt(const Surface& sf,int x,int y,float& depth,float& weight){
    return receiverAt(sf,x,y,depth,weight,nearestReceiver(sf,x,y));
}
}
