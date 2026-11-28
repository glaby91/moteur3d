#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>
#include <utility>
#include <cmath>
namespace m3d::escargot::object_visibility {
struct Stats {int projected=0,seedSamples=0,measured=0,zeroSplits=0,zeroProbes=0,mixedSplits=0,otherSplits=0;bool exhaustive=false;};
struct Cell {std::vector<int> ids;int known=0,positive=0;};
inline int64_t distance(int a,int b,int w){int64_t dx=a%w-b%w,dy=a/w-b/w;return dx*dx+dy*dy;}
inline std::pair<Cell,Cell> split(Cell c,int w){
    int x0=w,x1=0,y0=std::numeric_limits<int>::max(),y1=0;
    for(int i:c.ids){x0=std::min(x0,i%w);x1=std::max(x1,i%w);y0=std::min(y0,i/w);y1=std::max(y1,i/w);}
    bool vertical=y1-y0>x1-x0;
    auto key=[&](int i){return std::pair<int,int>{vertical?i/w:i%w,i};};
    std::sort(c.ids.begin(),c.ids.end(),[&](int a,int b){return key(a)<key(b);});
    auto mid=c.ids.begin()+c.ids.size()/2;
    Cell a,b;a.ids.assign(c.ids.begin(),mid);b.ids.assign(mid,c.ids.end());return {std::move(a),std::move(b)};
}
// All points supplied are geometrically valid receiver/source intersections.
// No ray lookup at a rectangle corner outside the reflected object.
// Budget is a count of observations (caller reserves two BVH queries each).
// Unknown and measured zero are independent; no oracle used for scheduling.
template<class Trace> Stats reconstruct(int w,const std::vector<int>& domain,int budget,Trace&& trace,std::vector<float>& mask){
    Stats stats;stats.projected=int(domain.size());if(domain.empty()||budget<=0)return stats;
    int cap=std::min(budget,int(domain.size()));std::vector<int8_t> values(mask.size(),-1);std::vector<int> measured;measured.reserve(cap);
    auto probe=[&](int i){
        if(values[i]>=0||int(measured.size())>=cap)return false;
        int z=trace(i);if(z!=0&&z!=1)return false;
        values[i]=int8_t(z);measured.push_back(i);return true;
    };
    if(cap==int(domain.size())){
        stats.exhaustive=true;
        for(int i:domain)if(probe(i))mask[i]=float(values[i]);
        stats.measured=int(measured.size());stats.seedSamples=std::min(32,stats.measured);return stats;
    }
    std::vector<Cell> cells;cells.push_back({domain});
    const int seed=std::min(cap,std::max(16,std::min(128,cap/4)));
    while(int(cells.size())<seed){
        auto it=std::max_element(cells.begin(),cells.end(),[](const Cell& a,const Cell& b){return a.ids.size()<b.ids.size();});
        if(it->ids.size()<2)break;
        auto p=split(std::move(*it),w);cells.erase(it);cells.push_back(std::move(p.first));cells.push_back(std::move(p.second));
    }
    auto ensure=[&](Cell& c){
        std::vector<int> known;for(int i:c.ids)if(values[i]>=0)known.push_back(i);
        int x0=w,x1=0,y0=std::numeric_limits<int>::max(),y1=0;
        for(int i:c.ids){x0=std::min(x0,i%w);x1=std::max(x1,i%w);y0=std::min(y0,i/w);y1=std::max(y1,i/w);}
        while(known.size()<2&&int(measured.size())<cap){
            int best=-1;int64_t score=known.empty()?std::numeric_limits<int64_t>::max():-1;
            for(int i:c.ids)if(values[i]<0){
                int64_t d;
                if(known.empty()){int64_t dx=2*(i%w)-x0-x1,dy=2*(i/w)-y0-y1;d=dx*dx+dy*dy;}
                else{d=std::numeric_limits<int64_t>::max();for(int j:known)d=std::min(d,distance(i,j,w));}
                if(best<0||(known.empty()?d<score:d>score)){best=i;score=d;}
            }
            if(best<0||!probe(best))break;
            known.push_back(best);
        }
        c.known=int(known.size());c.positive=0;for(int i:known)c.positive+=values[i];
    };
    // Finish every coarse cell before refinement: minimum 32 observations
    // when the projected footprint and conservative budget permit it.
    for(auto& c:cells)ensure(c);
    stats.seedSamples=int(measured.size());
    int round=0;
    while(int(measured.size())<cap){
        int best=-1;int64_t score=-1;bool reserveZero=round%3==0;
        for(int pass=0;pass<2&&best<0;++pass){
            for(int i=0;i<int(cells.size());++i){
                const auto& c=cells[i];if(c.ids.size()<2||c.known==int(c.ids.size()))continue;
                bool zero=c.known>0&&c.positive==0;
                if(reserveZero&&pass==0&&!zero)continue;
                bool mixed=c.positive>0&&c.positive<c.known;
                int64_t weight=int64_t(c.ids.size())*(mixed?4:1);
                if(weight>score){best=i;score=weight;}
            }
            if(!reserveZero)break;
        }
        if(best<0)break;
        const auto& old=cells[best];bool zero=old.known>0&&old.positive==0,mixed=old.positive>0&&old.positive<old.known;
        int before=int(measured.size());
        auto p=split(std::move(cells[best]),w);cells.erase(cells.begin()+best);
        ensure(p.first);ensure(p.second);cells.push_back(std::move(p.first));cells.push_back(std::move(p.second));
        if(zero){++stats.zeroSplits;stats.zeroProbes+=int(measured.size())-before;}
        else if(mixed)++stats.mixedSplits;else ++stats.otherSplits;
        ++round;
    }
    stats.measured=int(measured.size());
    // Shared global stencil for the visibility scalar only; UV projection
    // is not touched. No independent tile-local texture or mask transforms.
    // Exact measured values override the interpolation. No samples => unknown.
    for(int i:domain){
        if(values[i]>=0){mask[i]=float(values[i]);continue;}
        std::array<std::pair<int64_t,int>,4> nearest;
        nearest.fill({std::numeric_limits<int64_t>::max(),-1});
        for(int j:measured){
            auto p=std::pair<int64_t,int>{distance(i,j,w),j};
            if(p>=nearest.back())continue;
            for(int k=0;k<4;++k)if(p<nearest[k]){for(int l=3;l>k;--l)nearest[l]=nearest[l-1];nearest[k]=p;break;}
        }
        double sum=0,weight=0;for(auto p:nearest)if(p.second>=0){double z=1./(1.+double(p.first));sum+=z*values[p.second];weight+=z;}
        if(weight>0)mask[i]=float(sum/weight);
    }
    return stats;
}
}
