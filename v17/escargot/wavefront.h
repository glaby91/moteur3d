#pragma once
#include "sampling.h"
#include <chrono>
namespace m3d::escargot {
// Independent fixed-quota paths allow one query per live sample in each
// dispatch. Unlike the old sequential quota redistribution, unused path
// quotas are not transferred. Both CPU and GPU wavefront use this policy.
struct WaveStats {uint32_t batches=0,maxBatch=0;double traceMs=0,prepareMs=0,dispatchMs=0;};
struct WavePath {
    size_t surface=0,sample=0;
    uint32_t quota=0,used=0,crossings=0,previous=0xffffffffu;
    Ray ray{};float low=0;
    bool secondary=false,done=false;
    std::map<uint32_t,uint32_t> sourceCrossings;
};
inline EscargotRayQuery waveQuery(const WavePath& p){
    return {{p.ray.o.x,p.ray.o.y,p.ray.o.z},p.low,{p.ray.d.x,p.ray.d.y,p.ray.d.z},p.previous};
}
inline bool decodeWaveHit(const Scene& scene,const std::vector<uint32_t>& owners,const EscargotRayQuery& query,const EscargotRayHit& raw,HitSample& hit){
    hit={};if(raw.status==0)return true;
    if(raw.status!=1||raw.primitive>=scene.obj.size()||raw.primitive>=owners.size()||!owners[raw.primitive]||
       raw.primitive==query.previousPrimitive||!std::isfinite(raw.depth)||raw.depth<=query.low||
       !std::isfinite(raw.u)||!std::isfinite(raw.v)||raw.u<0||raw.v<0||raw.u+raw.v>1)return false;
    const auto& o=scene.obj[raw.primitive];const float u=raw.u,v=raw.v,w=1-u-v;
    const V origin{query.origin[0],query.origin[1],query.origin[2]},dir{query.direction[0],query.direction[1],query.direction[2]};
    const V nn=o.n0*w+o.n1*u+o.n2*v,geom=norm(cross(o.b-o.a,o.c-o.a));
    hit={raw.primitive,owners[raw.primitive],raw.depth,origin+dir*raw.depth,length(nn)>1e-8f?norm(nn):geom,o.uv0*w+o.uv1*u+o.uv2*v,geom};
    hit.uv.z=0;return finite(hit.p)&&finite(hit.n)&&finite(hit.uv)&&finite(hit.geometricNormal);
}
inline void advanceWave(WavePath& p,Surface& sf,Sample& s,const Scene& scene,const Options& opt,const HitSample& hit,Result& stats){
    ++p.crossings;
    if(!p.secondary){
        if(!hit.object){s.receiver=State::Zero;s.complete=true;p.done=true;return;}
        if(hit.object!=sf.candidate.object){
            s.primaryTransmission*=transmission(scene.mat[scene.obj[hit.primitive].mat]);
            if(s.primaryTransmission<=.001f){s.receiver=State::Zero;s.complete=true;p.done=true;return;}
            p.low=hit.depth;p.previous=hit.primitive;
            if(p.crossings>=opt.maxCrossings)p.done=true;
            return;
        }
        s.receiver=State::Measured;s.position=hit.p;s.receiverDepth=hit.depth;
        const V authoredN=hit.geometricNormal;const bool entering=dot(authoredN,p.ray.d)<0.f;
        const V geometric=entering?authoredN:authoredN*-1.f;
        s.normal=dot(hit.n,geometric)<0.f?hit.n*-1.f:hit.n;
        if(dot(s.normal,p.ray.d)>0.f)s.normal=geometric;
        const auto& material=scene.mat[sf.candidate.material];
        s.receiverWeight=opticalWeight(material,s.normal,p.ray.d,opt.mode)*s.primaryTransmission;
        if(opt.mode==Mode::Reflection){
            s.direction=reflect(p.ray.d,s.normal);
            if(dot(s.direction,geometric)<=0.f)s.direction=reflect(p.ray.d,geometric);
        }else if(!refract(p.ray.d,geometric,entering?1.f/clampf(material.refractiveIndex,1.f,3.f):clampf(material.refractiveIndex,1.f,3.f),s.direction)){
            s.receiverWeight=0;s.complete=true;p.done=true;return;
        }
        s.directionValid=true;++stats.paths;
        p.secondary=true;p.crossings=0;p.previous=0xffffffffu;p.ray={hit.p,s.direction};
        p.low=std::max(1e-6f,4.f*std::numeric_limits<float>::epsilon()*std::max(1.f,length(hit.p)));
        return;
    }
    if(!hit.object){s.complete=true;p.done=true;return;}
    p.low=hit.depth;p.previous=hit.primitive;
    if(opt.mode!=Mode::Refraction||hit.object!=sf.candidate.object){
        const auto mid=scene.obj[hit.primitive].mat;const auto& mat=scene.mat[mid];
        const LayerKey key{hit.object,p.sourceCrossings[hit.object]++,mid};
        Value v;v.state=State::Measured;v.object=hit.object;v.primitive=hit.primitive;v.material=mid;
        v.depth=hit.depth;v.uv=hit.uv;v.normal=dot(hit.n,p.ray.d)>0?hit.n*-1.f:hit.n;
        v.opacity=opacity(mat);v.visibility=1;v.weight=1;
        s.values.emplace(key,v);++stats.measured;
        if(v.opacity>=.999f){s.complete=true;p.done=true;return;}
    }
    if(p.crossings>=opt.maxCrossings)p.done=true;
}
// Trace callback returns HitSample in request order. GPU only supplies
// actual intersections; there is no speculative ray, nor CPU fallback.
template<class Trace>
bool sampleWavefront(std::vector<Surface>& surfaces,const Scene& scene,const Camera& cam,int w,int h,const Options& opt,Result& stats,WaveStats& ws,Trace&& trace){
    std::vector<WavePath> paths;paths.reserve(opt.rayBudget/2);
    uint64_t allocated=0;
    for(size_t i=0;i<surfaces.size();++i){
        auto& sf=surfaces[i];if(sf.deferred)continue;
        if(!initializeLattice(sf)){++stats.deferred;continue;}
        const auto order=spiral(sf.cols,sf.rows);const uint32_t n=uint32_t(order.size()),q=sf.candidate.budget;
        if(!n||q/n<2){stats.error="invalid wavefront quota";return false;}
        allocated+=q;if(allocated>opt.rayBudget){stats.error="wavefront quota overflow";return false;}
        for(uint32_t j=0;j<n;++j){
            WavePath p;p.surface=i;p.sample=order[j];p.quota=q/n+uint32_t(j<q%n);
            const auto& s=sf.samples[p.sample];p.ray=screenRay(cam,s.x,s.y,w,h);
            p.low=.08f/std::max(1e-6f,dot(p.ray.d,cam.f));paths.push_back(std::move(p));
        }
    }
    std::vector<size_t> active;std::vector<EscargotRayQuery> queries;std::vector<HitSample> hits;
    active.reserve(paths.size());queries.reserve(paths.size());hits.reserve(paths.size());
    for(;;){
        active.clear();queries.clear();hits.clear();
        for(size_t i=0;i<paths.size();++i){const auto& p=paths[i];if(!p.done&&p.used<p.quota){active.push_back(i);queries.push_back(waveQuery(p));}}
        if(active.empty())break;
        if(queries.size()>opt.rayBudget-stats.rays||ws.batches>=2*opt.maxCrossings){stats.error="wavefront safety bound";return false;}
        ++ws.batches;ws.maxBatch=std::max(ws.maxBatch,uint32_t(queries.size()));
        const auto start=std::chrono::steady_clock::now();
        if(!trace(queries,hits,stats.error)||hits.size()!=queries.size()){if(stats.error.empty())stats.error="wavefront trace failure";return false;}
        ws.traceMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        for(size_t j=0;j<active.size();++j){
            auto& p=paths[active[j]];auto& sf=surfaces[p.surface];auto& s=sf.samples[p.sample];
            ++p.used;++sf.queries;++stats.rays;
            if(opt.captureQueries)sf.recorded.push_back({queries[j],hits[j].object?hits[j].primitive:0xffffffffu,hits[j].depth});
            advanceWave(p,sf,s,scene,opt,hits[j],stats);
        }
    }
    for(auto& sf:surfaces){
        if(sf.deferred)continue;sf.complete=true;
        for(const auto& s:sf.samples){if(s.complete)++sf.completePaths;else{++sf.unknownPaths;sf.complete=false;}}
        stats.completePaths+=sf.completePaths;stats.unknownPaths+=sf.unknownPaths;
    }
    return true;
}
}
