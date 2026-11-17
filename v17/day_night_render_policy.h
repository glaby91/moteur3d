#pragma once
// Stabilizes expensive CPU shadow-map rebuilds during raster day/night playback.
// This changes only the refresh cadence; sun and local-light values remain untouched.
#include <cmath>

namespace cse::daynight {
inline float shadowStepHours(int backendIndex, bool dayNightCycle) noexcept {
    // Backend indices 4/5 are DX11/Vulkan raster. Keep both on the same one-hour
    // cache buckets during the accelerated 24h/6min simulation.
    return dayNightCycle && (backendIndex == 4 || backendIndex == 5) ? 1.0f : 0.25f;
}
inline float shadowBucketHour(float hour, float stepHours) noexcept {
    if (!std::isfinite(hour) || !std::isfinite(stepHours) || stepHours <= 0.0f)
        return hour;
    return std::floor(hour / stepHours) * stepHours;
}
}
