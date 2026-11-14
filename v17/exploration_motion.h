#pragma once
// V17.429: same exploration speed selection for ground walking and free flight.
// Pure C++17 shared with deterministic walker regression tests.
namespace cse { namespace exploration {
inline constexpr float kNormalMetersPerSecond=2.5f;
inline constexpr float kShiftMultiplier=3.f;
constexpr float speedMetersPerSecond(bool shiftHeld) noexcept {
    return kNormalMetersPerSecond*(shiftHeld?kShiftMultiplier:1.f);
}
static_assert(speedMetersPerSecond(false)==2.5f,"normal speed 2.5m/s");
static_assert(speedMetersPerSecond(true)==7.5f,"Shift exactly 3x speed");
}} // namespace cse::exploration
