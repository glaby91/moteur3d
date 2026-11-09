#include "m3d_scene_v16.h"
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace m3d::v16 {
namespace {
uint32_t hash(uint32_t x,uint32_t y,uint32_t seed) {
    uint32_t h=x*374761393u+y*668265263u+seed*2246822519u;
    h=(h^(h>>13))*1274126177u;
    return h^(h>>16);
}
float noise(int x,int y,uint32_t seed) {
    return float(hash(uint32_t(x),uint32_t(y),seed)&65535u)/65535.f;
}
uint8_t clampByte(float x){return uint8_t(std::lround(clampf(x,0.f,255.f)));}
uint16_t pack565(float r,float g,float b){
    return uint16_t((uint16_t(std::lround(clampf(r,0.f,1.f)*31))<<11)|
                    (uint16_t(std::lround(clampf(g,0.f,1.f)*63))<<5)|
                    uint16_t(std::lround(clampf(b,0.f,1.f)*31)));
}
float srgbToLinear(float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
float linearToSrgb(float v){return v<=.0031308f?12.92f*v:1.055f*std::pow(v,1.f/2.4f)-.055f;}
uint32_t downsampleColor(uint32_t a,uint32_t b,uint32_t c,uint32_t d){
    auto channel=[&](int shift)->uint32_t {
        const float x=(srgbToLinear(float((a>>shift)&255u)/255.f)+
                       srgbToLinear(float((b>>shift)&255u)/255.f)+
                       srgbToLinear(float((c>>shift)&255u)/255.f)+
                       srgbToLinear(float((d>>shift)&255u)/255.f))*.25f;
        return uint32_t(clampByte(linearToSrgb(x)*255.f));
    };
    return (channel(16)<<16)|(channel(8)<<8)|channel(0);
}
float gray565(uint16_t value){return float((value>>11)&31u)/31.f;}
uint16_t downsampleRough(uint16_t a,uint16_t b,uint16_t c,uint16_t d){
    const float value=(gray565(a)+gray565(b)+gray565(c)+gray565(d))*.25f;
    return pack565(value,value,value);
}
V decodeNormal(uint16_t v) {
    const float x=float((v>>11)&31u)/31.f*2.f-1.f;
    const float y=float((v>>5)&63u)/63.f*2.f-1.f;
    const float z=float(v&31u)/31.f*2.f-1.f;
    return norm({x,y,z});
}
uint16_t encodeNormal(V v){v=norm(v);return pack565(v.x*.5f+.5f,v.y*.5f+.5f,v.z*.5f+.5f);}
uint16_t downsampleNormal(uint16_t a,uint16_t b,uint16_t c,uint16_t d){
    return encodeNormal(decodeNormal(a)+decodeNormal(b)+decodeNormal(c)+decodeNormal(d));
}
template<typename T,typename Reduce>
void buildMips(std::vector<T>& chain,Reduce reduce){
    while(chain.back().size>1){
        const auto& prev=chain.back();
        T next;next.size=prev.size/2;next.p.resize(size_t(next.size)*next.size);
        const uint32_t n=prev.size;
        for(uint32_t y=0;y<next.size;++y)for(uint32_t x=0;x<next.size;++x){
            const size_t at=size_t(2*y)*n+2*x;
            next.p[size_t(y)*next.size+x]=reduce(prev.p[at],prev.p[at+1],
                                                  prev.p[at+n],prev.p[at+n+1]);
        }
        chain.push_back(std::move(next));
    }
}
}
bool generateProceduralPbr(Material& material,const QString& recipe,int resolution,QString& error){
    if(resolution<16||resolution>2048||(resolution&(resolution-1))){
        error=QStringLiteral("PBR resolution must be power of two in [16,2048]");
        return false;
    }
    const QString id=recipe.toLower();
    if(id!="concrete"&&id!="oak"&&id!="grass"&&id!="glass"&&id!="water"&&id!="metal"){
        error=QStringLiteral("Unknown PBR recipe: %1").arg(recipe);return false;
    }
    ColorLevel color;color.size=uint32_t(resolution);color.p.resize(size_t(resolution)*resolution);
    Level normals;normals.size=color.size;normals.p.resize(color.p.size());
    Level rough;rough.size=color.size;rough.p.resize(color.p.size());
    // Seeded, tileable procedural previews. User-supplied PBR image maps will supersede these.
    const float baseR=id=="concrete"?.75f:id=="oak"?.47f:id=="grass"?.22f:id=="glass"?.62f:id=="water"?.10f:.62f;
    const float baseG=id=="concrete"?.74f:id=="oak"?.28f:id=="grass"?.34f:id=="glass"?.77f:id=="water"?.32f:.63f;
    const float baseB=id=="concrete"?.70f:id=="oak"?.13f:id=="grass"?.16f:id=="glass"?.85f:id=="water"?.42f:.65f;
    for(int y=0;y<resolution;++y)for(int x=0;x<resolution;++x){
        const float ny=noise(x,y,17),waves=std::sin(float(x)*.046f+std::sin(float(y)*.013f)*1.2f);
        const float grain=id=="oak"?.13f*waves+.08f*(ny-.5f):
                          id=="concrete"?.13f*(ny-.5f):
                          id=="grass"?.21f*(ny-.5f):0.f;
        const auto r=clampByte((baseR+grain)*255.f);
        const auto g=clampByte((baseG+grain*.75f)*255.f);
        const auto b=clampByte((baseB+grain*.5f)*255.f);
        const size_t index=size_t(y)*resolution+x;
        color.p[index]=(uint32_t(r)<<16)|(uint32_t(g)<<8)|b;
        const float bump=(id=="oak"?.13f:id=="concrete"?.11f:id=="grass"?.22f:0.f);
        const V n=norm({bump*(noise(x+1,y,17)-noise(x-1,y,17)),
                        bump*(noise(x,y+1,17)-noise(x,y-1,17)),1.f});
        normals.p[index]=encodeNormal(n);
        const float baseRough=id=="glass"?.06f:id=="water"?.04f:id=="metal"?.3f:id=="concrete"?.83f:id=="grass"?.95f:.58f;
        rough.p[index]=pack565(clampf(baseRough+.08f*(ny-.5f),0.f,1.f),
                               clampf(baseRough+.08f*(ny-.5f),0.f,1.f),
                               clampf(baseRough+.08f*(ny-.5f),0.f,1.f));
    }
    material.colorMip={std::move(color)};
    material.normalMip={std::move(normals)};
    material.roughnessMip={std::move(rough)};
    buildMips(material.colorMip,downsampleColor);
    buildMips(material.normalMip,downsampleNormal);
    buildMips(material.roughnessMip,downsampleRough);
    return true;
}
}
