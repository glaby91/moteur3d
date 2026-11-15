// CSE WARP Vulkan incremental resource invalidation policy, no GPU required.
// Tracked content: an explicit geometry revision only invalidates geometry.
// Legacy markGpuContentChanged() is conservatively treated as material+geometry
// to keep old editors and callers fully correct.
#pragma once
#include <cstddef>
#include <cstdint>
namespace m3d { namespace vkdirty {
struct Key {
    const void* address=nullptr;
    uint64_t identity=0, revision=0, geometryRevision=0, materialsRevision=0, parametersRevision=0;
    size_t objects=0, materials=0;
    uint64_t legacyRevision() const {
        return revision-geometryRevision-materialsRevision-parametersRevision; // unsigned wrap
    }
};
enum class Update : uint8_t { None, Geometry, Full };
inline Update classify(const Key& oldKey,const Key& now) {
    if(!oldKey.address||!now.address||now.identity==0||
       oldKey.address!=now.address||oldKey.identity!=now.identity||
       oldKey.materials!=now.materials||
       oldKey.materialsRevision!=now.materialsRevision||
       oldKey.legacyRevision()!=now.legacyRevision())
        return Update::Full;
    if(oldKey.objects!=now.objects||
       oldKey.geometryRevision!=now.geometryRevision)
        return Update::Geometry;
    // gpuParametersRevision changes only need per-frame uniform writes.
    return Update::None;
}
}} // namespace m3d::vkdirty
