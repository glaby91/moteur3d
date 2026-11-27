#pragma once
#include "composite.h"
namespace m3d::escargot {
inline Result run(const v16::Runtime& rt,const v16::SceneDatabase& db,const Camera& cam,int w,int h,const Options& opt={}){
    using Clock=std::chrono::steady_clock;Result out;
    if(!validOptions(opt)||w<1||h<1||w>opt.maxDimension||h>opt.maxDimension||!validCamera(cam)){out.error="invalid options/camera";return out;}
    if(rt.lumaTraceBvh4.nodes.empty()||rt.lumaTraceBvh4.primitiveCount!=rt.renderScene.order.size()){out.error="missing BVH4";return out;}
    std::vector<uint32_t> owners;if(!objectMap(rt,owners)){out.error="invalid object ranges";return out;}
    auto c=candidates(rt,db,cam,w,h,opt.mode);
    // Filter memory admission before allocating rays; no quota wasted on a deferred footprint.
    std::vector<Candidate> admitted;uint64_t cellsReserved=0;
    for(const auto& item:c){const uint64_t cells=uint64_t(item.bounds.w)*item.bounds.h;
        if(cells<=opt.maxOutputCells-cellsReserved&&admitted.size()<opt.rayBudget/2){admitted.push_back(item);cellsReserved+=cells;}}
    distribute(admitted,opt.rayBudget);
    std::map<uint32_t,uint32_t> quotas;for(const auto& item:admitted)quotas.emplace(item.object,item.budget);
    for(auto& item:c){auto it=quotas.find(item.object);if(it!=quotas.end())item.budget=it->second;}
    out.candidates=uint32_t(c.size());Traversal trace(rt.renderScene,rt.lumaTraceBvh4,owners);
    if(!trace.valid()){out.error="invalid BVH4 geometry/order";return out;}
    auto time=Clock::now();
    for(const auto& item:c){
        Surface s;s.candidate=item;const uint64_t cells=uint64_t(item.bounds.w)*item.bounds.h;
        if(!item.budget||cells>opt.maxOutputCells-out.outputCells){s.deferred=true;++out.deferred;}
        else{
            sampleSurface(s,trace,rt.renderScene,cam,w,h,opt,out);
            makeLayers(s);out.outputCells+=cells;
        }
        out.surfaces.push_back(std::move(s));
    }
    out.matrixMs=std::chrono::duration<double,std::milli>(Clock::now()-time).count();time=Clock::now();
    escargotLightingSnapshot(proxyDaylight,proxySun);
    for(auto& s:out.surfaces)if(!s.deferred)compose(s,rt.renderScene,out);
    out.compositionMs=std::chrono::duration<double,std::milli>(Clock::now()-time).count();
    out.ok=out.rays<=opt.rayBudget;return out;
}
}
