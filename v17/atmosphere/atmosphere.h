#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace m3d::atmosphere {
// 002 : gain de grain uniquement, jamais coefficient d'extinction.
constexpr float defaultDensity=.002f, maxDensity=.0066f;
constexpr float maxDistance=6.75f, maxLinear=.00077f;
constexpr unsigned lightLimit=4;
inline float density() noexcept {
    const char* on=std::getenv("M3D_V17_ZATMOSPHERE");
    if(!on||std::strcmp(on,"1")!=0)return 0.f;
    const char* value=std::getenv("M3D_V17_ZATMOSPHERE_DENSITY");
    if(!value)return defaultDensity;
    char* end=nullptr;float v=std::strtof(value,&end);
    return end!=value&&*end=='\0'&&std::isfinite(v)?std::clamp(v,0.f,maxDensity):defaultDensity;
}
inline float smooth(float x) noexcept {x=std::clamp(x,0.f,1.f);return x*x*(3.f-2.f*x);}
struct alignas(16) Lights {float pos[lightLimit][4]{},rgb[lightLimit][4]{};};
// Classement independant de l'orientation; la cinquieme source assure une
// transition continue quand deux candidats permutent au seuil de selection.
template<class Scene,class Camera>
inline Lights selectLights(const Scene& scene,const Camera& cam,float day,float exteriorFactor) {
    Lights out;std::array<float,lightLimit+1> score{};
    for(const auto& l:scene.lights){
        float energy=l.intensity*(l.exterior?exteriorFactor:(.42f+.58f*(1.f-day)));
        if(!(energy>0.f&&l.radius>0.f))continue;
        auto q=l.p-cam.eye;
        float dist=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z);
        float importance=energy*std::max(0.f,1.f-std::max(0.f,dist-6.5f)/l.radius)/(1.f+.08f*dist);
        if(!(importance>0))continue;
        unsigned at=0;while(at<=lightLimit&&score[at]>=importance)++at;
        if(at>lightLimit)continue;
        for(unsigned j=lightLimit;j>at;--j){
            score[j]=score[j-1];
            if(j<lightLimit)for(int k=0;k<4;++k){out.pos[j][k]=out.pos[j-1][k];out.rgb[j][k]=out.rgb[j-1][k];}
        }
        score[at]=importance;
        if(at==lightLimit)continue;
        out.pos[at][0]=l.p.x;out.pos[at][1]=l.p.y;out.pos[at][2]=l.p.z;out.pos[at][3]=l.radius;
        out.rgb[at][0]=l.color.x/255.f;out.rgb[at][1]=l.color.y/255.f;out.rgb[at][2]=l.color.z/255.f;out.rgb[at][3]=energy;
    }
    for(unsigned i=0;i<lightLimit;++i)
        out.rgb[i][3]*=smooth((score[i]-score[lightLimit])/std::max(.00001f,.15f*score[i]));
    return out;
}
}
