#pragma once
// Shared opt-in primary-ray scheduler for CPU and D3D11 Compute.
// The quarter-resolution mode is intentionally conservative: one genuine
// primary ray per 2x2 pixel quad, followed by zero-ray pixel reconstruction.
// No upsampled pixel can increment the primary-ray counter.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace m3d {
inline bool primaryQuarterEnabled() noexcept {
    const char* mode=std::getenv("M3D_PRIMARY_RAYS_25");
    return mode && std::strcmp(mode,"1")==0;
}
inline uint64_t primaryQuarterSampleCount(int w,int h) noexcept {
    if(w<=0||h<=0)return 0;
    return uint64_t((w+1)/2)*uint64_t((h+1)/2);
}
inline void reconstructQuarterPixels(std::vector<uint32_t>& pixels,int w,int h) {
    if(w<=0||h<=0||pixels.size()!=size_t(w)*size_t(h))return;
    // Every donor lies on an even-even coordinate, so in-place expansion is safe.
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        if((x&1)==0&&(y&1)==0)continue;
        pixels[size_t(y)*w+x]=pixels[size_t(y&~1)*w+(x&~1)];
    }
}
}
