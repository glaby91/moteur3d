#pragma once
#include "radio_audio.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace cse::linux_audio {
struct Vec {float x,y,z;};
inline Vec vec(const float p[3]) {return {p[0],p[1],p[2]};}
inline Vec sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float norm(Vec a){return std::sqrt(dot(a,a));}
inline Vec unit(Vec a,Vec fallback={0,0,1}){
    const float n=norm(a);
    if(!(n>1e-5f)||!std::isfinite(n))return fallback;
    return {a.x/n,a.y/n,a.z/n};
}
struct Stereo {float left=0,right=0;};
inline Stereo gains(const std::array<RadioSpeaker,4>& speakers,
                    const float eye[3],const float target[3]){
    const Vec listener=vec(eye),forward=unit(sub(vec(target),listener));
    // Same handedness as makeCamera: right = cross(forward, world up).
    const Vec right=unit({-forward.z,0,forward.x},{-1,0,0});
    Stereo mix;
    constexpr float inner=0.84339145f; // cos(65 degrees / 2)
    constexpr float outer=0.46174861f; // cos(125 degrees / 2)
    for(const auto& speaker:speakers){
        const Vec delta=sub(vec(speaker.position),listener);
        const float d=norm(delta);
        if(!std::isfinite(d))continue;
        const Vec direction=unit(delta,forward);
        const float facing=d<1e-5f?1.f:dot(unit(vec(speaker.front)),
            {-direction.x,-direction.y,-direction.z});
        const float cone=.2f+.8f*std::clamp((facing-outer)/(inner-outer),0.f,1.f);
        const float scaled=d/20.f;
        const float attenuation=cone/(1.f+scaled*scaled);
        const float pan=std::clamp(dot(right,direction),-1.f,1.f);
        mix.left+=attenuation*std::sqrt((1.f-pan)*.5f);
        mix.right+=attenuation*std::sqrt((1.f+pan)*.5f);
    }
    // Four coherent copies of the same signal must not clip. Preserve balance.
    const float scale=std::max({1.f,mix.left,mix.right});
    return {mix.left/scale,mix.right/scale};
}
}
