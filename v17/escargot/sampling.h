#pragma once
#include "footprint.h"
#include "bvh4.h"
namespace m3d::escargot {
// Fixed coarse lattice, ordered by square spiral. No adaptive refinement.
inline std::vector<size_t> spiral(int cols,int rows){
    std::vector<size_t> out;if(cols<=0||rows<=0)return out;out.reserve(size_t(cols)*rows);
    const int cx=(cols-1)/2,cy=(rows-1)/2;
    auto add=[&](int x,int y){if(x>=0&&y>=0&&x<cols&&y<rows)out.push_back(size_t(y)*cols+x);};
    add(cx,cy);
    for(int r=1;out.size()<size_t(cols)*rows;++r){
        // Clip each side before iteration: a long thin footprint is O(cells+rings),
        // not O(max(width,height)^2). Exactly the same in-bounds spiral order.
        if(cy-r>=0)for(int x=std::max(-r+1,-cx);x<=std::min(r,cols-1-cx);++x)add(cx+x,cy-r);
        if(cx+r<cols)for(int y=std::max(-r+1,-cy);y<=std::min(r,rows-1-cy);++y)add(cx+r,cy+y);
        if(cy+r<rows)for(int x=std::min(r-1,cols-1-cx);x>=std::max(-r,-cx);--x)add(cx+x,cy+r);
        if(cx-r>=0)for(int y=std::min(r-1,rows-1-cy);y>=std::max(-r,-cy);--y)add(cx-r,cy+y);
    }return out;
}
inline float opacity(const Material& m){return 1.f-transmission(m);}
inline float opticalWeight(const Material& m,V n,V d,Mode mode){
    const float ior=clampf(m.refractiveIndex,1.f,3.f),q=(ior-1)/(ior+1);
    const float f0=std::max(q*q,std::max(sat(m.metallic)*.85f,float(m.reflectivityPercent)/100.f));
    const float f=f0+(1-f0)*std::pow(1-sat(-dot(n,d)),5.f);
    return mode==Mode::Reflection?sat(f):sat((1-f)*transmission(m));
}
inline void sampleSurface(Surface& sf,Traversal& trace,const Scene& scene,const Camera& cam,int w,int h,const Options& opt,Result& stats){
    const auto& r=sf.candidate.bounds;
    const uint32_t quota=sf.candidate.budget;
    if(quota<2){sf.deferred=true;return;}
    // Reserve room for two BVH queries per sample; extra transparent crossings
    // consume the same hard quota. No additional samples after the fixed pass.
    const uint32_t target=std::max(1u,quota/4); // reserve more than minimum for transparent paths
    // Smallest admissible step at or above the former initial estimate.
    // Monotonic cell count allows binary search instead of repeated increments.
    int lower=std::max(1,int(std::ceil(std::sqrt(double(r.w)*r.h/target)))),upper=std::max(r.w,r.h);
    while(lower<upper){
        const int mid=lower+(upper-lower)/2;
        const uint64_t count=uint64_t((r.w+mid-1)/mid)*((r.h+mid-1)/mid);
        if(count>target)lower=mid+1;else upper=mid;
    }
    sf.step=lower;sf.cols=(r.w+sf.step-1)/sf.step;sf.rows=(r.h+sf.step-1)/sf.step;
    sf.samples.resize(size_t(sf.cols)*sf.rows);
    for(int y=0;y<sf.rows;++y)for(int x=0;x<sf.cols;++x){
        auto& s=sf.samples[size_t(y)*sf.cols+x];
        s.x=r.x+x*sf.step+std::min(sf.step,r.w-x*sf.step)/2;
        s.y=r.y+y*sf.step+std::min(sf.step,r.h-y*sf.step)/2;
    }
    uint32_t pathLimit=quota;
    auto query=[&](const Ray& ray,float low,HitSample& hit)->int{
        if(sf.queries>=pathLimit)return -1;
        ++sf.queries;++stats.rays;return trace.closest(ray,low,hit)?1:0;
    };
    uint32_t remaining=uint32_t(sf.samples.size());
    for(size_t id:spiral(sf.cols,sf.rows)){
        if(!remaining||sf.queries+2>quota)break;
        // No adaptive rays: spread the quota across every fixed lattice point.
        pathLimit=sf.queries+(quota-sf.queries)/remaining;
        --remaining;
        auto& s=sf.samples[id];const Ray primary=screenRay(cam,s.x,s.y,w,h);
        HitSample hit;float low=.08f/std::max(1e-6f,dot(primary.d,cam.f));bool found=false,blocked=false;
        for(uint32_t crossing=0;crossing<opt.maxCrossings;++crossing){
            const int q=query(primary,low,hit);if(q<0)break;
            if(!q){blocked=true;break;}
            if(hit.object==sf.candidate.object){found=true;break;}
            s.primaryTransmission*=transmission(scene.mat[scene.obj[hit.primitive].mat]);
            if(s.primaryTransmission<=.001f){blocked=true;break;}
            low=hit.depth;
        }
        if(!found){if(blocked){s.receiver=State::Zero;s.complete=true;}continue;}
        s.receiver=State::Measured;s.position=hit.p;s.receiverDepth=hit.depth;
        const V authoredN=hit.geometricNormal;const bool entering=dot(authoredN,primary.d)<0.f;
        const V geometric=entering?authoredN:authoredN*-1.f;
        s.normal=dot(hit.n,geometric)<0.f?hit.n*-1.f:hit.n;
        if(dot(s.normal,primary.d)>0.f)s.normal=geometric;
        const auto& material=scene.mat[sf.candidate.material];
        s.receiverWeight=opticalWeight(material,s.normal,primary.d,opt.mode)*s.primaryTransmission;
        if(opt.mode==Mode::Reflection){s.direction=reflect(primary.d,s.normal);if(dot(s.direction,geometric)<=0.f)s.direction=reflect(primary.d,geometric);}
        else if(!refract(primary.d,geometric,entering?1.f/clampf(material.refractiveIndex,1.f,3.f):clampf(material.refractiveIndex,1.f,3.f),s.direction)){
            s.receiverWeight=0;s.complete=true;continue; // TIR belongs to reflection pass
        }
        s.directionValid=true;
        const Ray secondary{hit.p,s.direction};low=std::max(1e-6f,4.f*std::numeric_limits<float>::epsilon()*std::max(1.f,length(hit.p)));
        std::map<uint32_t,uint32_t> crossings;
        for(uint32_t j=0;j<opt.maxCrossings;++j){
            HitSample src;const int q=query(secondary,low,src);
            if(q<0)break;
            if(!q){s.complete=true;break;}
            low=src.depth;
            // Do not reflect the same receiver's own triangle surfaces.
            // One-interface refraction is explicit: exit bending is a later experiment.
            if(src.object==sf.candidate.object)continue;
            const auto mid=scene.obj[src.primitive].mat;
            const auto& mat=scene.mat[mid];
            const LayerKey key{src.object,crossings[src.object]++,mid};
            Value v;v.state=State::Measured;v.object=src.object;v.primitive=src.primitive;v.material=mid;
            v.depth=src.depth;v.uv=src.uv;v.normal=dot(src.n,secondary.d)>0?src.n*-1.f:src.n;
            v.opacity=opacity(mat);v.visibility=1;v.weight=1;
            s.values.emplace(key,v);++stats.measured;
            if(v.opacity>=.999f){s.complete=true;break;}
        }
        ++stats.paths;
    }
    sf.complete=true;
    for(const auto& s:sf.samples){if(s.complete)++sf.completePaths;else{++sf.unknownPaths;sf.complete=false;}}
    stats.completePaths+=sf.completePaths;stats.unknownPaths+=sf.unknownPaths;
}
}
