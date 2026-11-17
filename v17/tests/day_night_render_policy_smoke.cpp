#include "day_night_render_policy.h"
#include <cassert>
#include <cmath>

int main() {
    using cse::daynight::shadowBucketHour;
    using cse::daynight::shadowStepHours;
    assert(shadowStepHours(4, true) == 1.0f); // DX11 raster
    assert(shadowStepHours(5, true) == 1.0f); // Vulkan raster
    assert(shadowStepHours(4, false) == 0.25f);
    assert(shadowStepHours(5, false) == 0.25f);
    assert(shadowStepHours(2, true) == 0.25f); // Compute stays unchanged
    assert(std::abs(shadowBucketHour(14.75f, 1.0f) - 14.0f) < 1e-6f);
    assert(std::abs(shadowBucketHour(15.25f, 1.0f) - 15.0f) < 1e-6f);
    assert(std::abs(shadowBucketHour(15.25f, 0.25f) - 15.25f) < 1e-6f);
    assert(std::abs(shadowBucketHour(24.0f, 1.0f) - 24.0f) < 1e-6f);
    return 0;
}
