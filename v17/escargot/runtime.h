#pragma once
#include "composite.h"
#include <memory>
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
    Result frame(const Camera& cam,int w,int h,const Options& opt={},bool composeCpu=true){
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
        for(const auto& item:c){
            Surface s;s.candidate=item;const uint64_t cells=uint64_t(item.bounds.w)*item.bounds.h;
            if(!item.budget){s.deferred=true;++out.deferred;}
            else{sampleSurface(s,*trace_,rt_.renderScene,cam,w,h,opt,out);makeLayers(s);out.outputCells+=cells;}
            out.surfaces.push_back(std::move(s));
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
