#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace m3d::atmosphere {
inline float density() noexcept {
    const char* on=std::getenv("M3D_V17_ZATMOSPHERE");
    if(!on||std::strcmp(on,"1")!=0)return 0.f;
    const char* value=std::getenv("M3D_V17_ZATMOSPHERE_DENSITY");
    if(!value)return .012f;
    char* end=nullptr;float v=std::strtof(value,&end);
    return end!=value&&*end=='\0'&&std::isfinite(v)?std::clamp(v,0.f,.04f):.012f;
}
constexpr float maxDistance=28.f;
constexpr unsigned lightLimit=4;
struct alignas(16) Lights {float pos[lightLimit][4]{},rgb[lightLimit][4]{};};
template<class Scene,class Camera>
inline Lights selectLights(const Scene& scene,const Camera& cam,float day,float exteriorFactor) {
    Lights out;std::array<float,lightLimit> score{};
    for(const auto& l:scene.lights){
        float energy=l.intensity*(l.exterior?exteriorFactor:(.42f+.58f*(1.f-day)));
        if(!(energy>0.f&&l.radius>0.f))continue;
        auto q=l.p-cam.eye;
        float t=std::clamp(q.x*cam.f.x+q.y*cam.f.y+q.z*cam.f.z,0.f,maxDistance);
        auto d=q-cam.f*t;
        float dist=std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
        float v=std::max(0.f,dist-t*std::max(.0f,cam.tanHalf));
        float importance=energy*std::max(0.f,1.f-v/l.radius)/(1.f+.08f*t);
        if(!(importance>0))continue;
        unsigned at=0;while(at<lightLimit&&score[at]>=importance)++at;
        if(at==lightLimit)continue;
        for(unsigned j=lightLimit-1;j>at;--j){
            score[j]=score[j-1];
            for(int k=0;k<4;++k){out.pos[j][k]=out.pos[j-1][k];out.rgb[j][k]=out.rgb[j-1][k];}
        }
        score[at]=importance;out.pos[at][0]=l.p.x;out.pos[at][1]=l.p.y;out.pos[at][2]=l.p.z;out.pos[at][3]=l.radius;
        out.rgb[at][0]=l.color.x/255.f;out.rgb[at][1]=l.color.y/255.f;out.rgb[at][2]=l.color.z/255.f;out.rgb[at][3]=energy;
    }
    return out;
}
}
