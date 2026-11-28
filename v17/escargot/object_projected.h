#pragma once
#include "sampling.h"
#include <array>
#include <atomic>
#include <memory>
#include <iterator>
#include <set>
#include <chrono>
namespace m3d::escargot::projected {
constexpr uint32_t maxReceivers=4,rayBudget=768,triangleBudget=120000,maxSources=160;
struct Metrics {uint32_t catalogue=0,patches=0,visible=0,water=0,rays=0,receivers=0,curved=0,deferred=0,sources=0,triangles=0,omitted=0,mini=0,lightingSamples=0;bool reused=false;double totalMs=0,receiverMs=0,discoverMs=0,rankMs=0,cullMs=0,sortMs=0,emitMs=0;uint64_t testedObjects=0,testedTriangles=0;};
struct Plane {V p,n;bool valid=false;int side=0;};
struct ReceiverPatch {Candidate candidate;Plane plane;double area=0;size_t index=0;};
inline V mirror(V p,Plane plane){return p-plane.n*(2.f*dot(p-plane.p,plane.n));}
inline Plane planeFor(const Scene& s,const v16::ObjectInfo& o){
    Plane p;float best=0;
    for(int j=0;j<o.triangleCount;++j){const auto& t=s.obj[size_t(o.firstTriangle)+j];V n=cross(t.b-t.a,t.c-t.a);float a=dot(n,n);
        if(a>best){best=a;p={t.a,norm(n),true};}}
    if(!p.valid||best<1e-12f)return {};
    for(int j=0;j<o.triangleCount;++j){const auto& t=s.obj[size_t(o.firstTriangle)+j];
        for(V q:{t.a,t.b,t.c})if(std::abs(dot(q-p.p,p.n))>.015f)return {};}
    return p;
}
inline Rect intersection(Rect a,Rect b){int x=std::max(a.x,b.x),y=std::max(a.y,b.y);
    return {x,y,std::max(0,std::min(a.x+a.w,b.x+b.w)-x),std::max(0,std::min(a.y+a.h,b.y+b.h)-y)};}
struct Box {V lo,hi;};
inline Box mirroredBox(V lo,V hi,Plane p){
    V a{INF,INF,INF},b{-INF,-INF,-INF};
    for(int i=0;i<8;++i){V q=mirror({(i&1)?hi.x:lo.x,(i&2)?hi.y:lo.y,(i&4)?hi.z:lo.z},p);
        a={std::min(a.x,q.x),std::min(a.y,q.y),std::min(a.z,q.z)};
        b={std::max(b.x,q.x),std::max(b.y,q.y),std::max(b.z,q.z)};}
    return {a,b};
}
inline Rect mirroredBounds(V lo,V hi,Plane p,const Camera& c,int w,int h){
    const auto b=mirroredBox(lo,hi,p);return projectBounds(b.lo,b.hi,c,w,h);
}
// Same projection for fully in-front boxes; near-plane intersections retain the exact old implementation.
inline Rect fastBounds(Box b,const Camera& c,int w,int h){
    if(!finite(b.lo)||!finite(b.hi)||b.lo.x>b.hi.x||b.lo.y>b.hi.y||b.lo.z>b.hi.z)return {};
    float minx=INF,miny=INF,maxx=-INF,maxy=-INF;
    for(int i=0;i<8;++i){const V q=V{(i&1)?b.hi.x:b.lo.x,(i&2)?b.hi.y:b.lo.y,(i&4)?b.hi.z:b.lo.z}-c.eye;
        const float z=dot(q,c.f);if(z<.08f)return projectBounds(b.lo,b.hi,c,w,h);
        const float x=(.5f+.5f*dot(q,c.r)/(z*c.tanHalf))*w,y=(.5f-.5f*dot(q,c.u)*c.aspect/(z*c.tanHalf))*h;
        minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);
    }
    if(!std::isfinite(minx)||!std::isfinite(miny)||maxx<=0||maxy<=0||minx>=w||miny>=h)return {};
    const int x0=int(std::floor(clampf(minx,0.f,float(w)))),y0=int(std::floor(clampf(miny,0.f,float(h))));
    const int x1=int(std::ceil(clampf(maxx,0.f,float(w)))),y1=int(std::ceil(clampf(maxy,0.f,float(h))));
    return {x0,y0,x1-x0,y1-y0};
}
// Conservative hierarchy: reject only if all points of a union box are outside a padded footprint plane.
// This also works for near-plane crossings; the final per-triangle test remains authoritative.
inline bool outsideFootprint(Box b,const Camera& c,Rect r,int w,int h){
    if(!finite(b.lo)||!finite(b.hi))return false;
    // Legacy projectBounds returns the full viewport for a camera inside any child box.
    // A containing union must never be rejected, even if it lies before the near plane.
    if(c.eye.x>=b.lo.x&&c.eye.x<=b.hi.x&&c.eye.y>=b.lo.y&&c.eye.y<=b.hi.y&&c.eye.z>=b.lo.z&&c.eye.z<=b.hi.z)return false;
    const V lo=b.lo-c.eye,hi=b.hi-c.eye;
    const float scale=1.f+std::max({std::abs(lo.x),std::abs(lo.y),std::abs(lo.z),std::abs(hi.x),std::abs(hi.y),std::abs(hi.z)});
    auto outside=[&](V n,float threshold){
        const V q{n.x>=0?hi.x:lo.x,n.y>=0?hi.y:lo.y,n.z>=0?hi.z:lo.z};
        const float guard=1e-4f*scale*(1.f+std::abs(n.x)+std::abs(n.y)+std::abs(n.z));
        return dot(q,n)<threshold-guard;
    };
    const float left=(2.f*(r.x-2)/w-1.f)*c.tanHalf,right=(2.f*(r.x+r.w+2)/w-1.f)*c.tanHalf;
    const float top=(1.f-2.f*(r.y-2)/h)*c.tanHalf/c.aspect,bottom=(1.f-2.f*(r.y+r.h+2)/h)*c.tanHalf/c.aspect;
    return outside(c.f,.08f)||outside(c.r-c.f*left,0)||outside(c.f*right-c.r,0)||outside(c.f*top-c.u,0)||outside(c.u-c.f*bottom,0);
}
inline void extendBox(Box& b,Box a){
    b.lo={std::min(b.lo.x,a.lo.x),std::min(b.lo.y,a.lo.y),std::min(b.lo.z,a.lo.z)};
    b.hi={std::max(b.hi.x,a.hi.x),std::max(b.hi.y,a.hi.y),std::max(b.hi.z,a.hi.z)};
}
constexpr size_t blockSize=32,cacheBytesPerReceiver=16u*1024u*1024u;
struct CachedBounds {Box box;float low,high;};
struct SourceBounds {std::vector<CachedBounds> triangles;std::vector<Box> blocks;};
struct ReceiverBounds {size_t patch=std::numeric_limits<size_t>::max(),bytes=0;std::map<uint32_t,SourceBounds> sources;};
struct Source{uint32_t id=0;V lo{},hi{};};
struct ResidentSourceBlock{EscargotResidentBlock draw;Box bounds;bool alpha=false;};

struct Seen{uint32_t last=0,hits=0;};
struct TriangleRef{uint32_t id;float depth;};
class Session {
    const v16::Runtime& rt_;std::vector<uint32_t> owners_;std::unique_ptr<Traversal> trace_;
    std::vector<Candidate> catalogue_;std::vector<ReceiverPatch> patches_;
    std::vector<Source> sources_;std::map<uint32_t,std::map<uint32_t,Seen>> seen_;
    bool gpuResident_=false;
    std::vector<std::vector<ResidentSourceBlock>> sourceBlocks_;
    std::array<ReceiverBounds,maxReceivers> boundsCache_;
    struct EmitCache {size_t patch=std::numeric_limits<size_t>::max();uint32_t epoch=0;float sign=0;std::vector<uint32_t> ids;std::vector<float> vertices;};
    std::array<EmitCache,maxReceivers> emitCache_;
    // Receiver slots are bounded; a different patch invalidates the slot.
    const SourceBounds* cachedBounds(size_t slot,size_t patch,uint32_t source,Plane p){
        auto& cache=boundsCache_[slot];if(cache.patch!=patch){cache=ReceiverBounds{};cache.patch=patch;}
        auto found=cache.sources.find(source);if(found!=cache.sources.end())return &found->second;
        const auto& o=rt_.objects[int(source)-1];const size_t n=size_t(o.triangleCount),blocks=(n+blockSize-1)/blockSize;
        if(n>cacheBytesPerReceiver/sizeof(CachedBounds))return nullptr;
        const size_t bytes=n*sizeof(CachedBounds)+blocks*sizeof(Box);
        if(bytes>cacheBytesPerReceiver-cache.bytes)return nullptr;
        SourceBounds data;data.triangles.reserve(n);data.blocks.reserve(blocks);
        const auto& scene=rt_.renderScene;
        for(size_t j=0;j<n;++j){const auto& tri=scene.obj[size_t(o.firstTriangle)+j];const Box b=mirroredBox(tri.mn,tri.mx,p);
            const float a=dot(tri.a-p.p,p.n),d=dot(tri.b-p.p,p.n),e=dot(tri.c-p.p,p.n);
            data.triangles.push_back({b,std::min({a,d,e}),std::max({a,d,e})});
            if(j%blockSize==0)data.blocks.push_back(b);else extendBox(data.blocks.back(),b);
        }
        cache.bytes+=bytes;return &cache.sources.emplace(source,std::move(data)).first->second;
    }
    std::vector<std::array<float,3>> light_;std::vector<uint32_t> lightStamp_;
    uint32_t epoch_=1,frame_=0;float day_=-1;V sun_{};
    std::array<float,16> key_{};bool cacheValid_=false;EscargotPacketBuild cached_;Metrics cachedMetrics_;
    float shade(V p,V n,const ShadowMap& sh)const{
        V q=p-sh.base;float shadow=1;
        if(sh.size>0&&sh.span>0&&sh.d.size()==size_t(sh.size)*sh.size){
            float scale=float(sh.size)/(2*sh.span);
            int x=int(std::floor(dot(q,sh.right)*scale+sh.size*.5f)),y=int(std::floor(dot(q,sh.up)*scale+sh.size*.5f));
            if(x>=0&&y>=0&&x<sh.size&&y<sh.size)
                shadow=sh.dist-dot(q,sun_)<=sh.d[size_t(y)*sh.size+x]+.045f?1.f:.22f;
        }
        return .16f+.34f*day_+.50f*day_*sat(dot(norm(n),sun_))*shadow;
    }
    uint32_t probe(const Candidate& r,Plane plane,const Camera& cam,int x,int y,int w,int h,uint32_t limit,Metrics& m){
        const auto& s=rt_.renderScene;Ray ray=screenRay(cam,x,y,w,h);
        HitSample hit;float low=.08f/std::max(1e-6f,dot(ray.d,cam.f));uint32_t previous=0xffffffffu;
        bool reached=false;
        for(int k=0;k<4&&m.rays<limit;++k){++m.rays;if(!trace_->closest(ray,low,hit,previous))return 0;
            if(hit.object==r.object&&std::abs(dot(hit.p-plane.p,plane.n))<.003f){reached=true;break;}
            if(transmission(s.mat[s.obj[hit.primitive].mat])<.01f)return 0;
            low=hit.depth;previous=hit.primitive;}
        if(!reached)return 0;
        ray={hit.p,reflect(ray.d,plane.n)};
        low=std::max(1e-5f,8.f*std::numeric_limits<float>::epsilon()*std::max(1.f,length(hit.p)));previous=0xffffffffu;
        uint32_t first=0;
        for(int k=0;k<8&&m.rays<limit;++k){++m.rays;if(!trace_->closest(ray,low,hit,previous))break;
            if(hit.object!=r.object){auto& a=seen_[r.object][hit.object];a.last=frame_;a.hits=std::min(a.hits+1,1024u);if(!first)first=hit.object;}
            if(transmission(s.mat[s.obj[hit.primitive].mat])<.001f)break;
            low=hit.depth;previous=hit.primitive;}
        return first;
    }
    void discover(const Candidate& r,Plane plane,const Camera& cam,int w,int h,uint32_t limit,Metrics& m){
        // Stratified discovery; unused budget is spent on mixed or zero cells.
        uint32_t seedLimit=m.rays+(limit-m.rays)*2/3;
        uint32_t v[5][5]{};
        const Rect b=r.bounds;
        auto x=[&](float i){return b.x+std::clamp(int(i*b.w/5.f),0,b.w-1);};
        auto y=[&](float i){return b.y+std::clamp(int(i*b.h/5.f),0,b.h-1);};
        float phase=gpuResident_?.5f:float(frame_%5)*.12f+.20f;
        for(int j=0;j<5&&m.rays<seedLimit;++j)for(int i=0;i<5&&m.rays<seedLimit;++i)
            v[j][i]=probe(r,plane,cam,x(i+phase),y(j+phase),w,h,seedLimit,m);
        for(int pass=0;pass<2;++pass)for(int j=0;j<4&&m.rays<limit;++j)for(int i=0;i<4&&m.rays<limit;++i){
            bool mixed=v[j][i]!=v[j][i+1]||v[j][i]!=v[j+1][i]||v[j][i]!=v[j+1][i+1];
            bool zero=!v[j][i]&&!v[j][i+1]&&!v[j+1][i]&&!v[j+1][i+1];
            if((pass==0&&mixed)||(pass==1&&zero)){++m.mini;probe(r,plane,cam,x(i+phase+.5f),y(j+phase+.5f),w,h,limit,m);}
        }
        if(!gpuResident_){auto& a=seen_[r.object];for(auto it=a.begin();it!=a.end();)if(frame_-it->second.last>64)it=a.erase(it);else ++it;}
    }
    void appendProjectedTriangle(EscargotPacketBuild& plan,const TriangleRef& ref,Plane p,const Camera& c,const ShadowMap& sh,Metrics& m){
        const auto& t=rt_.renderScene.obj[ref.id];const auto& mat=rt_.renderScene.mat[t.mat];
        V pos[]={t.a,t.b,t.c},uv[]={t.uv0,t.uv1,t.uv2},n[]={t.n0,t.n1,t.n2};
        if(lightStamp_[ref.id]!=epoch_){
            for(int k=0;k<3;++k)light_[ref.id][k]=shade(pos[k],n[k],sh);
            lightStamp_[ref.id]=epoch_;m.lightingSamples+=3;
        }
        const float sign=dot(c.eye-p.p,p.n)>=0?1.f:-1.f;
        for(int k=0;k<3;++k){
            // Camera-independent mirrored world position; VS performs the exact clip transform.
            const V q=mirror(pos[k],p);
            const float data[]={q.x,q.y,q.z,1.f,
                uv[k].x,uv[k].y,light_[ref.id][k],float(t.mat),opacity(mat),sign*dot(pos[k]-p.p,p.n)-.0001f};
            plan.projectedVertices.insert(plan.projectedVertices.end(),std::begin(data),std::end(data));
        }
    }
public:
    Session(const v16::Runtime& rt,const v16::SceneDatabase& db,bool gpuResident=false):rt_(rt),gpuResident_(gpuResident){
        if(!objectMap(rt,owners_))return;
        trace_=std::make_unique<Traversal>(rt.renderScene,rt.lumaTraceBvh4,owners_);
        if(!trace_->valid()){trace_.reset();return;}
        catalogue_=staticCandidates(rt,db,Mode::Reflection);
        // The production optical index already recognizes thin slabs and the top water face.
        // Split merged planes by exact triangle ownership to preserve author permissions.
        std::map<uint32_t,Candidate> allowed;for(const auto& c:catalogue_)allowed.emplace(c.object,c);
        std::set<uint32_t> indexed;
        for(const auto& p:rt.renderScene.opticalPlanes){
            std::map<uint32_t,zreflect::Bounds> bounds;
            std::map<uint32_t,double> area;
            for(uint32_t id:p.triangles){
                if(id>=owners_.size()||!allowed.count(owners_[id]))continue;
                uint32_t owner=owners_[id];const auto& t=rt.renderScene.obj[id];
                auto& b=bounds[owner];b.add(t.a);b.add(t.b);b.add(t.c);
                area[owner]+=.5*length(cross(t.b-t.a,t.c-t.a));
            }
            for(const auto& kv:bounds){
                auto c=allowed.at(kv.first);c.lo=kv.second.lo;c.hi=kv.second.hi;
                if(kv.second.uncertain||area[kv.first]<=0||!finite(p.normal))continue;
                patches_.push_back({c,{p.normal*float(p.offset),p.normal,true,p.side},area[kv.first]});
                indexed.insert(kv.first);
            }
        }
        // Non-indexed authored flat receivers are still supported. Do not
        // accept arbitrary curved objects or bypass the existing permission.
        for(const auto& c:catalogue_)if(!indexed.count(c.object)){
            const auto& o=rt.objects[int(c.object)-1];
            const auto& mat=rt.renderScene.mat[c.material];
            auto slabs=zreflect::extractSlab(rt.renderScene,o.firstTriangle,o.triangleCount,waterMaterial(mat),true);
            for(const auto& p:slabs){
                auto a=c;a.lo=p.bounds.lo;a.hi=p.bounds.hi;
                patches_.push_back({a,{p.normal*float(p.offset),p.normal,true,p.side},p.area});
            }
            if(slabs.empty()){
                auto p=planeFor(rt.renderScene,o);
                if(p.valid)patches_.push_back({c,p,1.});
            }
        }
        for(int i=0;i<rt.objects.size();++i){const auto& o=rt.objects[i];if(o.triangleCount<=0)continue;V lo{INF,INF,INF},hi{-INF,-INF,-INF};
            for(int j=0;j<o.triangleCount;++j){const auto& t=rt.renderScene.obj[size_t(o.firstTriangle)+j];
                lo={std::min(lo.x,t.mn.x),std::min(lo.y,t.mn.y),std::min(lo.z,t.mn.z)};
                hi={std::max(hi.x,t.mx.x),std::max(hi.y,t.mx.y),std::max(hi.z,t.mx.z)};}
            sources_.push_back({uint32_t(i)+1,lo,hi});}
        for(size_t i=0;i<patches_.size();++i)patches_[i].index=i;
        if(gpuResident_){
            sourceBlocks_.resize(size_t(rt.objects.size())+1);
            const auto& s=rt.renderScene;
            for(const auto& source:sources_){
                const auto& obj=rt.objects[int(source.id)-1];auto& out=sourceBlocks_[source.id];
                for(int j=0;j<obj.triangleCount;++j){
                    const uint32_t id=uint32_t(obj.firstTriangle+j);const auto& t=s.obj[id];const auto& mat=s.mat[t.mat];
                    const float a=opacity(mat);if(waterMaterial(mat)||a<.001f)continue;
                    const bool alpha=a<.999f;const Box box{t.mn,t.mx};
                    // Exact alpha ordering remains available: one triangle per transparent block.
                    // Opaque runs can contain up to 32 contiguous original triangles.
                    if(!alpha&&!out.empty()&&!out.back().alpha&&out.back().draw.count<blockSize&&
                       out.back().draw.first+out.back().draw.count==id){
                        ++out.back().draw.count;extendBox(out.back().bounds,box);
                    }else out.push_back({{id,1},box,alpha});
                }
            }
        }else {light_.resize(rt.renderScene.obj.size());lightStamp_.resize(light_.size(),0);}
    }
    bool valid()const{return bool(trace_);}
    const EscargotPacketBuild& build(const Camera& c,const ShadowMap& sh,int w,int h,Metrics& m){
        using Clock=std::chrono::steady_clock;const auto start=Clock::now();auto stage=start;
        auto ms=[](Clock::time_point a){return std::chrono::duration<double,std::milli>(Clock::now()-a).count();};
        m=Metrics{};++frame_;float day;V sun;escargotLightingSnapshot(day,sun);
        std::array<float,16> key={c.eye.x,c.eye.y,c.eye.z,c.f.x,c.f.y,c.f.z,c.r.x,c.r.y,c.r.z,c.u.x,c.u.y,c.u.z,c.tanHalf,c.aspect,float(w),float(h)};
        const bool lightChanged=day_!=day||length(sun-sun_)>1e-5f;
        if(lightChanged){day_=day;sun_=sun;++epoch_;}
        if(cacheValid_&&key==key_&&(gpuResident_||(!lightChanged&&frame_%8!=0))){m=cachedMetrics_;m.rays=m.mini=m.lightingSamples=0;m.receiverMs=m.discoverMs=m.rankMs=m.cullMs=m.sortMs=m.emitMs=0;m.testedObjects=m.testedTriangles=0;m.totalMs=ms(start);m.reused=true;return cached_;}
        cached_.projectedVertices.clear();cached_.projectedRanges.clear();cached_.resident.blocks.clear();auto& plan=cached_;plan.rasterProjected=true;plan.resident.enabled=gpuResident_;
        if(!gpuResident_)plan.projectedVertices.reserve(size_t(triangleBudget)*30);
        else seen_.clear(); // deterministic discovery: camera/scene, never elapsed frames or history
        static std::atomic<uint64_t> nextKey{1};plan.projectedKey=nextKey.fetch_add(1,std::memory_order_relaxed);
        plan.data.assign((2+2*size_t(rt_.objects.size()))*4,0);plan.data[0]=float(rt_.objects.size());
        m.catalogue=uint32_t(catalogue_.size());m.patches=uint32_t(patches_.size());
        std::vector<ReceiverPatch> candidates;
        std::set<uint32_t> hasPatch;for(auto r:patches_){
            hasPatch.insert(r.candidate.object);
            if(r.plane.side&&r.plane.side*dot(c.eye-r.plane.p,r.plane.n)<-.001f)continue;
            r.candidate.bounds=projectBounds(r.candidate.lo,r.candidate.hi,c,w,h);
            if(!r.candidate.bounds.valid()||uint64_t(r.candidate.bounds.w)*r.candidate.bounds.h<16)continue;
            r.candidate.priority*=std::sqrt(double(r.candidate.bounds.w)*r.candidate.bounds.h);
            candidates.push_back(r);
        }
        m.visible=uint32_t(candidates.size());m.curved=m.catalogue-uint32_t(hasPatch.size());
        std::stable_sort(candidates.begin(),candidates.end(),[&](const ReceiverPatch& a,const ReceiverPatch& b){
            bool aw=waterMaterial(rt_.renderScene.mat[a.candidate.material]),bw=waterMaterial(rt_.renderScene.mat[b.candidate.material]);
            if(aw!=bw)return aw;
            if(a.candidate.priority!=b.candidate.priority)return a.candidate.priority>b.candidate.priority;
            return a.area>b.area;});
        std::vector<ReceiverPatch> admitted;std::set<uint32_t> used;
        for(const auto& a:candidates){
            if(used.count(a.candidate.object))continue;
            if(admitted.size()>=maxReceivers){++m.deferred;continue;}
            used.insert(a.candidate.object);admitted.push_back(a);
        }
        m.receiverMs=ms(stage);const auto& s=rt_.renderScene;
        for(size_t k=0;k<admitted.size();++k){
            const auto& a=admitted[k].candidate;auto p=admitted[k].plane;uint32_t quota=m.rays+(rayBudget-m.rays)/uint32_t(admitted.size()-k);
            stage=Clock::now();discover(a,p,c,w,h,quota,m);m.discoverMs+=ms(stage);stage=Clock::now();
            struct Ranked{uint32_t id;double score;bool seen;};
            std::vector<Ranked> ranked;auto& history=seen_[a.object];
            for(const auto& b:sources_){if(b.id==a.object)continue;++m.testedObjects;
                Rect r=intersection(a.bounds,mirroredBounds(b.lo,b.hi,p,c,w,h));if(!r.valid())continue;
                const auto it=history.find(b.id);bool observed=it!=history.end();
                // Geometry admits unobserved objects, so sparse misses are not holes.
                const V center=(b.lo+b.hi)*.5f;float depth=std::max(.5f,length(center-c.eye));
                ranked.push_back({b.id,double(r.w)*r.h/(1.+.04*depth),observed});}
            std::sort(ranked.begin(),ranked.end(),[](const Ranked& x,const Ranked& y){
                if(x.seen!=y.seen)return x.seen>y.seen;return x.score==y.score?x.id<y.id:x.score>y.score;});
            m.rankMs+=ms(stage);stage=Clock::now();std::vector<TriangleRef> opaque,transparent;
            const uint32_t limit=(triangleBudget-m.triangles)/uint32_t(admitted.size()-k);
            uint32_t count=0,usedSources=0;
            const float sign=dot(c.eye-p.p,p.n)>=0?1.f:-1.f;
            if(gpuResident_){
                struct AlphaBlock{EscargotResidentBlock block;float depth;};
                std::vector<EscargotResidentBlock> solid;
                std::vector<AlphaBlock> alpha;
                uint32_t opaqueCount=0,alphaCount=0;
                for(const auto& b:ranked){
                    if(usedSources>=maxSources||count>=limit){++m.omitted;continue;}
                    const auto oldSolid=solid.size(),oldAlpha=alpha.size();const uint32_t oldO=opaqueCount,oldA=alphaCount,oldCount=count;
                    bool overflow=false;
                    for(const auto& block:sourceBlocks_[b.id]){
                        // Coarse conservative box only; all individual plane/frustum clipping is GPU-side.
                        const auto reflected=mirroredBox(block.bounds.lo,block.bounds.hi,p);
                        if(outsideFootprint(reflected,c,a.bounds,w,h))continue;
                        const V n=p.n*sign;
                        const V support{n.x>=0?block.bounds.hi.x:block.bounds.lo.x,n.y>=0?block.bounds.hi.y:block.bounds.lo.y,n.z>=0?block.bounds.hi.z:block.bounds.lo.z};
                        if(dot(support-p.p,n)<-.001f)continue;
                        if(block.draw.count>limit-count){overflow=true;break;}
                        if(block.alpha){
                            const auto& t=s.obj[block.draw.first];
                            alpha.push_back({block.draw,dot(mirror((t.a+t.b+t.c)/3.f,p)-c.eye,c.f)});alphaCount+=block.draw.count;
                        }else {solid.push_back(block.draw);opaqueCount+=block.draw.count;}
                        count+=block.draw.count;
                    }
                    if(overflow){solid.resize(oldSolid);alpha.resize(oldAlpha);opaqueCount=oldO;alphaCount=oldA;count=oldCount;++m.omitted;continue;}
                    if(count>oldCount){++usedSources;++m.sources;}
                }
                m.cullMs+=ms(stage);stage=Clock::now();
                std::sort(alpha.begin(),alpha.end(),[](const AlphaBlock& x,const AlphaBlock& y){return x.depth==y.depth?x.block.first<y.block.first:x.depth>y.depth;});
                m.sortMs+=ms(stage);stage=Clock::now();
                EscargotProjectedRange r;r.receiver=a.object;r.first=m.triangles*3;r.opaque=opaqueCount*3;r.transparent=alphaCount*3;
                r.blockFirst=uint32_t(plan.resident.blocks.size());r.blockOpaque=uint32_t(solid.size());r.blockTransparent=uint32_t(alpha.size());
                r.x=std::max(0,a.bounds.x-1);r.y=std::max(0,a.bounds.y-1);
                r.w=std::min(w,a.bounds.x+a.bounds.w+1)-r.x;r.h=std::min(h,a.bounds.y+a.bounds.h+1)-r.y;
                plan.resident.blocks.insert(plan.resident.blocks.end(),solid.begin(),solid.end());
                for(const auto& b:alpha)plan.resident.blocks.push_back(b.block);
                const size_t at=4*(size_t(a.object)+1);
                plan.data[at]=float(plan.projectedRanges.size()+1);plan.data[at+1]=p.n.x;plan.data[at+2]=p.n.y;plan.data[at+3]=p.n.z;
                plan.data[4*(size_t(rt_.objects.size())+1+size_t(a.object))]=dot(p.p,p.n);
                plan.projectedRanges.push_back(r);m.triangles+=count;++m.receivers;
                if(waterMaterial(s.mat[a.material]))++m.water;m.emitMs+=ms(stage);
                continue;
            }
            for(const auto& b:ranked){const auto& o=rt_.objects[int(b.id)-1];
                if(usedSources>=maxSources||count>=limit){++m.omitted;continue;}
                uint32_t before=count;const auto opaqueBefore=opaque.size(),transparentBefore=transparent.size();
                bool overflow=false;
                const SourceBounds* resident=cachedBounds(k,admitted[k].index,b.id,p);
                for(int j=0;j<o.triangleCount;){
                    const size_t local=size_t(j);
                    if(resident&&local%blockSize==0&&outsideFootprint(resident->blocks[local/blockSize],c,a.bounds,w,h)){
                        j+=int(std::min(blockSize,size_t(o.triangleCount-j)));continue;
                    }
                    ++m.testedTriangles;const uint32_t id=uint32_t(o.firstTriangle+j++);const auto& t=s.obj[id];const auto& mat=s.mat[t.mat];
                    if(waterMaterial(mat)||opacity(mat)<.001f)continue;
                    if(resident){const auto& q=resident->triangles[local];if((sign>0?q.high:-q.low)<=.0001f)continue;}
                    else if(std::max({sign*dot(t.a-p.p,p.n),sign*dot(t.b-p.p,p.n),sign*dot(t.c-p.p,p.n)})<=.0001f)continue;
                    // Preserve the exact per-triangle footprint and source ordering.
                    const Rect bounds=intersection(a.bounds,resident?fastBounds(resident->triangles[local].box,c,w,h):mirroredBounds(t.mn,t.mx,p,c,w,h));if(!bounds.valid())continue;
                    if(count==limit){overflow=true;break;}
                    if(opacity(mat)>=.999f)opaque.push_back({id,0.f});
                    else transparent.push_back({id,dot(mirror((t.a+t.b+t.c)/3.f,p)-c.eye,c.f)});
                    ++count;
                }
                if(overflow){opaque.resize(opaqueBefore);transparent.resize(transparentBefore);count=before;++m.omitted;continue;}
                if(count>before){++usedSources;++m.sources;}
            }
            m.cullMs+=ms(stage);stage=Clock::now();std::sort(transparent.begin(),transparent.end(),[](const TriangleRef& x,const TriangleRef& y){return x.depth==y.depth?x.id<y.id:x.depth>y.depth;});
            m.sortMs+=ms(stage);stage=Clock::now();EscargotProjectedRange range;range.receiver=a.object;range.first=uint32_t(plan.projectedVertices.size()/10);
            range.opaque=uint32_t(opaque.size()*3);range.transparent=uint32_t(transparent.size()*3);
            range.x=std::max(0,a.bounds.x-1);range.y=std::max(0,a.bounds.y-1);
            range.w=std::min(w,a.bounds.x+a.bounds.w+1)-range.x;range.h=std::min(h,a.bounds.y+a.bounds.h+1)-range.y;
            auto& ec=emitCache_[k];
            bool same=ec.patch==admitted[k].index&&ec.epoch==epoch_&&ec.sign==sign&&ec.ids.size()==opaque.size()+transparent.size();
            if(same){size_t j=0;for(const auto& t:opaque)if(ec.ids[j++]!=t.id){same=false;break;}
                if(same)for(const auto& t:transparent)if(ec.ids[j++]!=t.id){same=false;break;}}
            if(same)plan.projectedVertices.insert(plan.projectedVertices.end(),ec.vertices.begin(),ec.vertices.end());
            else {
                const size_t first=plan.projectedVertices.size();
                for(const auto& t:opaque)appendProjectedTriangle(plan,t,p,c,sh,m);
                for(const auto& t:transparent)appendProjectedTriangle(plan,t,p,c,sh,m);
                ec.patch=admitted[k].index;ec.epoch=epoch_;ec.sign=sign;ec.ids.clear();ec.ids.reserve(count);
                for(const auto& t:opaque)ec.ids.push_back(t.id);for(const auto& t:transparent)ec.ids.push_back(t.id);
                ec.vertices.assign(plan.projectedVertices.begin()+first,plan.projectedVertices.end());
            }
            const size_t at=4*(size_t(a.object)+1);
            plan.data[at]=float(plan.projectedRanges.size()+1);plan.data[at+1]=p.n.x;plan.data[at+2]=p.n.y;plan.data[at+3]=p.n.z;
            plan.data[4*(size_t(rt_.objects.size())+1+size_t(a.object))]=dot(p.p,p.n);
            plan.projectedRanges.push_back(range);m.triangles+=count;++m.receivers;
            if(waterMaterial(s.mat[a.material]))++m.water;m.emitMs+=ms(stage);
        }
        m.totalMs=ms(start);key_=key;cacheValid_=true;cachedMetrics_=m;return cached_;
    }
};
}
