#pragma once
#include "runtime.h"
#include <stdexcept>
namespace m3d::escargot {
constexpr int kTile=32;
constexpr size_t kMaxTileLayers=64;
constexpr size_t kMaxRecords=4u*1024u*1024u;
struct Packet {
    std::vector<float> data;
    uint64_t tiles=0,layerLists=0,observations=0,affineLayers=0;
    bool ok=false;std::string error;
    size_t size()const{return data.size()/4;}
    size_t add(float a=0,float b=0,float c=0,float d=0){
        if(size()>=kMaxRecords)throw std::runtime_error("Escargot compact packet limit");
        size_t i=size();data.insert(data.end(),{a,b,c,d});return i;
    }
    size_t reserveRecords(size_t n){
        if(n>kMaxRecords-size())throw std::runtime_error("Escargot compact packet limit");
        const auto i=size();data.resize(data.size()+4*n,0.f);return i;
    }
    void set(size_t i,float a,float b,float c,float d){data[4*i]=a;data[4*i+1]=b;data[4*i+2]=c;data[4*i+3]=d;}
    void vec(size_t i,V v,float a=0){set(i,v.x,v.y,v.z,a);}
};
inline int64_t minDistance(const Sample& s,Rect r){
    const int64_t dx=std::max({0,r.x-s.x,s.x-(r.x+r.w-1)});
    const int64_t dy=std::max({0,r.y-s.y,s.y-(r.y+r.h-1)});
    return dx*dx+dy*dy;
}
inline int64_t maxDistance(const Sample& s,Rect r){
    const int64_t dx=std::max(std::abs(r.x-s.x),std::abs(r.x+r.w-1-s.x));
    const int64_t dy=std::max(std::abs(r.y-s.y),std::abs(r.y+r.h-1-s.y));
    return dx*dx+dy*dy;
}
// For any point in the tile, four known points have distances <= limit.
// All points strictly beyond limit cannot belong to its exact top four.
// Keep ties, including observed zeros. No approximate neighbor selection.
inline std::vector<size_t> shortlist(const Surface& s,const std::vector<size_t>& ids,Rect r){
    if(ids.size()<=4)return ids;
    std::array<int64_t,4> best;best.fill(std::numeric_limits<int64_t>::max());
    for(auto i:ids){auto v=maxDistance(s.samples[i],r);for(size_t j=0;j<4;++j)if(v<best[j]){for(size_t k=3;k>j;--k)best[k]=best[k-1];best[j]=v;break;}}
    std::vector<size_t> out;
    for(auto i:ids)if(minDistance(s.samples[i],r)<=best[3])out.push_back(i);
    return out;
}
inline bool overlaps(Rect a,Rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
struct Affine {
    V origin{},normal{},du{},dv{};float u=0,v=0;bool valid=false;
    V uv(V p)const{return {u+dot(du,p-origin),v+dot(dv,p-origin),0};}
};
inline Affine affine(const Scene& scene,const Layer& l){
    Affine a;
    if(l.measured.size()<4)return a;
    const auto id=l.measured.begin()->second.primitive;
    if(id>=scene.obj.size())return a;
    const auto& t=scene.obj[id];const V x=t.b-t.a,y=t.c-t.a;
    const float xx=dot(x,x),xy=dot(x,y),yy=dot(y,y),det=xx*yy-xy*xy;
    if(det<=1e-12f)return a;
    const V gx=(x*yy-y*xy)/det,gy=(y*xx-x*xy)/det;
    a.origin=t.a;a.normal=norm(cross(x,y));a.u=t.uv0.x;a.v=t.uv0.y;
    a.du=gx*(t.uv1.x-a.u)+gy*(t.uv2.x-a.u);
    a.dv=gx*(t.uv1.y-a.v)+gy*(t.uv2.y-a.v);
    if(!finite(a.du)||!finite(a.dv))return {};
    for(const auto& kv:l.measured){
        if(kv.second.primitive>=scene.obj.size())return {};
        const auto& q=scene.obj[kv.second.primitive];
        for(const auto& pv:{std::pair<V,V>{q.a,q.uv0},{q.b,q.uv1},{q.c,q.uv2}}){
            const auto uv=a.uv(pv.first);
            if(std::abs(dot(pv.first-a.origin,a.normal))>.002f||
                std::abs(uv.x-pv.second.x)>.002f||std::abs(uv.y-pv.second.y)>.002f)return {};
        }
    }
    a.valid=true;return a;
}
inline Packet packGpu(const Result& frame,const Scene& scene,size_t objectCount){
    Packet p;
    try{
        // [0]: light direction/day; [1]: max object ID, tile size, format, unused.
        // Two float4 per receiver, indexed by exact source-object ID.
        p.data.reserve(4*std::min(kMaxRecords,std::max<size_t>(2+2*(objectCount+1),65536)));
        p.reserveRecords(2+2*(objectCount+1));float day;V sunDir;
        escargotLightingSnapshot(day,sunDir);p.vec(0,sunDir,day);p.set(1,float(objectCount),float(kTile),1,0);
        for(const auto& sf:frame.surfaces){
            if(sf.deferred||sf.layers.empty()||!sf.candidate.bounds.valid())continue;
            const auto r=sf.candidate.bounds;const auto object=sf.candidate.object;
            if(!object||object>objectCount)throw std::runtime_error("Invalid receiver ID");
            const int cols=(r.w+kTile-1)/kTile,rows=(r.h+kTile-1)/kTile;
            const auto tileBase=p.reserveRecords(size_t(cols)*rows);
            const size_t rec=2+2*size_t(object);
            p.set(rec,float(r.x),float(r.y),float(r.w),float(r.h));
            const bool coverage=sf.sameKnownSet&&std::all_of(sf.layers.begin(),sf.layers.end(),[](const auto& kv){return kv.second.partial.empty();});
            p.set(rec+1,float(tileBase),float(cols),float(sf.candidate.material+1),coverage?1.f:0.f);
            struct LayerInfo {const Layer* l;Affine a;std::vector<const Value*> lookup;};
            std::vector<LayerInfo> layers;layers.reserve(sf.layers.size());
            // Bounded dense lookup removes repeated map searches across tiles.
            const bool denseLookup=sf.samples.size()<=262144/std::max<size_t>(1,sf.layers.size());
            for(const auto& kv:sf.layers){
                auto a=affine(scene,kv.second);if(a.valid)++p.affineLayers;
                LayerInfo info{&kv.second,a,{}};
                if(denseLookup){info.lookup.resize(sf.samples.size(),nullptr);for(const auto& v:kv.second.measured)info.lookup[v.first]=&v.second;}
                layers.push_back(std::move(info));
            }
            auto findValue=[](const LayerInfo& info,size_t id)->const Value*{
                if(!info.lookup.empty())return info.lookup[id];
                const auto it=info.l->measured.find(id);return it==info.l->measured.end()?nullptr:&it->second;
            };
            for(int ty=0;ty<rows;++ty)for(int tx=0;tx<cols;++tx){
                Rect tile{r.x+tx*kTile,r.y+ty*kTile,std::min(kTile,r.w-tx*kTile),std::min(kTile,r.h-ty*kTile)};
                auto receivers=shortlist(sf,sf.receiverIndices,tile);
                auto common=coverage?std::vector<size_t>{}:shortlist(sf,sf.completeIndices,tile);
                if(receivers.empty())continue;
                struct Entry{const LayerInfo* info;std::vector<size_t> ids;};
                std::vector<Entry> active;
                for(const auto& item:layers){
                    const auto& l=*item.l;if(!overlaps(l.bounds,tile))continue;
                    // Complete paths share exactly the same ordered shortlist.
                    // Only partial paths need a separate spatial search.
                    std::vector<size_t> ids;
                    if(!coverage){ids=common;if(!l.partial.empty()){ids.insert(ids.end(),l.partial.begin(),l.partial.end());ids=shortlist(sf,ids,tile);}}
                    const auto& chosen=coverage?receivers:ids;
                    if(std::none_of(chosen.begin(),chosen.end(),[&](size_t i){return findValue(item,i)!=nullptr;}))continue;
                    active.push_back({&item,std::move(ids)});
                }
                if(active.empty())continue;
                if(active.size()>kMaxTileLayers)throw std::runtime_error("Escargot tile exceeds 64 layers; no layers silently discarded");
                const auto receiverStart=p.size();
                for(auto id:receivers){const auto& s=sf.samples[id];p.add(float(s.x),float(s.y),float(id),s.receiver==State::Measured?1.f:0.f);
                    p.add(s.position.x,s.position.y,s.position.z,s.receiverWeight);p.add(s.normal.x,s.normal.y,s.normal.z,s.receiverDepth);}
                const auto layerStart=p.reserveRecords(active.size()*7);
                for(size_t j=0;j<active.size();++j){
                    const auto& entry=active[j];const auto& l=*entry.info->l;const auto& a=entry.info->a;
                    const auto& chosen=coverage?receivers:entry.ids;
                    const auto& mat=scene.mat[l.key.material];const auto first=p.size(),base=layerStart+7*j;
                    p.set(base,float(first),float(chosen.size()),float(l.key.material),a.valid?1.f:0.f);
                    p.set(base+1,float(l.bounds.x),float(l.bounds.y),float(l.bounds.w),float(l.bounds.h));
                    p.vec(base+2,a.origin,a.u);p.vec(base+3,a.normal,a.v);p.vec(base+4,a.du);p.vec(base+5,a.dv);
                    p.vec(base+6,mat.emissionColor*std::max(0.f,mat.emissionIntensity),mat.imageAlbedo?1.f:0.f);
                    for(auto id:chosen){const auto& s=sf.samples[id];const auto* value=findValue(*entry.info,id);
                        const bool hit=value!=nullptr;
                        if(!coverage)p.add(float(s.x),float(s.y),float(id),hit?1.f:0.f);
                        // On complete paths the receiver shortlist already
                        // supplies positions/IDs. Only 2 float4 per layer cell.
                        if(hit){const auto& v=*value;p.add(v.uv.x,v.uv.y,v.depth,v.opacity);p.vec(p.add(),v.normal,float(v.primitive+1u));}
                        else {p.add();p.add();}
                        ++p.observations;
                    }
                    ++p.layerLists;
                }
                p.set(tileBase+size_t(ty)*cols+tx,float(receiverStart),float(receivers.size()),float(layerStart),float(active.size()));++p.tiles;
            }
        }
        p.ok=true;
    }catch(const std::exception& ex){p.error=ex.what();p.ok=false;}
    return p;
}
}
