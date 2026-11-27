#pragma once
#include "matrix.h"
namespace m3d::escargot {
inline float linear(float x){x=sat(x);return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
inline V decode(uint32_t p){return {linear(float((p>>16)&255)/255),linear(float((p>>8)&255)/255),linear(float(p&255)/255)};}
inline V decode565(uint16_t p){return {linear(float((p>>11)&31)/31),linear(float((p>>5)&63)/63),linear(float(p&31)/31)};}
inline V sourceTexture(const Material& m,V uv){
    auto sample=[&](uint32_t n,auto get){
        if(!n||!std::isfinite(uv.x)||!std::isfinite(uv.y))return V{};
        const float u=m.imageAlbedo?sat(uv.x):uv.x-std::floor(uv.x),v=m.imageAlbedo?sat(uv.y):uv.y-std::floor(uv.y);
        const float fx=u*n-.5f,fy=v*n-.5f;const int x=int(std::floor(fx)),y=int(std::floor(fy));
        auto idx=[&](int q){return m.imageAlbedo?std::clamp(q,0,int(n)-1):((q%int(n))+int(n))%int(n);};
        const float a=fx-x,b=fy-y;
        return get(size_t(idx(y))*n+idx(x))*((1-a)*(1-b))+get(size_t(idx(y))*n+idx(x+1))*(a*(1-b))+
            get(size_t(idx(y+1))*n+idx(x))*((1-a)*b)+get(size_t(idx(y+1))*n+idx(x+1))*(a*b);
    };
    if(!m.colorMip.empty()){const auto& l=m.colorMip.front();if(l.p.size()==size_t(l.size)*l.size)return sample(l.size,[&](size_t i){return decode(l.p[i]);});}
    if(!m.mip.empty()){const auto& l=m.mip.front();if(l.p.size()==size_t(l.size)*l.size)return sample(l.size,[&](size_t i){return decode565(l.p[i]);});}
    return {.2f,.2f,.2f};
}
// Reference radiance proxy, not the existing LumaTrace lighting shader.
// Crucially texture is sampled AFTER UV reconstruction and depth ordering.
inline V sourceColor(const Material& m,const Value& v){
    const V base=sourceTexture(m,v.uv);
    const float diffuse=.12f+.35f*daylight+.48f*daylight*sat(dot(v.normal,sun));
    return base*diffuse+m.emissionColor*std::max(0.f,m.emissionIntensity);
}
inline void compose(Surface& sf,const Scene& scene,Result& stats){
    const Rect r=sf.candidate.bounds;sf.image.resize(size_t(r.w)*r.h);
    std::vector<Value> values;values.reserve(sf.layers.size());
    for(int y=r.y;y<r.y+r.h;++y)for(int x=r.x;x<r.x+r.w;++x){
        auto& p=sf.image[size_t(y-r.y)*r.w+x-r.x];
        const auto complete=nearestComplete(sf,x,y);++stats.completeSearches;
        auto receiver=complete;if(!sf.sameKnownSet){receiver=nearestReceiver(sf,x,y);++stats.receiverSearches;}
        if(!receiverAt(sf,x,y,p.receiverDepth,p.weight,receiver)){++stats.zero;continue;}
        values.clear();
        for(const auto& kv:sf.layers){
            if(!kv.second.bounds.contains(x,y))continue;
            ++stats.layerVisits;auto v=reconstruct(sf,kv.second,x,y,complete,&stats);
            if(v.state==State::Zero){++stats.zero;continue;}
            if(!v.object||v.visibility<=0||v.material>=scene.mat.size())continue;
            if(v.state==State::Reconstructed)++stats.reconstructed;
            values.push_back(v);
        }
        // Optical depth is local per destination cell, not distance to camera.
        std::sort(values.begin(),values.end(),[](const Value& a,const Value& b){
            if(a.depth!=b.depth)return a.depth>b.depth;
            if(a.object!=b.object)return a.object<b.object;return a.primitive<b.primitive;
        });
        for(const auto& v:values){
            const float alpha=sat(v.visibility*v.opacity);
            p.rgb=sourceColor(scene.mat[v.material],v)*alpha+p.rgb*(1-alpha);
            p.alpha=alpha+p.alpha*(1-alpha);
        }
        // RGB is premultiplied; optical weight remains separate for compositor.
    }
}
inline std::vector<Pixel> composeScreen(const Result& frame,int w,int h){
    // Diagnostic projection only. Native renderer must match actual receiving
    // surface depth/ID; this projection chooses the closest reconstructed one.
    std::vector<Pixel> out(size_t(w)*h);
    for(const auto& s:frame.surfaces){if(s.image.empty())continue;const auto r=s.candidate.bounds;
        for(int y=0;y<r.h;++y)for(int x=0;x<r.w;++x){
            const auto& p=s.image[size_t(y)*r.w+x];if(p.alpha<=0||p.weight<=0)continue;
            auto& q=out[size_t(y+r.y)*w+x+r.x];
            if(!q.alpha||p.receiverDepth<q.receiverDepth)q=p;
        }
    }return out;
}
}
