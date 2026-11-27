#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
namespace m3d::zmaterial {
inline bool enabled() noexcept {
    const char* v=std::getenv("M3D_V17_ZMATERIAL");
    return !v || std::strcmp(v,"0")!=0;
}
struct Prepared { float metal, diffuse, roughScale; };
inline Prepared prepare(float metal,float roughScale,bool opaque,bool water,bool emissive,bool on) noexcept {
    const float m=on&&opaque&&!water&&!emissive&&std::isfinite(metal)?std::clamp(metal,0.f,1.f):0.f;
    return {m,1.f-m,std::isfinite(roughScale)?std::clamp(roughScale,.01f,2.f):1.f};
}
}
