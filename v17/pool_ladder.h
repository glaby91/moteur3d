#pragma once
// V17.440: traverse the TWO real ladders retained by V8, never invented anchors.
// 50 x 25 m basin and ladder anchors match benchmark-civic/generate.cjs.
// Generic walk physics still controls ordinary stairs and dry floor movement.
#include <algorithm>
#include <array>
#include <cmath>

namespace cse::pool_ladder {
struct Point { float x=0.f,y=0.f,z=0.f; };
struct Ladder { float side; float z; };
constexpr std::array<Ladder,2> anchors = {{
    {1.f,-44.05f},{1.f,-19.95f}
}};
constexpr float deckX=25.75f;
constexpr float submergedX=24.60f;
constexpr float deckFeetY=.55f;
constexpr float bottomFeetY=-1.80f;
constexpr float durationSeconds=2.4f;
inline bool inWater(const Point& p) {
    return std::abs(p.x)<24.90f && p.z>-44.35f && p.z<-19.65f
        && p.y<-.65f;
}
inline bool nearby(const Point& p, const Ladder& ladder) {
    const float side=ladder.side;
    if(std::abs(p.z-ladder.z)>1.00f || p.y<-2.15f || p.y>1.35f)
        return false;
    const float x=side*(p.x);
    return x>=24.05f && x<=27.1f;
}
inline int nearest(const Point& p) {
    for(size_t i=0;i<anchors.size();++i)
        if(nearby(p,anchors[i]))return int(i);
    return -1;
}
inline Point path(int index,float fraction,bool up) {
    const Ladder ladder=anchors[size_t(index)];
    // Feet follow an S-shaped trajectory: reach the handrails, pass over
    // the copings, and descend toward the underwater rungs.
    float t=std::clamp(fraction,0.f,1.f);
    if(up)t=1.f-t;
    const float eased=t*t*(3.f-2.f*t);
    const float x=ladder.side*(deckX+(submergedX-deckX)*eased);
    const float y=deckFeetY+(bottomFeetY-deckFeetY)*eased;
    return {x,y,ladder.z};
}
struct Motion {
    int index=-1;
    float progress=0.f;
    bool up=false;
    bool active()const {return index>=0;}
    bool begin(const Point& feet) {
        if(active())return false;
        const int which=nearest(feet);
        if(which<0)return false;
        index=which;
        up=feet.y<-.50f;
        progress=0.f;
        return true;
    }
    Point advance(float dt) {
        if(!active())return {};
        progress=std::min(1.f,progress+std::clamp(dt,0.f,.08f)/durationSeconds);
        const Point result=path(index,progress,up);
        if(progress>=1.f) index=-1;
        return result;
    }
};
inline Point walkInWater(Point feet,float dx,float dz) {
    if(!inWater(feet))return feet;
    // Camera remains at walking eye height above basin bottom; prevents
    // walking through the tiled concrete perimeter while wading.
    feet.x=std::clamp(feet.x+dx,-24.72f,24.72f);
    feet.z=std::clamp(feet.z+dz,-44.24f,-19.76f);
    feet.y=bottomFeetY;
    return feet;
}
}
