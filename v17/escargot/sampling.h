#pragma once
#include "footprint.h"
#include "bvh4.h"
namespace m3d::escargot {
// Fixed coarse lattice, ordered by square spiral. No adaptive refinement.
inline std::vector<size_t> spiral(int cols,int rows){
    std::vector<size_t> out;out.reserve(size_t(cols)*rows);
    const int cx=(cols-1)/2,cy=(rows-1)/2;
    auto add=[&](int x,int y){if(x>=0&&y>=0&&x<cols&&y<rows)out.push_back(size_t(y)*cols+x);};
    add(cx,cy);
    for(int r=1;out.size()<size_t(cols)*rows;++r){
        for(int x=-r+1;x<=r;++x)add(cx+x,cy-r);
        for(int y=-r+1;y<=r;++y)add(cx+r,cy+y);
        for(int x=r-1;x>=-r;--x)add(cx+x,cy+r);
        for(int y=r-1;y>=-r;--y)add(cx-r,cy+y);
    }return out;
}
inline float opacity(const Material& m){return 1.f-sat(float(m.transmissionPercent)/100.f);}
inline float opticalWeight(const Material& m,V n,V d,Mode mode){
    const float ior=clampf(m.refractiveIndex,1.f,3.f),q=(ior-1)/(ior+1);
    const float f0=std::max(q*q,std::max(sat(m.metallic)*.85f,float(m.reflectivityPercent)/100.f));
    const float f=f0+(1-f0)*std::pow(1-sat(-dot(n,d)),5.f);
    return mode==Mode::Reflection?sat(f):sat((1-f)*(m.waterSurface?1.f:float(m.transmissionPercent)/100.f));
}
inline void sampleSurface(Surface& sf,Traversal& trace,const Scene& scene,const Camera& cam,int w,int h,const Options& opt,Result& stats){
    const auto& r=sf.candidate.bounds;
    const uint32_t quota=sf.candidate.budget;
    if(quota<2){sf.deferred=true;return;}
    // Reserve room for two BVH queries per sample; extra transparent crossings
    // consume the same hard quota. No additional samples after the fixed pass.
    const uint32_t target=std::max(1u,quota/2);
    sf.step=std::max(1,int(std::ceil(std::sqrt(double(r.w)*r.h/target))));
    auto dims=[&]{sf.cols=(r.w+sf.step-1)/sf.step;sf.rows=(r.h+sf.step-1)/sf.step;};
    dims();while(uint64_t(sf.cols)*sf.rows>target){++sf.step;dims();}
    sf.samples.resize(size_t(sf.cols)*sf.rows);
    for(int y=0;y<sf.rows;++y)for(int x=0;x<sf.cols;++x){
        auto& s=sf.samples[size_t(y)*sf.cols+x];
        s.x=r.x+x*sf.step+std::min(sf.step,r.w-x*sf.step)/2;
        s.y=r.y+y*sf.step+std::min(sf.step,r.h-y*sf.step)/2;
    }
    auto query=[&](const Ray& ray,float low,HitSample& hit)->int{
        if(sf.queries>=quota)return -1;
        ++sf.queries;++stats.rays;return trace.closest(ray,low,hit)?1:0;
    };
    for(size_t id:spiral(sf.cols,sf.rows)){
        if(sf.queries+2>quota)break;
        auto& s=sf.samples[id];const Ray primary=screenRay(cam,s.x,s.y,w,h);
        HitSample hit;float low=.08f;bool found=false,blocked=false;
        for(uint32_t crossing=0;crossing<opt.maxCrossings;++crossing){
            const int q=query(primary,low,hit);if(q<0)break;
            if(!q){blocked=true;break;}
            if(hit.object==sf.candidate.object){found=true;break;}
            if(opacity(scene.mat[scene.obj[hit.primitive].mat])>=.999f){blocked=true;break;}
            low=hit.depth+.0005f;
        }
        if(!found){if(blocked){s.receiver=State::Zero;s.complete=true;}continue;}
        s.receiver=State::Measured;s.position=hit.p;s.receiverDepth=hit.depth;
        const V authoredN=hit.n;const bool entering=dot(authoredN,primary.d)<0.f;
        s.normal=entering?authoredN:authoredN*-1.f;
        const auto& material=scene.mat[sf.candidate.material];
        s.receiverWeight=opticalWeight(material,s.normal,primary.d,opt.mode);
        if(opt.mode==Mode::Reflection)s.direction=reflect(primary.d,s.normal);
        else if(!refract(primary.d,s.normal,entering?1.f/clampf(material.refractiveIndex,1.f,3.f):clampf(material.refractiveIndex,1.f,3.f),s.direction)){
            s.receiverWeight=0;s.complete=true;continue; // TIR belongs to reflection pass
        }
        const Ray secondary{hit.p+s.direction*.0005f,s.direction};low=.0005f;
        std::map<uint32_t,uint32_t> crossings;
        for(uint32_t j=0;j<opt.maxCrossings;++j){
            HitSample src;const int q=query(secondary,low,src);
            if(q<0)break;
            if(!q){s.complete=true;break;}
            low=src.depth+.0005f;
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
    for(const auto& s:sf.samples)if(!s.complete){sf.complete=false;break;}
}
}
