#include "../weather/solar_cycle.h"
#include <cassert>
#include <cmath>
int main() {
    using cse::weather::solarMotion;
    using cse::weather::cycleHour;
    const auto dawn=solarMotion(6.f),noon=solarMotion(12.5f),
               dusk=solarMotion(19.f),night=solarMotion(0.f);
    assert(std::abs(dawn.up)<.001f && dawn.daylight<.001f);
    assert(noon.up>.60f && noon.daylight>.99f);
    assert(std::abs(dusk.up)<.001f && dusk.daylight<.001f);
    assert(night.up<-.4f && night.daylight==0.f);
    // Sunrise and sunset must be at DIFFERENT horizons.
    assert(dawn.east*dusk.east+dawn.north*dusk.north<0.0f);
    const auto early=solarMotion(9.f),late=solarMotion(16.f);
    assert(std::hypot(early.east-late.east,early.north-late.north)>.7f);
    // Wrap must be seamless through midnight.
    const auto before=solarMotion(23.999f),after=solarMotion(.001f);
    assert(std::hypot(before.east-after.east,before.north-after.north)<.01f);
    assert(std::abs(before.up-after.up)<.01f);
    // 24 hours must take exactly 360 physical seconds even if rendering skips frames.
    assert(std::abs(cycleHour(14.75,90.0)-20.75)<.0001);
    assert(std::abs(cycleHour(14.75,360.0)-14.75)<.0001);
    assert(std::abs(cycleHour(23.5,15.0)-.5)<.0001);
    assert(std::abs(cycleHour(14.75,0.0)-14.75)<.0001);
    return 0;
}
