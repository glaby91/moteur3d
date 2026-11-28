#pragma once
#include "sampling.h"
#include <array>
#include <memory>
#include <set>
namespace m3d::escargot::projected {
constexpr uint32_t maxReceivers=4,rayBudget=768,triangleBudget=120000,maxSources=160;
struct Metrics {uint32_t rays=0,receivers=0,curved=0,deferred=0,sources=0,triangles=0,omitted=0,mini=0,lightingSamples=0;bool reused=false;};
struct Plane {V p,n;bool valid=false;};
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
inline Rect mirroredBounds(V lo,V hi,Plane p,const Camera& c,int w,int h){
    V a{INF,INF,INF},b{-INF,-INF,-INF};
    for(int i=0;i<8;++i){V q=mirror({(i&1)?hi.x:lo.x,(i&2)?hi.y:lo.y,(i&4)?hi.z:lo.z},p);
        a={std::min(a.x,q.x),std::min(a.y,q.y),std::min(a.z,q.z)};
        b={std::max(b.x,q.x),std::max(b.y,q.y),std::max(b.z,q.z)};}
    return projectBounds(a,b,c,w,h);
}
struct Source{uint32_t id=0;V lo{},hi{};};
struct Seen{uint32_t last=0,hits=0;};
struct TriangleRef{uint32_t id;float depth;};
class Session {
    const v16::Runtime& rt_;std::vector<uint32_t> owners_;std::unique_ptr<Traversal> trace_;
    std::vector<Candidate> catalogue_;std::vector<Plane> planes_;
    std::vector<Source> sources_;std::map<uint32_t,std::map<uint32_t,Seen>> seen_;
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
    uint32_t probe(const Candidate& r,const Camera& cam,int x,int y,int w,int h,uint32_t limit,Metrics& m){
        const auto& s=rt_.renderScene;Ray ray=screenRay(cam,x,y,w,h);
        HitSample hit;float low=.08f/std::max(1e-6f,dot(ray.d,cam.f));uint32_t previous=0xffffffffu;
        bool reached=false;
        for(int k=0;k<4&&m.rays<limit;++k){++m.rays;if(!trace_->closest(ray,low,hit,previous))return 0;
            if(hit.object==r.object){reached=true;break;}
            if(transmission(s.mat[s.obj[hit.primitive].mat])<.01f)return 0;
            low=hit.depth;previous=hit.primitive;}
        if(!reached)return 0;
        const auto plane=planes_[r.object];ray={hit.p,reflect(ray.d,plane.n)};
        low=std::max(1e-5f,8.f*std::numeric_limits<float>::epsilon()*std::max(1.f,length(hit.p)));previous=0xffffffffu;
        uint32_t first=0;
        for(int k=0;k<8&&m.rays<limit;++k){++m.rays;if(!trace_->closest(ray,low,hit,previous))break;
            if(hit.object!=r.object){auto& a=seen_[r.object][hit.object];a.last=frame_;a.hits=std::min(a.hits+1,1024u);if(!first)first=hit.object;}
            if(transmission(s.mat[s.obj[hit.primitive].mat])<.001f)break;
            low=hit.depth;previous=hit.primitive;}
        return first;
    }
    void discover(const Candidate& r,const Camera& cam,int w,int h,uint32_t limit,Metrics& m){
        // Stratified discovery; unused budget is spent on mixed or zero cells.
        uint32_t seedLimit=m.rays+(limit-m.rays)*2/3;
        uint32_t v[5][5]{};
        const Rect b=r.bounds;
        auto x=[&](float i){return b.x+std::clamp(int(i*b.w/5.f),0,b.w-1);};
        auto y=[&](float i){return b.y+std::clamp(int(i*b.h/5.f),0,b.h-1);};
        float phase=float(frame_%5)*.12f+.20f;
        for(int j=0;j<5&&m.rays<seedLimit;++j)for(int i=0;i<5&&m.rays<seedLimit;++i)
            v[j][i]=probe(r,cam,x(i+phase),y(j+phase),w,h,seedLimit,m);
        for(int pass=0;pass<2;++pass)for(int j=0;j<4&&m.rays<limit;++j)for(int i=0;i<4&&m.rays<limit;++i){
            bool mixed=v[j][i]!=v[j][i+1]||v[j][i]!=v[j+1][i]||v[j][i]!=v[j+1][i+1];
            bool zero=!v[j][i]&&!v[j][i+1]&&!v[j+1][i]&&!v[j+1][i+1];
            if((pass==0&&mixed)||(pass==1&&zero)){++m.mini;probe(r,cam,x(i+phase+.5f),y(j+phase+.5f),w,h,limit,m);}
        }
        auto& a=seen_[r.object];for(auto it=a.begin();it!=a.end();)if(frame_-it->second.last>64)it=a.erase(it);else ++it;
    }
    void emit(EscargotPacketBuild& plan,const TriangleRef& ref,Plane p,const Camera& c,const ShadowMap& sh,Metrics& m){
        const auto& t=rt_.renderScene.obj[ref.id];const auto& mat=rt_.renderScene.mat[t.mat];
        V pos[]={t.a,t.b,t.c},uv[]={t.uv0,t.uv1,t.uv2},n[]={t.n0,t.n1,t.n2};
        if(lightStamp_[ref.id]!=epoch_){
            for(int k=0;k<3;++k)light_[ref.id][k]=shade(pos[k],n[k],sh);
            lightStamp_[ref.id]=epoch_;m.lightingSamples+=3;
        }
        const float sign=dot(c.eye-p.p,p.n)>=0?1.f:-1.f;
        for(int k=0;k<3;++k){
            V q=mirror(pos[k],p)-c.eye;float z=dot(q,c.f);
            const float data[]={dot(q,c.r)/c.tanHalf,dot(q,c.u)*c.aspect/c.tanHalf,(10000.f/(10000.f-.08f))*z-(10000.f*.08f)/(10000.f-.08f),z,
                uv[k].x,uv[k].y,light_[ref.id][k],float(t.mat),opacity(mat),sign*dot(pos[k]-p.p,p.n)-.0001f};
            plan.projectedVertices.insert(plan.projectedVertices.end(),std::begin(data),std::end(data));
        }
    }
public:
    Session(const v16::Runtime& rt,const v16::SceneDatabase& db):rt_(rt){
        if(!objectMap(rt,owners_))return;
        trace_=std::make_unique<Traversal>(rt.renderScene,rt.lumaTraceBvh4,owners_);
        if(!trace_->valid()){trace_.reset();return;}
        catalogue_=staticCandidates(rt,db,Mode::Reflection);planes_.resize(size_t(rt.objects.size())+1);
        for(const auto& c:catalogue_)planes_[c.object]=planeFor(rt.renderScene,rt.objects[int(c.object)-1]);
        for(int i=0;i<rt.objects.size();++i){const auto& o=rt.objects[i];if(o.triangleCount<=0)continue;V lo{INF,INF,INF},hi{-INF,-INF,-INF};
            for(int j=0;j<o.triangleCount;++j){const auto& t=rt.renderScene.obj[size_t(o.firstTriangle)+j];
                lo={std::min(lo.x,t.mn.x),std::min(lo.y,t.mn.y),std::min(lo.z,t.mn.z)};
                hi={std::max(hi.x,t.mx.x),std::max(hi.y,t.mx.y),std::max(hi.z,t.mx.z)};}
            sources_.push_back({uint32_t(i)+1,lo,hi});}
        light_.resize(rt.renderScene.obj.size());lightStamp_.resize(light_.size(),0);
    }
    bool valid()const{return bool(trace_);}
    const EscargotPacketBuild& build(const Camera& c,const ShadowMap& sh,int w,int h,Metrics& m){
        ++frame_;float day;V sun;escargotLightingSnapshot(day,sun);
        std::array<float,16> key={c.eye.x,c.eye.y,c.eye.z,c.f.x,c.f.y,c.f.z,c.r.x,c.r.y,c.r.z,c.u.x,c.u.y,c.u.z,c.tanHalf,c.aspect,float(w),float(h)};
        const bool lightChanged=day_!=day||length(sun-sun_)>1e-5f;
        if(lightChanged){day_=day;sun_=sun;++epoch_;}
        if(cacheValid_&&key==key_&&!lightChanged&&frame_%8!=0){m=cachedMetrics_;m.rays=m.mini=m.lightingSamples=0;m.reused=true;return cached_;}
        cached_=EscargotPacketBuild{};auto& plan=cached_;plan.rasterProjected=true;
        plan.data.resize((size_t(rt_.objects.size())+2)*4,0);plan.data[0]=float(rt_.objects.size());
        auto candidates=projectCandidates(catalogue_,c,w,h);
        std::stable_sort(candidates.begin(),candidates.end(),[&](const Candidate& a,const Candidate& b){
            bool aw=waterMaterial(rt_.renderScene.mat[a.material]),bw=waterMaterial(rt_.renderScene.mat[b.material]);
            return aw!=bw?aw:a.priority>b.priority;});
        std::vector<Candidate> admitted;
        for(const auto& a:candidates){if(!planes_[a.object].valid){++m.curved;continue;}if(admitted.size()>=maxReceivers){++m.deferred;continue;}admitted.push_back(a);}
        const auto& s=rt_.renderScene;
        for(size_t k=0;k<admitted.size();++k){
            const auto& a=admitted[k];auto p=planes_[a.object];uint32_t quota=m.rays+(rayBudget-m.rays)/uint32_t(admitted.size()-k);
            discover(a,c,w,h,quota,m);
            struct Ranked{uint32_t id;double score;bool seen;};
            std::vector<Ranked> ranked;auto& history=seen_[a.object];
            for(const auto& b:sources_){if(b.id==a.object)continue;
                Rect r=intersection(a.bounds,mirroredBounds(b.lo,b.hi,p,c,w,h));if(!r.valid())continue;
                const auto it=history.find(b.id);bool observed=it!=history.end();
                // Geometry admits unobserved objects, so sparse misses are not holes.
                const V center=(b.lo+b.hi)*.5f;float depth=std::max(.5f,length(center-c.eye));
                ranked.push_back({b.id,double(r.w)*r.h/(1.+.04*depth),observed});}
            std::sort(ranked.begin(),ranked.end(),[](const Ranked& x,const Ranked& y){
                if(x.seen!=y.seen)return x.seen>y.seen;return x.score==y.score?x.id<y.id:x.score>y.score;});
            std::vector<TriangleRef> opaque,transparent;
            const uint32_t limit=(triangleBudget-m.triangles)/uint32_t(admitted.size()-k);
            uint32_t count=0,usedSources=0;
            const float sign=dot(c.eye-p.p,p.n)>=0?1.f:-1.f;
            for(const auto& b:ranked){const auto& o=rt_.objects[int(b.id)-1];
                if(usedSources>=maxSources||uint32_t(o.triangleCount)>limit-count){++m.omitted;continue;}
                uint32_t before=count;
                for(int j=0;j<o.triangleCount;++j){uint32_t id=uint32_t(o.firstTriangle+j);const auto& t=s.obj[id];const auto& mat=s.mat[t.mat];
                    if(waterMaterial(mat)||opacity(mat)<.001f)continue;
                    if(std::max({sign*dot(t.a-p.p,p.n),sign*dot(t.b-p.p,p.n),sign*dot(t.c-p.p,p.n)})<=.0001f)continue;
                    // Narrow footprint test using the reflected triangle AABB.
                    Rect bounds=intersection(a.bounds,mirroredBounds(t.mn,t.mx,p,c,w,h));if(!bounds.valid())continue;
                    float depth=dot(mirror((t.a+t.b+t.c)/3.f,p)-c.eye,c.f);
                    (opacity(mat)>=.999f?opaque:transparent).push_back({id,depth});++count;
                }
                if(count>before){++usedSources;++m.sources;}
            }
            std::sort(transparent.begin(),transparent.end(),[](const TriangleRef& x,const TriangleRef& y){return x.depth==y.depth?x.id<y.id:x.depth>y.depth;});
            EscargotProjectedRange range;range.receiver=a.object;range.first=uint32_t(plan.projectedVertices.size()/10);
            range.opaque=uint32_t(opaque.size()*3);range.transparent=uint32_t(transparent.size()*3);
            range.x=a.bounds.x;range.y=a.bounds.y;range.w=a.bounds.w;range.h=a.bounds.h;
            for(const auto& t:opaque)emit(plan,t,p,c,sh,m);for(const auto& t:transparent)emit(plan,t,p,c,sh,m);
            plan.data[4*(size_t(a.object)+1)]=float(plan.projectedRanges.size()+1);
            plan.projectedRanges.push_back(range);m.triangles+=count;++m.receivers;
        }
        key_=key;cacheValid_=true;cachedMetrics_=m;return cached_;
    }
};
}
