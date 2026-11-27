#pragma once
#include "matrix.h"
#include <future>
#include <thread>
namespace m3d::escargot {
inline float linear(float x){x=sat(x);return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
template<size_t N> inline const std::array<float,N>& linearLut(){
    static const auto values=[](){std::array<float,N> a{};for(size_t i=0;i<N;++i)a[i]=linear(float(i)/float(N-1));return a;}();
    return values;
}
inline V decode(uint32_t p){const auto& a=linearLut<256>();return {a[(p>>16)&255],a[(p>>8)&255],a[p&255]};}
inline V decode565(uint16_t p){const auto& a=linearLut<32>();const auto& b=linearLut<64>();return {a[(p>>11)&31],b[(p>>5)&63],a[p&31]};}
inline V sourceTexture(const Material& m,V uv){
    auto sample=[&](uint32_t n,auto get){
        if(!n||!std::isfinite(uv.x)||!std::isfinite(uv.y))return V{};
        const float u=m.imageAlbedo?sat(uv.x):uv.x-std::floor(uv.x),v=m.imageAlbedo?sat(uv.y):uv.y-std::floor(uv.y);
        const float fx=u*n-.5f,fy=v*n-.5f;const int x=int(std::floor(fx)),y=int(std::floor(fy));
        auto idx=[&](int q){return m.imageAlbedo?std::clamp(q,0,int(n)-1):((q%int(n))+int(n))%int(n);};
        const float a=fx-x,b=fy-y;
        return get(size_t(idx(y))*n+idx(x))*((1-a)*(1-b))+get(size_t(idx(y))*n+idx(x+1))*(a*(1-b))+
            get(size_t(idx(y+1))*n+idx(x))*((1-a)*b)+get(size_t(idx(y+1))*n+idx(x+1))*(a*b);
    };
    if(!m.colorMip.empty()){const auto& l=m.colorMip.front();if(l.p.size()==size_t(l.size)*l.size)return sample(l.size,[&](size_t i){return decode(l.p[i]);});}
    if(!m.mip.empty()){const auto& l=m.mip.front();if(l.p.size()==size_t(l.size)*l.size)return sample(l.size,[&](size_t i){return decode565(l.p[i]);});}
    return {.2f,.2f,.2f};
}
// Reference radiance proxy, not the existing LumaTrace lighting shader.
// Crucially texture is sampled AFTER UV reconstruction and depth ordering.
inline thread_local float proxyDaylight=1.f;
inline thread_local V proxySun=norm({.70f,.78f,-.714f});
inline V sourceColor(const Material& m,const Value& v){
    const V base=sourceTexture(m,v.uv);
    const float diffuse=.12f+.35f*proxyDaylight+.48f*proxyDaylight*sat(dot(v.normal,proxySun));
    return base*diffuse+m.emissionColor*std::max(0.f,m.emissionIntensity);
}
struct Contribution {Value value;const Layer* layer=nullptr;uint8_t mask=0;};
inline void over(Pixel& p,V color,float alpha){
    p.rgb=color*alpha+p.rgb*(1-alpha);p.alpha=alpha+p.alpha*(1-alpha);
}
struct RowEvent {int x;size_t layer;bool enter;};
// Build disjoint half-open spans with identical layer membership. The
// stable index order is the LayerKey order, including depth ties.
inline std::vector<RowEvent> rowEvents(const std::vector<const Layer*>& layers){
    std::vector<RowEvent> events;events.reserve(layers.size()*2);
    for(size_t i=0;i<layers.size();++i){
        const auto& b=layers[i]->bounds;
        events.push_back({b.x,i,true});events.push_back({b.x+b.w,i,false});
    }
    std::sort(events.begin(),events.end(),[](const RowEvent& a,const RowEvent& b){
        if(a.x!=b.x)return a.x<b.x;
        if(a.layer!=b.layer)return a.layer<b.layer;
        return a.enter<b.enter;
    });return events;
}
inline void composeRows(Surface& sf,const Scene& scene,Result& stats,bool reuse,int firstRow,int endRow,
                        const NeighborIndex& completeIndex,const NeighborIndex& receiverIndex){
    const Rect r=sf.candidate.bounds;
    std::vector<Contribution> values;values.reserve(sf.layers.size());
    std::vector<const Layer*> rowLayers,active;rowLayers.reserve(sf.layers.size());active.reserve(sf.layers.size());
    // Cache only within an unchanged span, only if no partial layer may
    // alter its neighbors. No cross-frame reuse, no approximate cache.
    const bool noPartial=std::all_of(sf.layers.begin(),sf.layers.end(),[](const auto& kv){return kv.second.partial.empty();});
    const bool canReuse=reuse&&sf.sameKnownSet&&noPartial;
    for(int y=firstRow;y<endRow;++y){
        rowLayers.clear();active.clear();
        for(const auto& kv:sf.layers){const auto& b=kv.second.bounds;if(y>=b.y&&y<b.y+b.h)rowLayers.push_back(&kv.second);}
        const auto events=rowEvents(rowLayers);
        std::vector<uint8_t> member(rowLayers.size(),0);
        size_t event=0;int x=r.x;
        while(x<r.x+r.w){
            while(event<events.size()&&events[event].x<=x){
                member[events[event].layer]=events[event].enter?1:0;++event;
            }
            const int end=event<events.size()?std::min(r.x+r.w,events[event].x):r.x+r.w;
            active.clear();for(size_t i=0;i<member.size();++i)if(member[i])active.push_back(rowLayers[i]);
            if(active.empty()){stats.emptySpanCells+=uint64_t(end-x);x=end;continue;}
            bool cached=false;Neighbors previous{};Pixel previousPixel{};
            for(;x<end;++x){
            auto& p=sf.image[size_t(y-r.y)*r.w+x-r.x];
            const auto complete=completeIndex.active()?completeIndex.query(x,y):nearestComplete(sf,x,y);++stats.completeSearches;
            if(canReuse&&cached&&sameStencil(previous,complete)){
                p=previousPixel;++stats.reusedCells;continue;
            }
            ++stats.composedCells;
            auto remember=[&]{if(canReuse){cached=true;previous=complete;previousPixel=p;}};
            auto receiver=complete;if(!sf.sameKnownSet){receiver=receiverIndex.active()?receiverIndex.query(x,y):nearestReceiver(sf,x,y);++stats.receiverSearches;}
            if(!receiverAt(sf,x,y,p.receiverDepth,p.weight,receiver)||p.weight<=0){++stats.zero;remember();continue;}
            values.clear();
            // A common four-observation coverage mask is exact for original
            // complete paths. Unknown partial observations keep the original
            // per-layer reconstruction fallback; never fabricate an occluder.
            const bool coverage=sf.sameKnownSet&&noPartial&&complete.count>0;
            const size_t lanes=coverage?(complete.distance[0]==0?1:complete.count):0;
            for(const auto* l:active){
                ++stats.layerVisits;auto v=reconstruct(sf,*l,x,y,complete,&stats);
                if(v.state==State::Zero){++stats.zero;continue;}
                if(!v.object||v.visibility<=0||v.material>=scene.mat.size())continue;
                if(v.state==State::Reconstructed)++stats.reconstructed;
                uint8_t mask=0;
                for(size_t i=0;i<lanes;++i)if(l->measured.count(complete.ids[i]))mask|=uint8_t(1u<<i);
                if(coverage&&!mask)continue;
                values.push_back({v,l,mask});
            }
            // Optical depth is local to this destination cell. Stable ties
            // follow the deterministic LayerKey order (map / row insertion).
            if(values.size()>1)std::sort(values.begin(),values.end(),[](const Contribution& a,const Contribution& b){
                if(a.value.depth!=b.value.depth)return a.value.depth>b.value.depth;
                return a.layer->key<b.layer->key; // same deterministic order, no stable_sort scratch allocation
            });
            if(coverage){
                ++stats.coverageCells;
                // Avoid dark halos when two opaque objects each cover half
                // the four samples: their disjoint masks union to alpha=1,
                // not 0.75. Each layer's UV is still reconstructed once.
                uint8_t blocked=0;
                for(auto i=values.rbegin();i!=values.rend();++i){
                    i->mask=uint8_t(i->mask&~blocked);
                    if(i->value.opacity>=1.f)blocked|=i->mask;
                }
                std::array<Pixel,4> lane{};
                for(const auto& c:values){
                    if(!c.mask){++stats.opaqueSkipped;continue;}
                    const auto& v=c.value;const float a=sat(v.opacity);
                    if(a<=0)continue;
                    const V color=sourceColor(scene.mat[v.material],v);++stats.textureSamples;
                    for(size_t i=0;i<lanes;++i)if(c.mask&(1u<<i))over(lane[i],color,a);
                }
                for(size_t i=0;i<lanes;++i){p.rgb=p.rgb+lane[i].rgb;p.alpha+=lane[i].alpha;}
                p.rgb=p.rgb/float(lanes);p.alpha/=float(lanes);
            }else{
                ++stats.fallbackCells;
                size_t first=0;
                for(size_t i=values.size();i>0;--i)if(values[i-1].value.visibility*values[i-1].value.opacity>=1.f){first=i-1;break;}
                stats.opaqueSkipped+=first;
                for(size_t i=first;i<values.size();++i){
                    const auto& v=values[i].value;const float a=sat(v.visibility*v.opacity);if(a<=0)continue;
                    over(p,sourceColor(scene.mat[v.material],v),a);++stats.textureSamples;
                }
            }
            // RGB is premultiplied; optical weight remains separate for compositor.
            remember();
            }
        }
    }
}
// Independent rows, immutable observations and lighting snapshot, disjoint
// writes. Parallelism is deliberately bounded; small footprints stay serial.
inline void mergeCompositionStats(Result& a,const Result& b){
    a.reconstructed+=b.reconstructed;a.zero+=b.zero;
    a.completeSearches+=b.completeSearches;a.receiverSearches+=b.receiverSearches;
    a.partialTests+=b.partialTests;a.layerVisits+=b.layerVisits;
    a.textureSamples+=b.textureSamples;a.opaqueSkipped+=b.opaqueSkipped;
    a.coverageCells+=b.coverageCells;a.fallbackCells+=b.fallbackCells;
    a.reusedCells+=b.reusedCells;a.composedCells+=b.composedCells;a.emptySpanCells+=b.emptySpanCells;
}
inline void compose(Surface& sf,const Scene& scene,Result& stats,bool reuse=true,unsigned workerLimit=4){
    const Rect r=sf.candidate.bounds;
    sf.image.clear();if(!r.valid()||sf.layers.empty())return;
    sf.image.resize(size_t(r.w)*r.h);
    const NeighborIndex completeIndex(sf,sf.completeIndices),receiverIndex(sf,sf.receiverIndices);
    const unsigned workers=std::max(1u,std::min({workerLimit,std::max(1u,std::thread::hardware_concurrency()),
        unsigned(r.h),unsigned(std::max<uint64_t>(1,uint64_t(r.w)*r.h/65536))}));
    if(workers==1){composeRows(sf,scene,stats,reuse,r.y,r.y+r.h,completeIndex,receiverIndex);return;}
    const float day=proxyDaylight;const V direction=proxySun;
    std::vector<std::future<Result>> futures;futures.reserve(workers-1);
    auto task=[&](unsigned i){
        proxyDaylight=day;proxySun=direction;Result local;
        const int y0=r.y+int(uint64_t(r.h)*i/workers),y1=r.y+int(uint64_t(r.h)*(i+1)/workers);
        composeRows(sf,scene,local,reuse,y0,y1,completeIndex,receiverIndex);
        return local;
    };
    for(unsigned i=1;i<workers;++i)futures.push_back(std::async(std::launch::async,task,i));
    mergeCompositionStats(stats,task(0));
    for(auto& future:futures)mergeCompositionStats(stats,future.get());
}
inline std::vector<Pixel> composeScreen(const Result& frame,int w,int h){
    // Diagnostic projection only. Native renderer must match actual receiving
    // surface depth/ID; this projection chooses the closest reconstructed one.
    std::vector<Pixel> out(size_t(w)*h);
    for(const auto& s:frame.surfaces){if(s.image.empty())continue;const auto r=s.candidate.bounds;
        for(int y=0;y<r.h;++y)for(int x=0;x<r.w;++x){
            const auto& p=s.image[size_t(y)*r.w+x];if(p.alpha<=0||p.weight<=0)continue;
            auto& q=out[size_t(y+r.y)*w+x+r.x];
            if(!q.alpha||p.receiverDepth<q.receiverDepth){q=p;q.receiverMaterial=s.candidate.material;q.receiverObject=s.candidate.object;}
        }
    }return out;
}
}
