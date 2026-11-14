#pragma once
// CSE WARP: stable 24-hour solar path and monotonic simulation clock.
// Sun rises at 06:00, reaches its zenith at 12:30 and sets at 19:00.
// Horizontal bearing traverses 360 degrees per 24h; the sun is below
// the horizon all night (unlike the previous constant-altitude shortcut).
#include <algorithm>
#include <cmath>
namespace cse::weather {
struct SolarMotion {
    float east=0.f, up=0.f, north=0.f, daylight=0.f;
};
inline double cycleHour(double startHour,double elapsedSeconds,double cycleSeconds=360.0){
    if(!(cycleSeconds>0.0))return startHour;
    double hour=std::fmod(startHour+elapsedSeconds*24.0/cycleSeconds,24.0);
    return hour<0.0?hour+24.0:hour;
}
inline SolarMotion solarMotion(float hour){
    constexpr float pi=3.14159265358979323846f;
    constexpr float sunrise=6.0f,sunset=19.0f;
    const float h=float(cycleHour(hour,0.0));
    float altitude=0.f,daylight=0.f;
    if(h>=sunrise && h<sunset){
        const float phase=(h-sunrise)/(sunset-sunrise);
        daylight=std::max(0.f,std::sin(pi*phase));
        altitude=0.64f*daylight;
    }else{
        // Night occupies the remaining 11h; continuous at 00:00 and 24:00.
        const float nightPhase=(h>=sunset?h-sunset:h+24.f-sunset)
                               /(24.f-sunset+sunrise);
        altitude=-.58f*std::sin(pi*nightPhase);
    }
    // Reference noon orientation retained so façades keep their visual identity.
    // Increase sweep from 6.9 degrees/hour to the correct full 15 degrees/hour.
    const float azimuth=(h-14.75f)*(2.f*pi/24.f);
    const float c=std::cos(azimuth),s=std::sin(azimuth);
    const float east=.70f*c+.714f*s,north=.70f*s-.714f*c;
    const float norm=std::hypot(east,north);
    const float horizon=std::sqrt(std::max(0.f,1.f-altitude*altitude));
    return {east/norm*horizon,altitude,north/norm*horizon,daylight};
}
}
