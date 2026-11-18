#pragma once
// CSE WARP: stricter version policy than the Vulkan core compatibility requirement.
// Khronos ignores the patch in vkCreateInstance, so enforce it explicitly.
#include <vulkan/vulkan.h>
#include <cstdint>
namespace m3d { namespace vulkanVersion {
inline constexpr uint32_t minimum=VK_MAKE_API_VERSION(0,1,3,204);
static_assert(VK_API_VERSION_MAJOR(minimum)==1 &&
              VK_API_VERSION_MINOR(minimum)==3 &&
              VK_API_VERSION_PATCH(minimum)==204, "Vulkan version encoding");
inline constexpr bool supported(uint32_t actual) noexcept {
    return VK_API_VERSION_VARIANT(actual)==0 && actual>=minimum;
}
}} // namespace m3d::vulkanVersion
