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
// The distance magnitudes do not enter the arithmetic average; only
// the ordered IDs and exact-hit flag matter. Never reuse a measured cell
// as an interpolated cell (or an explicitly observed zero as a hole).
inline bool sameStencil(const Neighbors& a,const Neighbors& b){
    if(a.count!=b.count)return false;
    if(!a.count)return true;
    if((a.distance[0]==0)!=(b.distance[0]==0))return false;
    for(size_t i=0;i<a.count;++i)if(a.ids[i]!=b.ids[i])return false;
    return true;
}
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
class NeighborIndex {
    struct Node {size_t id=0;int left=-1,right=-1;int minX=0,maxX=0,minY=0,maxY=0;};
    const Surface& sf_;std::vector<Node> nodes_;int root_=-1;
    int build(std::vector<size_t>& ids,size_t begin,size_t end,int axis){
        if(begin==end)return -1;
        const size_t mid=begin+(end-begin)/2;
        std::nth_element(ids.begin()+begin,ids.begin()+mid,ids.begin()+end,[&](size_t a,size_t b){
            const auto& x=sf_.samples[a];const auto& y=sf_.samples[b];
            const int v=axis?x.y:x.x,w=axis?y.y:y.x;return v<w||(v==w&&a<b);
        });
        const auto id=ids[mid];const auto& s=sf_.samples[id];const int at=int(nodes_.size());
        nodes_.push_back({id,-1,-1,s.x,s.x,s.y,s.y});
        const int l=build(ids,begin,mid,1-axis),r=build(ids,mid+1,end,1-axis);
        auto& n=nodes_[at];n.left=l;n.right=r;
        for(int k:{l,r})if(k>=0){const auto& c=nodes_[k];n.minX=std::min(n.minX,c.minX);n.maxX=std::max(n.maxX,c.maxX);n.minY=std::min(n.minY,c.minY);n.maxY=std::max(n.maxY,c.maxY);}
        return at;
    }
    int64_t distance(int id,int x,int y)const{
        if(id<0)return std::numeric_limits<int64_t>::max();
        const auto& n=nodes_[id];
        const int64_t dx=std::max({int64_t(0),int64_t(n.minX)-x,int64_t(x)-n.maxX});
        const int64_t dy=std::max({int64_t(0),int64_t(n.minY)-y,int64_t(y)-n.maxY});
        return dx*dx+dy*dy;
    }
    void visit(int id,int x,int y,Neighbors& out)const{
        if(id<0||(out.count==4&&distance(id,x,y)>out.distance[3]))return;
        const auto& n=nodes_[id];insertNeighbor(out,sf_,x,y,n.id);
        int l=n.left,r=n.right;if(distance(r,x,y)<distance(l,x,y))std::swap(l,r);
        visit(l,x,y,out);visit(r,x,y,out);
    }
public:
    NeighborIndex(const Surface& sf,const std::vector<size_t>& indices):sf_(sf){
        if(indices.size()<16||indices.size()*4>=sf.samples.size())return;
        auto ids=indices;nodes_.reserve(ids.size());root_=build(ids,0,ids.size(),0);
    }
    bool active()const{return root_>=0;}
    Neighbors query(int x,int y)const{Neighbors out;visit(root_,x,y,out);return out;}
};
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
        else{
            const int ax=std::max(-radius,-gx),bx=std::min(radius,sf.cols-1-gx);
            if(gy-radius>=0)for(int a=ax;a<=bx;++a)visit(gx+a,gy-radius);
            if(gy+radius<sf.rows)for(int a=ax;a<=bx;++a)visit(gx+a,gy+radius);
            const int ay=std::max(-radius+1,-gy),by=std::min(radius-1,sf.rows-1-gy);
            if(gx-radius>=0)for(int b=ay;b<=by;++b)visit(gx-radius,gy+b);
            if(gx+radius<sf.cols)for(int b=ay;b<=by;++b)visit(gx+radius,gy+b);
        }
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
    if(!l.bounds.contains(x,y)){
        Value v;
        if(complete.count&&complete.distance[0]==0&&sf.samples[complete.ids[0]].complete)v.state=State::Zero;
        return v;
    }
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
