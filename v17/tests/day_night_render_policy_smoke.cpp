#include "day_night_render_policy.h"
#include <cmath>
#include <iostream>

namespace {
bool near(float actual, float expected) {
    return std::abs(actual - expected) < 1e-6f;
}
}
int main() {
    using cse::daynight::shadowBucketHour;
    using cse::daynight::shadowStepHours;
    const bool ok =
        near(shadowStepHours(4, true), 1.0f) && // DX11 raster
        near(shadowStepHours(5, true), 1.0f) && // Vulkan raster
        near(shadowStepHours(4, false), 0.25f) &&
        near(shadowStepHours(5, false), 0.25f) &&
        near(shadowStepHours(2, true), 0.25f) && // Compute remains unchanged
        near(shadowBucketHour(14.75f, 1.0f), 14.0f) &&
        near(shadowBucketHour(15.25f, 1.0f), 15.0f) &&
        near(shadowBucketHour(15.25f, 0.25f), 15.25f) &&
        near(shadowBucketHour(24.0f, 1.0f), 24.0f);
    if (!ok) {
        std::cerr << "Day/night shadow refresh policy regression\n";
        return 1;
    }
    return 0;
}
