#pragma once
// V17.444 — FPS scoped optical zoom, pure math independent of render/backend.
// Right mouse hold: zoom 3x; release: recover the exact preset FOV.
// This affects only the camera projection, not its physical position/collisions.
#include <algorithm>
#include <cmath>

namespace cse::scope {
inline constexpr float pi=3.14159265358979323846f;
inline constexpr float magnification=3.f;
inline float targetFov(float baseFov,float power=magnification){
    const float fov=std::clamp(baseFov,20.f,100.f);
    return (360.f/pi)*std::atan(std::tan(fov*pi/360.f)/std::max(power,1.f));
}
inline float effectiveFov(float baseFov,float blend){
    const float a=std::clamp(blend,0.f,1.f);
    if(a==0.f)return baseFov;
    return targetFov(baseFov,1.f+(magnification-1.f)*a);
}
inline float advance(float current,bool held,float elapsedSeconds){
    const float goal=held?1.f:0.f;
    const float dt=std::clamp(elapsedSeconds,0.f,.08f);
    const float speed=held?13.f:10.f;
    const float next=std::clamp(current+(goal-current)*(1.f-std::exp(-speed*dt)),0.f,1.f);
    return std::abs(next-goal)<.001f?goal:next;
}
inline float mouseSensitivity(float blend){
    // Reduced sensitivity during sighting avoids jitter when magnified.
    return 1.f/(1.f+1.5f*std::clamp(blend,0.f,1.f));
}
} // namespace cse::scope
