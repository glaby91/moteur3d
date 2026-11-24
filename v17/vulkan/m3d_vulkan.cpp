// CSE WARP V17 Vulkan Raster (experimental independent backend).
// Requires Vulkan API 1.3.204, native GPU raster of the existing Scene and Camera.
// Two paths: legacy offscreen QImage A/B tests, or native Win32 Vulkan swapchain.
// The direct path sends rendered GPU pixels to the window without CPU readback.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif
#include <windows.h>
#endif
#include "m3d_engine.h"
#include "m3d_vulkan_mesh.h"
#include "m3d_vulkan_version.h"
#include "m3d_vulkan_readback.h"
#include "m3d_vulkan_resources.h"
#include "m3d_vulkan_mips.h"
#include "khronos_capture.h"
#include "vulkan_dirty_policy.h"
#include "../weather/clouds.h"
#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_gi_bake.h"
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace m3d { namespace {
using resources::Buffer;
using resources::Texture;
using Clock=std::chrono::steady_clock;
template<class Handle>
Handle vulkanHandleFromBits(uintptr_t bits) noexcept{
    if constexpr(std::is_pointer_v<Handle>)return reinterpret_cast<Handle>(bits);
    else return static_cast<Handle>(bits);
}
using Vertex=vkmesh::GpuVertex;
constexpr uint32_t kMaxMaterials=256, kLights=64;
void vkcheck(VkResult code,const char* name) {
    if(code!=VK_SUCCESS)throw std::runtime_error(std::string(name)+" Vulkan error "+std::to_string(int(code)));
}
struct alignas(16) Uniforms {
    float eye[4]{},forward[4]{},right[4]{},up[4]{},sunDir[4]{},sky[4]{};
    float shadowBase[4]{},shadowRight[4]{},shadowUp[4]{};
    float material[kMaxMaterials][4]{},emission[kMaxMaterials][4]{};
    float lightPos[kLights][4]{},lightRgb[kLights][4]{},misc[4]{};
    float textureDetail[kMaxMaterials][4]{}; // x = protected layer, -1 for auto
    float skyZenith[4]{},skyHorizon[4]{},skyWind[4]{},skyViewport[4]{};
    float giOrigin[4]{},giInvSpan[4]{},giInfo[4]{};
};
static_assert(sizeof(Uniforms)%16==0,"std140 uniform size must be aligned");
struct Chunk {uint32_t first=0,count=0;V lo{},hi{};};
std::vector<uint32_t> spirv(const std::filesystem::path& file){
    std::ifstream in(file,std::ios::binary|std::ios::ate);
    if(!in)throw std::runtime_error("Missing Vulkan SPIR-V shader: "+file.u8string());
    const std::streamoff byteCount=static_cast<std::streamoff>(in.tellg());
    if(byteCount<=0||(byteCount%4)!=0)throw std::runtime_error("Invalid SPIR-V bytes: "+file.u8string());
    std::vector<uint32_t> result(static_cast<size_t>(byteCount)/4u);
    in.seekg(0);in.read(reinterpret_cast<char*>(result.data()),static_cast<std::streamsize>(byteCount));
    if(!in)throw std::runtime_error("Incomplete SPIR-V shader: "+file.u8string());
    return result;
}
std::filesystem::path shaderFile(const char* name){
    std::vector<std::filesystem::path> roots;
#ifdef _WIN32
    wchar_t exe[MAX_PATH]{};
    if(GetModuleFileNameW(nullptr,exe,MAX_PATH)){
        auto p=std::filesystem::path(exe).parent_path();
        roots.push_back(p/"vulkan");
        roots.push_back(p);
    }
#endif
    roots.push_back(std::filesystem::current_path()/"vulkan");
    roots.push_back(std::filesystem::current_path());
    for(const auto& root:roots)if(std::filesystem::exists(root/name))return root/name;
    throw std::runtime_error(std::string("Vulkan shader not installed: ")+name);
}
uint32_t rgba8(uint32_t rgb) {
    // ColorLevel holds 0x00RRGGBB. Vk RGBA8 expects R in least-significant byte.
    return (rgb>>16&255u)|((rgb>>8&255u)<<8)|((rgb&255u)<<16)|0xff000000u;
}
uint32_t unpack565(uint16_t c){
    return ((uint32_t(c>>11&31)*255/31)<<16)|
           ((uint32_t(c>>5&63)*255/63)<<8)|
            (uint32_t(c&31)*255/31);
}
template<typename T>const T* nearestLevel(const std::vector<T>& levels,uint32_t size) {
    if(levels.empty())return nullptr;
    const T* found=&levels.front();
    for(const T& level:levels)
       if(level.size>0 && (found->size==0 || std::abs(int(level.size)-int(size))<std::abs(int(found->size)-int(size))))
           found=&level;
    return found;
}
class VulkanRaster final {
    // Only the direct renderer creates a surface and swapchain. The existing
    // preview/benchmark renderer remains completely offscreen.
    void* presentWindow_=nullptr;
    VkSurfaceKHR displaySurface_=VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_=VK_NULL_HANDLE;
    struct RetiredSwapchain {
        VkSwapchainKHR handle=VK_NULL_HANDLE;
        std::vector<VkSemaphore> presentSemaphores;
    };
    std::vector<RetiredSwapchain> retiredSwapchains_;
    std::vector<VkImage> swapImages_;
    std::vector<VkSemaphore> imageReady_;
    VkSemaphore imageAcquired_=VK_NULL_HANDLE;
    VkFence imageAcquiredFence_=VK_NULL_HANDLE;
    VkExtent2D swapExtent_{};
    VkFormat swapFormat_=VK_FORMAT_UNDEFINED;
    bool swapDirty_=false;
    // Khronos' pre-maintenance1 retirement pattern: old swapchains and their
    // present-wait semaphores survive until the first image from the new chain
    // has itself been presented and reacquired.
    bool retireMarkerPending_=false;
    uint32_t retireMarkerImage_=UINT32_MAX;
    VkPresentModeKHR swapMode_=VK_PRESENT_MODE_FIFO_KHR;
    VkInstance instance_=VK_NULL_HANDLE;
    bool ownsInstance_=true;
    bool ownsSurface_=true;
    VkPhysicalDevice physical_=VK_NULL_HANDLE;
    VkDevice device_=VK_NULL_HANDLE;
    VkQueue queue_=VK_NULL_HANDLE;
    uint32_t family_=0;
    VkPhysicalDeviceMemoryProperties memoryTypes_{};
    VkPhysicalDeviceProperties props_{};
    VkPhysicalDeviceFeatures features_{};
    VkCommandPool commands_=VK_NULL_HANDLE;
    VkCommandBuffer cmd_=VK_NULL_HANDLE;
    VkFence fence_=VK_NULL_HANDLE;
    VkQueryPool queries_=VK_NULL_HANDLE;
    VkRenderPass renderPass_=VK_NULL_HANDLE;
    VkPipelineLayout layout_=VK_NULL_HANDLE;
    VkPipeline opaquePipeline_=VK_NULL_HANDLE,alphaPipeline_=VK_NULL_HANDLE,boxPipeline_=VK_NULL_HANDLE,foliagePipeline_=VK_NULL_HANDLE;
    VkPipeline depthOpaque_=VK_NULL_HANDLE,depthBox_=VK_NULL_HANDLE,depthFoliage_=VK_NULL_HANDLE;
    VkPipeline skyPipeline_=VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_=VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;
    VkDescriptorSet descriptor_=VK_NULL_HANDLE;
    VkSampler albedoSampler_=VK_NULL_HANDLE,protectedSampler_=VK_NULL_HANDLE,shadowSampler_=VK_NULL_HANDLE;
    Texture color_,msColor_,depth_,albedo_,surface_,shadow_,detailAlbedo_,detailSurface_;
    Texture gi0_,gi1_;
    VkSampler giSampler_=VK_NULL_HANDLE;
    gibake::Table giTable_{};
    std::filesystem::path giPath_{};
    uint64_t giSceneHash_=0,giSceneIdentity_=0,giSceneRevision_=~uint64_t(0);
    const Scene* giScenePointer_=nullptr;
    bool giAvailable_=false,gi0Uploaded_=false,gi1Uploaded_=false;
    int giHour0_=-1,giHour1_=-1;
    float giMix_=0.f;
    VkFramebuffer framebuffer_=VK_NULL_HANDLE;
    Buffer vertices_,transparent_,boxTemplate_,boxInstances_,foliageTemplate_,foliageInstances_,uniform_,readback_;
    uint32_t vertexCount_=0,transparentCount_=0,boxCount_=0,foliageCount_=0,foliageTemplateCount_=0;
    std::vector<vkmesh::TransparentTriangle> glass_;
    std::vector<size_t> glassOrder_; // reused across frames; preserves original sort order
    std::vector<Chunk> chunks_,boxChunks_,foliageChunks_;
    std::vector<uint32_t> stagingColors_;
    std::array<int32_t,kMaxMaterials> protectedLayer_{};
    uint32_t width_=0,height_=0;
    uint64_t sceneIdentity_=0,sceneRevision_=~uint64_t(0);
    size_t sceneObjects_=0,sceneMaterials_=0;
    const Scene* scenePointer_=nullptr;
    vkdirty::Key sceneKey_{};
    // Persistent CPU copy and mapped instance buffer for mutable opaque boxes.
    // Identical sizes/materials mean only altered 48-byte instances are written.
    std::vector<vkmesh::BoxInstance> boxCpuCache_;
    std::array<float,4