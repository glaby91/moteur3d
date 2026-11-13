#pragma once
// Frame presentation diagnostics. No dependence on Qt, GPU or audio.
// The metric is time BETWEEN frames accepted by the UI, not FPS reported by
// the DirectX render call. Counters reset per exploration session.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace cse::pacing {
inline bool accept(bool sceneLoading, bool sceneChanged, bool validFrame) {
    // Camera movement alone must never invalidate a completed full image.
    // Scene change and a failed output remain strict rejection conditions.
    return !sceneLoading && !sceneChanged && validFrame;
}
struct Sample {
    int64_t timestampMs = 0;
    double intervalMs = 0;
    double renderMs = 0;
    double cameraLagMs = 0;
    unsigned pendingRequests = 0;
};
struct Summary {
    size_t frames = 0;
    double medianPresentMs = 0;
    double p95PresentMs = 0;
    double p99PresentMs = 0;
    double meanPresentMs = 0;
    double worstPresentMs = 0;
    double medianRenderMs = 0;
    double stutterPercent = 0;
    uint64_t invalidated = 0;
    uint64_t queued = 0;
};
inline double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double position = std::clamp(fraction,0.0,1.0) * double(values.size()-1);
    const auto index = static_cast<size_t>(position);
    const double ratio = position - double(index);
    if (index+1>=values.size()) return values[index];
    return values[index] * (1.0-ratio) + values[index+1] * ratio;
}
inline Summary summarize(const std::vector<Sample>& samples,
                         uint64_t invalidated, uint64_t queued) {
    Summary result{};
    result.invalidated = invalidated;
    result.queued = queued;
    result.frames = samples.size();
    if (samples.empty()) return result;
    std::vector<double> intervals;
    std::vector<double> rendering;
    intervals.reserve(samples.size());
    rendering.reserve(samples.size());
    double sum=0.0;
    for (const auto& sample:samples) {
        rendering.push_back(sample.renderMs);
        // First image has no prior present interval and must NOT enter pacing stats.
        if (sample.intervalMs > 0.0 && std::isfinite(sample.intervalMs)) {
            intervals.push_back(sample.intervalMs);
            sum += sample.intervalMs;
        }
    }
    result.medianRenderMs=percentile(rendering,.5);
    if (intervals.empty()) return result;
    result.meanPresentMs=sum/double(intervals.size());
    result.medianPresentMs=percentile(intervals,.5);
    result.p95PresentMs=percentile(intervals,.95);
    result.p99PresentMs=percentile(intervals,.99);
    result.worstPresentMs=*std::max_element(intervals.begin(),intervals.end());
    // Explicit threshold: frames slower than max(33.3 ms, 2x the median).
    const double threshold=std::max(33.3,2.0*result.medianPresentMs);
    result.stutterPercent=100.0*double(std::count_if(intervals.begin(),intervals.end(),
        [threshold](double value){return value>threshold;}))/double(intervals.size());
    return result;
}
}
