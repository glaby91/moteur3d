#pragma once

#include "m3d_engine.h"

#include <cstdint>
#include <vector>

namespace m3d {

// Packed exactly like LumaTrace's HLSL BVH4 node (56 bytes). Leaf ranges
// point into Scene::order, so geometry and the primitive-order array remain
// owned by V17 and are not copied into a second triangle representation.
struct LumaTraceBvh4Node {
    float minX, minY, minZ;
    std::uint32_t child0;
    float maxX, maxY, maxZ;
    std::uint32_t child1;
    std::uint32_t child2, child3;
    std::uint32_t firstPrimitive, primitiveCount;
    std::uint32_t childCount, padding=0;
};
static_assert(sizeof(LumaTraceBvh4Node)==56,
              "Keep V17 BVH4 packing synchronized with LumaTrace HLSL");

struct LumaTraceBvh4 {
    std::vector<LumaTraceBvh4Node> nodes;
    std::uint32_t primitiveCount=0;
};

// Collapse V17's existing binary hierarchy into four-way nodes. The returned
// leaf ranges index scene.order; the caller must retain that Scene unchanged
// for the lifetime of the GPU buffers. Only triangle-only scenes are accepted
// until the compute shader supports V17's analytic primitive kinds.
bool buildLumaTraceBvh4(const Scene& scene,LumaTraceBvh4& out,
                        std::wstring& error);

} // namespace m3d
