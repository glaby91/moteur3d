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
inline Packet packGpu(const Result& frame,const Scene& scene,size_t objectCount,int tileSize=kTile,bool scalarNearest=false){
    Packet p;
    try{
        if(tileSize!=8&&tileSize!=16&&tileSize!=32)throw std::runtime_error("Invalid tile size");
        // [0]: light direction/day; [1]: max object ID, tile size, format, unused.
        // Two float4 per receiver, indexed by exact source-object ID.
        p.data.reserve(4*std::min(kMaxRecords,std::max<size_t>(2+2*(objectCount+1),65536)));
        p.reserveRecords(2+2*(objectCount+1));float day;V sunDir;
        escargotLightingSnapshot(day,sunDir);p.vec(0,sunDir,day);p.set(1,float(objectCount),float(tileSize),1,scalarNearest?1.f:0.f);
        for(const auto& sf:frame.surfaces){
            if(sf.deferred||sf.layers.empty()||!sf.candidate.bounds.valid())continue;
            const auto r=sf.candidate.bounds;const auto object=sf.candidate.object;
            if(!object||object>objectCount)throw std::runtime_error("Invalid receiver ID");
            const int cols=(r.w+tileSize-1)/tileSize,rows=(r.h+tileSize-1)/tileSize;
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
                Rect tile{r.x+tx*tileSize,r.y+ty*tileSize,std::min(tileSize,r.w-tx*tileSize),std::min(tileSize,r.h-ty*tileSize)};
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

// F input: one copy of each observation per receiver/layer, not one per
// tile. GPU computes the exact conservative shortlists and final packet.
// Source layer grouping/affine metadata remain CPU-side in this phase.
struct BuildPlan {EscargotPacketBuild native;bool ok=false;std::string error;uint32_t regions=0,regionalSurfaces=0;bool regionalFallback=false;};
inline BuildPlan planGpuBuild(const Result& frame,const Scene& scene,size_t objectCount,int tileSize=16,bool scalarNearest=true,bool regionSearch=true){
    BuildPlan plan;Packet p;
    try{
        if(tileSize!=8&&tileSize!=16&&tileSize!=32)throw std::runtime_error("Invalid optical tile size");
        if(objectCount>=kMaxRecords/2-2)throw std::runtime_error("Too many receiver IDs");
        p.reserveRecords(2+2*(objectCount+1));float day;V sunDir;escargotLightingSnapshot(day,sunDir);
        p.vec(0,sunDir,day);p.set(1,float(objectCount),float(tileSize),1,scalarNearest?1.f:0.f);
        struct SurfaceJob {const Surface* sf;size_t tileBase;int cols,rows;};
        std::vector<SurfaceJob> surfaces;uint64_t jobs=0;
        for(const auto& sf:frame.surfaces){
            if(sf.deferred||sf.layers.empty()||!sf.candidate.bounds.valid()||sf.receiverIndices.empty())continue;
            const auto r=sf.candidate.bounds;auto object=sf.candidate.object;
            if(!object||object>objectCount)throw std::runtime_error("Invalid receiver ID");
            const int cols=(r.w+tileSize-1)/tileSize,rows=(r.h+tileSize-1)/tileSize;
            const auto tileBase=p.reserveRecords(size_t(cols)*rows);
            jobs+=uint64_t(cols)*rows;
            if(jobs>65535ull*32)throw std::runtime_error("Too many optical tile jobs");
            const bool coverage=sf.sameKnownSet&&std::all_of(sf.layers.begin(),sf.layers.end(),[](const auto& kv){return kv.second.partial.empty();});
            const size_t rec=2+2*size_t(object);
            p.set(rec,float(r.x),float(r.y),float(r.w),float(r.h));
            p.set(rec+1,float(tileBase),float(cols),float(sf.candidate.material+1),coverage?1.f:0.f);
            surfaces.push_back({&sf,tileBase,cols,rows});
        }
        plan.native.prefixRecords=uint32_t(p.size());
        plan.native.jobBase=uint32_t(p.reserveRecords(size_t(jobs)*3));plan.native.jobCount=uint32_t(jobs);
        size_t job=0;uint64_t allocationUpper=plan.native.prefixRecords;
        for(const auto& item:surfaces){
            const auto& sf=*item.sf;const auto r=sf.candidate.bounds;
            const bool coverage=p.data[4*(2+2*size_t(sf.candidate.object)+1)+3]>.5f;
            struct SourceLayer {const Layer* l;Affine a;std::vector<size_t> ids;};
            std::vector<SourceLayer> sourceLayers;sourceLayers.reserve(sf.layers.size());
            for(const auto& kv:sf.layers){
                SourceLayer item{&kv.second,affine(scene,kv.second),{}};
                if(!coverage){item.ids=sf.completeIndices;item.ids.insert(item.ids.end(),kv.second.partial.begin(),kv.second.partial.end());}
                sourceLayers.push_back(std::move(item));
            }
            // Regions are only a conservative search index, never optical
            // geometry or new samples. Keep original order and all ties.
            const bool regional=regionSearch&&sf.receiverIndices.size()>=128&&item.cols*item.rows>16;
            if(regional)++plan.regionalSurfaces;
            const int blockX=regional?std::max(1,128/tileSize):item.cols,blockY=regional?std::max(1,128/tileSize):item.rows;
            for(int by=0;by<item.rows;by+=blockY)for(int bx=0;bx<item.cols;bx+=blockX){
                ++plan.regions;
                const int ex=std::min(bx+blockX,item.cols),ey=std::min(by+blockY,item.rows);
                const Rect region{r.x+bx*tileSize,r.y+by*tileSize,std::min(r.w,ex*tileSize)-bx*tileSize,std::min(r.h,ey*tileSize)-by*tileSize};
                const auto receivers=regional?shortlist(sf,sf.receiverIndices,region):sf.receiverIndices;
                const size_t receiverStart=p.size(),sourceBegin=p.size();
                for(auto id:receivers){
                    const auto& s=sf.samples[id];p.add(float(s.x),float(s.y),float(id),s.receiver==State::Measured?1.f:0.f);
                    p.add(s.position.x,s.position.y,s.position.z,s.receiverWeight);p.add(s.normal.x,s.normal.y,s.normal.z,s.receiverDepth);
                }
                std::vector<const SourceLayer*> active;
                for(const auto& l:sourceLayers)if(!regional||overlaps(l.l->bounds,region))active.push_back(&l);
                const size_t layerStart=p.reserveRecords(active.size()*7);size_t layerIndex=0;
                for(const auto* itemLayer:active){
                    const auto& l=*itemLayer->l;const auto& a=itemLayer->a;const auto& mat=scene.mat[l.key.material];
                    const size_t base=layerStart+7*layerIndex++,first=p.size();
                    const auto chosen=coverage?receivers:(regional?shortlist(sf,itemLayer->ids,region):itemLayer->ids);
                    p.set(base,float(first),float(chosen.size()),float(l.key.material),a.valid?1.f:0.f);
                    p.set(base+1,float(l.bounds.x),float(l.bounds.y),float(l.bounds.w),float(l.bounds.h));
                    p.vec(base+2,a.origin,a.u);p.vec(base+3,a.normal,a.v);p.vec(base+4,a.du);p.vec(base+5,a.dv);
                    p.vec(base+6,mat.emissionColor*std::max(0.f,mat.emissionIntensity),mat.imageAlbedo?1.f:0.f);
                    for(auto id:chosen){
                        auto it=l.measured.find(id);const bool hit=it!=l.measured.end();const auto& s=sf.samples[id];
                        if(!coverage)p.add(float(s.x),float(s.y),float(id),hit?1.f:0.f);
                        if(hit){const auto& v=it->second;p.add(v.uv.x,v.uv.y,v.depth,v.opacity);p.vec(p.add(),v.normal,float(v.primitive+1u));}
                        else{p.add();p.add();}
                    }
                }
                allocationUpper+=uint64_t(p.size()-sourceBegin)*uint64_t(ex-bx)*(ey-by);
                if(allocationUpper>std::numeric_limits<uint32_t>::max())throw std::runtime_error("Optical GPU allocator upper bound exceeds uint32");
                for(int ty=by;ty<ey;++ty)for(int tx=bx;tx<ex;++tx){
                    const size_t base=plan.native.jobBase+3*job++;
                    const size_t tileId=item.tileBase+size_t(ty)*item.cols+tx;
                    p.set(base,float(tileId),float(receiverStart),float(receivers.size()),float(layerStart));
                    p.set(base+1,float(active.size()),coverage?1.f:0.f,float(r.x+tx*tileSize),float(r.y+ty*tileSize));
                    p.set(base+2,float(std::min(tileSize,r.w-tx*tileSize)),float(std::min(tileSize,r.h-ty*tileSize)),0,0);
                }
            }
        }
        // A bounded reusable 16 MiB output; explicit failure on overflow.
        // Never truncate layers or discard known observations silently.
        plan.native.maxRecords=1u<<20;
        if(plan.native.prefixRecords>=plan.native.maxRecords)throw std::runtime_error("Optical tile header exceeds resident packet budget");
        plan.native.data=std::move(p.data);plan.ok=true;
    }catch(const std::exception& ex){plan.error=ex.what();}
    // Region duplication is bounded by the existing packet limit. On a
    // large pathological footprint, preserve the exact nonregional plan.
    if(!plan.ok&&regionSearch&&plan.regionalSurfaces){auto fallback=planGpuBuild(frame,scene,objectCount,tileSize,scalarNearest,false);fallback.regionalFallback=true;return fallback;}
    return plan;
}
inline bool logicalPacketEqual(const std::vector<float>& a,const std::vector<float>& b,std::string& why){
    auto valid=[](const std::vector<float>& p,size_t start,size_t count){return start<=p.size()/4&&count<=p.size()/4-start;};
    auto equal=[&](size_t x,size_t y,size_t n){return valid(a,x,n)&&valid(b,y,n)&&std::equal(a.begin()+4*x,a.begin()+4*(x+n),b.begin()+4*y);};
    if(a.size()<8||b.size()<8||!equal(0,0,2)){why="packet global header";return false;}
    const size_t objects=size_t(a[4]);const int tileSize=int(a[5]);
    if(tileSize<=0||objects>(a.size()/4-2)/2||objects>(b.size()/4-2)/2){why="packet object count";return false;}
    for(size_t id=1;id<=objects;++id){
        const size_t rec=2+2*id;if(!equal(rec,rec,1)||!valid(a,rec+1,1)||!valid(b,rec+1,1)){why="receiver header "+std::to_string(id);return false;}
        const float* r=&a[4*rec];const float* desc=&a[4*(rec+1)],*other=&b[4*(rec+1)];
        if(desc[1]!=other[1]||desc[2]!=other[2]||desc[3]!=other[3]){why="receiver descriptor "+std::to_string(id);return false;}
        if(r[2]<=0||r[3]<=0)continue;
        const size_t cols=size_t(desc[1]),rows=(size_t(r[3])+tileSize-1)/tileSize,base=size_t(desc[0]),otherBase=size_t(other[0]);
        const bool common=desc[3]>.5f;
        for(size_t j=0;j<cols*rows;++j){
            const size_t tile=base+j,otherTile=otherBase+j;if(!valid(a,tile,1)||!valid(b,otherTile,1)){why="tile range";return false;}
            const float* ta=&a[4*tile],*tb=&b[4*otherTile];
            if(ta[1]!=tb[1]||ta[3]!=tb[3]){why="tile counts "+std::to_string(tile);return false;}
            if(ta[3]==0)continue;
            if(!equal(size_t(ta[0]),size_t(tb[0]),3*size_t(ta[1]))){why="receiver observations "+std::to_string(tile);return false;}
            for(size_t l=0;l<size_t(ta[3]);++l){
                const size_t la=size_t(ta[2])+7*l,lb=size_t(tb[2])+7*l;
                if(!valid(a,la,7)||!valid(b,lb,7)){why="layer range";return false;}
                const float* x=&a[4*la],*y=&b[4*lb];
                if(x[1]!=y[1]||x[2]!=y[2]||x[3]!=y[3]||!equal(la+1,lb+1,6)){why="layer descriptor "+std::to_string(tile);return false;}
                if(!equal(size_t(x[0]),size_t(y[0]),size_t(x[1])*(common?2:3))){why="layer observations "+std::to_string(tile);return false;}
            }
        }
    }
    return true;
}
struct PacketKey {
    std::array<float,18> cameraLight{};
    uint64_t identity=0,revision=0,maxCells=0;
    int w=0,h=0,maxDimension=0;uint32_t budget=0,crossings=0,workers=0;Mode mode=Mode::Reflection;
    bool operator==(const PacketKey& k)const{return cameraLight==k.cameraLight&&identity==k.identity&&revision==k.revision&&maxCells==k.maxCells&&
        w==k.w&&h==k.h&&maxDimension==k.maxDimension&&budget==k.budget&&crossings==k.crossings&&workers==k.workers&&mode==k.mode;}
};
inline PacketKey packetKey(const Scene& scene,const Camera& cam,int w,int h,const Options& opt){
    float day;V direction;escargotLightingSnapshot(day,direction);
    PacketKey k;k.cameraLight={cam.eye.x,cam.eye.y,cam.eye.z,cam.f.x,cam.f.y,cam.f.z,cam.r.x,cam.r.y,cam.r.z,cam.u.x,cam.u.y,cam.u.z,cam.tanHalf,cam.aspect,day,direction.x,direction.y,direction.z};
    k.identity=scene.gpuCacheIdentity;k.revision=scene.gpuCacheRevision;k.maxCells=opt.maxOutputCells;k.w=w;k.h=h;k.maxDimension=opt.maxDimension;k.budget=opt.rayBudget;k.crossings=opt.maxCrossings;k.workers=opt.rayWorkers;k.mode=opt.mode;return k;
}
class PacketCache {
    PacketKey key_{};Packet packet_{};bool valid_=false;
public:
    const Packet& get(Prepared& session,const Scene& scene,size_t objectCount,const Camera& cam,int w,int h,const Options& opt,bool& reused){
        auto key=packetKey(scene,cam,w,h,opt);reused=valid_&&key==key_;
        if(reused)return packet_;
        valid_=false;auto frame=session.frame(cam,w,h,opt,false);
        if(!frame.ok){packet_={};packet_.error=frame.error;return packet_;}
        packet_=packGpu(frame,scene,objectCount);if(packet_.ok){key_=key;valid_=true;}return packet_;
    }
};
}
