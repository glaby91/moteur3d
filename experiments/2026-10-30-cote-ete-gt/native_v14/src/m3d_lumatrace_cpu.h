#pragma once

#include "m3d_engine.h"
#include "m3d_lumatrace_bvh4.h"

#include <cstdint>
#include <vector>

namespace m3d {

// CPU-owned input prepared once per scene revision for the LumaTrace compute
// backend. GPU APIs and resources deliberately do not leak into this module.
struct LumaTraceCpuScene {
    std::vector<std::uint32_t> objectToTriangle;
    std::vector<std::uint32_t> primitiveOrder;
    std::uint32_t triangleCount=0;
    std::uint64_t sceneIdentity=0;
    std::uint64_t sceneRevision=0;
};

// Build and validate all scene-side data that LumaTrace needs before dispatch.
// Transparent objects keep the sentinel used by the compute traversal.
bool prepareLumaTraceCpuScene(const Scene& scene,const LumaTraceBvh4& bvh,
                              LumaTraceCpuScene& out,std::wstring& error);

} // namespace m3d
