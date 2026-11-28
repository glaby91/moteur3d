#pragma once
#include "composite.h"
#include "wavefront.h"
#include <memory>
#include <atomic>
#include <future>
#include <thread>
namespace m3d::escargot {
using FrameClock=std::chrono::steady_clock;
inline double elapsedMs(FrameClock::time_point t){return std::chrono::duration<double,std::milli>(FrameClock::now()-t).count();}
// Lifetime contract: the immutable Runtime and SceneDatabase outlive this
// session. Recreate after scene or author-policy changes. No temporal image cache.
class Prepared {
    const v16::Runtime& rt_;
    Mode mode_;
    uint64_t revision_;
    std::vector<uint32_t> owners_;
    std::vector<Candidate> catalogue_;
    std::unique_ptr<Traversal> trace_;
    std::string error_;
    double prepareMs_=0;
public:
    Prepared(const v16::Runtime& rt,const v16::SceneDatabase& db,Mode mode):rt_(rt),mode_(mode),revision_(rt.renderScene.gpuCacheRevision){
        const auto t=FrameClock::now();
        if(!objectMap(rt,owners_)){error_="invalid object ranges";return;}
        trace_=std::make_unique<Traversal>(rt.renderScene,rt.lumaTraceBvh4,owners_);
        if(!trace_->valid()){error_="invalid BVH4 geometry/order";return;}
        catalogue_=staticCandidates(rt,db,mode);
        prepareMs_=elapsedMs(t);
    }
    Prepared(const Prepared&)=delete;Prepared& operator=(const Prepared&)=delete;
    const std::vector<uint32_t>& owners()const{return owners_;}
    Result frame(const Camera& cam,int w,int h,const Options& opt={},bool composeCpu=true,bool wavefront=false,bool gpuWave=false,WaveStats* waveStats=nullptr){
        const auto begin=FrameClock::now();Result out;out.prepareMs=prepareMs_;
        if(!error_.empty()){out.error=error_;return out;}
        if(rt_.renderScene.gpuCacheRevision!=revision_||opt.mode!=mode_){out.error="stale prepared scene/mode";return out;}
        if(!validOptions(opt)||w<1||h<1||w>opt.maxDimension||h>opt.maxDimension||!validCamera(cam)){out.error="invalid options/camera";return out;}
        out.staticCandidates=uint32_t(catalogue_.size());
        auto t=FrameClock::now();auto c=projectCandidates(catalogue_,cam,w,h);out.candidateMs=elapsedMs(t);
        t=FrameClock::now();
        std::vector<Candidate> admitted;uint64_t cellsReserved=0;
        for(const auto& item:c){const uint64_t cells=uint64_t(item.bounds.w)*item.bounds.h;
            if(cells<=opt.maxOutputCells-cellsReserved&&admitted.size()<opt.rayBudget/2){admitted.push_back(item);cellsReserved+=cells;}}
        distribute(admitted,opt.rayBudget);
        std::map<uint32_t,uint32_t> quotas;for(const auto& item:admitted)quotas.emplace(item.object,item.budget);
        for(auto& item:c){auto it=quotas.find(item.object);if(it!=quotas.end())item.budget=it->second;}
        out.candidates=uint32_t(c.size());out.admissionMs=elapsedMs(t);t=FrameClock::now();
        out.surfaces.resize(c.size());std::vector<size_t> jobs;
        for(size_t i=0;i<c.size();++i){
            auto& s=out.surfaces[i];s.candidate=c[i];
            if(!c[i].budget){s.deferred=true;++out.deferred;}
            else{jobs.push_back(i);out.outputCells+=uint64_t(c[i].bounds.w)*c[i].bounds.h;}
        }
        if(wavefront){
            WaveStats localStats;auto& ws=waveStats?*waveStats:localStats;ws={};
            auto traceBatch=[&](const std::vector<EscargotRayQuery>& queries,std::vector<HitSample>& hits,std::string& error){
                hits.resize(queries.size());
                if(!gpuWave){
                    for(size_t i=0;i<queries.size();++i){const auto& q=queries[i];trace_->closest({{q.origin[0],q.origin[1],q.origin[2]},{q.direction[0],q.direction[1],q.direction[2]}},q.low,hits[i],q.previousPrimitive);}
                    return true;
                }
                std::vector<EscargotRayHit> raw;double prepare=0,dispatch=0;std::wstring failure;
                if(!traceEscargotGpuBatch(rt_.renderScene,rt_.lumaTraceBvh4,queries,raw,prepare,dispatch,failure)||raw.size()!=queries.size()){
                    error="native GPU wavefront trace failed: "+std::string(failure.begin(),failure.end());return false;
                }
                ws.prepareMs+=prepare;ws.dispatchMs+=dispatch;
                for(size_t i=0;i<raw.size();++i)if(!decodeWaveHit(rt_.renderScene,owners_,queries[i],raw[i],hits[i])){error="invalid native GPU hit in wavefront";return false;}
                return true;
            };
            if(!sampleWavefront(out.surfaces,rt_.renderScene,cam,w,h,opt,out,ws,traceBatch))return out;
            for(auto& s:out.surfaces)if(!s.deferred)makeLayers(s);
        }else{
        // Large quotas first, original surface ordering unchanged. Each worker
        // owns its traversal scratch/counters; all geometry is immutable.
        std::stable_sort(jobs.begin(),jobs.end(),[&](size_t a,size_t b){return c[a].budget>c[b].budget;});
        const unsigned workers=std::max(1u,std::min({opt.rayWorkers,std::max(1u,std::thread::hardware_concurrency()),
            unsigned(jobs.size()),std::max(1u,opt.rayBudget/256u)}));
        out.rayWorkers=workers;
        std::atomic<size_t> next{0};
        auto work=[&](){
            Traversal local(*trace_);Result counters;
            for(;;){const auto j=next.fetch_add(1,std::memory_order_relaxed);if(j>=jobs.size())break;
                auto& s=out.surfaces[jobs[j]];sampleSurface(s,local,rt_.renderScene,cam,w,h,opt,counters);makeLayers(s);}
            return counters;
        };
        auto merge=[&](const Result& r){out.rays+=r.rays;out.paths+=r.paths;out.measured+=r.measured;
            out.completePaths+=r.completePaths;out.unknownPaths+=r.unknownPaths;};
        std::vector<std::future<Result>> futures;futures.reserve(workers-1);
        for(unsigned i=1;i<workers;++i)futures.push_back(std::async(std::launch::async,work));
        merge(work());for(auto& future:futures)merge(future.get());
        }
        out.matrixMs=elapsedMs(t);t=FrameClock::now();
        escargotLightingSnapshot(proxyDaylight,proxySun);
        if(composeCpu)for(auto& s:out.surfaces)if(!s.deferred)compose(s,rt_.renderScene,out);
        out.compositionMs=elapsedMs(t);out.totalMs=elapsedMs(begin);out.ok=out.rays<=opt.rayBudget;return out;
    }
};
inline Result run(const v16::Runtime& rt,const v16::SceneDatabase& db,const Camera& cam,int w,int h,const Options& opt={}){
    Prepared session(rt,db,opt.mode);return session.frame(cam,w,h,opt);
}
}
