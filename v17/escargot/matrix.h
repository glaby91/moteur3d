#pragma once
#include "sampling.h"
namespace m3d::escargot {
inline void makeLayers(Surface& sf){
    for(size_t i=0;i<sf.samples.size();++i){
        const auto& s=sf.samples[i];
        for(const auto& kv:s.values){
            auto& l=sf.layers[kv.first];l.key=kv.first;l.measured.emplace(i,kv.second);
            if(!l.bounds.valid())l.bounds={s.x,s.y,1,1};
            else{const int x=std::min(l.bounds.x,s.x),y=std::min(l.bounds.y,s.y);
                const int right=std::max(l.bounds.x+l.bounds.w,s.x+1),bottom=std::max(l.bounds.y+l.bounds.h,s.y+1);
                l.bounds={x,y,right-x,bottom-y};}
        }
    }
    const auto r=sf.candidate.bounds;
    for(auto& kv:sf.layers){auto& b=kv.second.bounds;const int x=std::max(r.x,b.x-sf.step),y=std::max(r.y,b.y-sf.step);
        const int right=std::min(r.x+r.w,b.x+b.w+sf.step),bottom=std::min(r.y+r.h,b.y+b.h+sf.step);b={x,y,right-x,bottom-y};}
}
// Exactly four nearest original known cells (or fewer if insufficient).
// Unknown is not Zero. Neither interpolated cells nor unknown observations
// participate. ID zero is never assigned a source UV or interpolated away.
struct Neighbors {std::array<size_t,4> ids{};size_t count=0;};
template<class Known> inline Neighbors nearest(const Surface& sf,int x,int y,Known known){
    Neighbors out;std::array<std::pair<int64_t,size_t>,4> best{};
    for(auto& b:best)b={std::numeric_limits<int64_t>::max(),0};
    // Fixed lattice permits an early stop once the unvisited grid cannot
    // contain a closer sample. Partial/budget-limited paths are skipped.
    const int gx=std::clamp((x-sf.candidate.bounds.x)/sf.step,0,sf.cols-1);
    const int gy=std::clamp((y-sf.candidate.bounds.y)/sf.step,0,sf.rows-1);
    auto visit=[&](int a,int b){
        if(a<0||b<0||a>=sf.cols||b>=sf.rows)return;
        const size_t i=size_t(b)*sf.cols+a;const auto& s=sf.samples[i];if(!known(i,s))return;
        const int64_t dx=s.x-x,dy=s.y-y;const std::pair<int64_t,size_t> q{dx*dx+dy*dy,i};
        for(size_t k=0;k<4;++k)if(q<best[k]){for(size_t j=3;j>k;--j)best[j]=best[j-1];best[k]=q;break;}
    };
    for(int radius=0;radius<std::max(sf.cols,sf.rows);++radius){
        if(radius==0)visit(gx,gy);
        else{for(int a=-radius;a<=radius;++a){visit(gx+a,gy-radius);visit(gx+a,gy+radius);}
            for(int b=-radius+1;b<radius;++b){visit(gx-radius,gy+b);visit(gx+radius,gy+b);}}
        const int64_t lower=int64_t(radius)*sf.step;
        if(best[3].first<lower*lower)break;
    }
    for(auto b:best)if(b.first!=std::numeric_limits<int64_t>::max())out.ids[out.count++]=b.second;
    return out;
}
inline Value reconstruct(const Surface& sf,const Layer& l,int x,int y){
    if(!l.bounds.contains(x,y))return {};
    auto n=nearest(sf,x,y,[&](size_t i,const Sample& s){return s.complete||l.measured.count(i)!=0;});
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
inline bool receiverAt(const Surface& sf,int x,int y,float& depth,float& weight){
    const auto n=nearest(sf,x,y,[](size_t,const Sample& s){return s.receiver!=State::Unknown;});
    if(!n.count)return false;
    const auto& first=sf.samples[n.ids[0]];
    if(first.x==x&&first.y==y){if(first.receiver==State::Zero)return false;depth=first.receiverDepth;weight=first.receiverWeight;return true;}
    depth=weight=0;size_t positive=0;
    for(size_t i=0;i<n.count;++i){const auto& s=sf.samples[n.ids[i]];if(s.receiver==State::Measured){depth+=s.receiverDepth;weight+=s.receiverWeight;++positive;}}
    if(!positive)return false;
    depth/=float(positive);weight/=float(n.count);return true;
}
}
