#pragma once
// CSE WARP weather: deterministic fair-weather cirrus layer.
// This module is rendering-backend independent. No volumetric simulation.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace cse::weather {
struct CloudState {
    float coverage=0.075f;
    float windEastMetersPerSecond=0.7f;
    float windNorthMetersPerSecond=0.25f;
    float altitudeMeters=7800.f;
    float opacity=0.24f;
};
struct CloudFrame {
    float uOffset=0.f, vOffset=0.f;
    float coverage=0.f, opacity=0.f;
    float daylight=0.f;
};
inline CloudFrame cirrusAt(const CloudState& state,double elapsedSeconds,
                          float sunAltitudeDeg,float domeRadiusMeters=12000.f) {
    const double circumference=6.283185307179586*std::max(1.f,domeRadiusMeters);
    const auto repeat=[](double x)->float {
        x=std::fmod(x,1.0); return float(x<0?x+1:x);
    };
    const float daylight=std::clamp((sunAltitudeDeg+8.f)/16.f,0.f,1.f);
    return {repeat(state.windEastMetersPerSecond*elapsedSeconds/circumference),
            repeat(state.windNorthMetersPerSecond*elapsedSeconds/circumference),
            std::clamp(state.coverage,0.f,1.f),
            std::clamp(state.opacity,0.f,1.f),
            daylight};
}
}
