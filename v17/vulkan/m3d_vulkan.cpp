// CSE WARP V17 Vulkan Raster (experimental independent backend).
// Requires Vulkan API 1.3.204, native GPU raster of the existing Scene and Camera.
// Offscreen color is read back for the current Qt QImage host. No swapchain.
#include "m3d_engine.h"
#include "m3d_vulkan_mesh.h"
#include "m3d_vulkan_version.h"
#include "m3d_vulkan_readback.h"
#include "m3d_vulkan_mips.h"
#include "../weather/clouds.h"
#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_gi_bake.h"
#include <vulkan/vulkan.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace m3d { namespace {
using Clock=std::chrono::steady_clock;
using Vertex=vkmesh::GpuVertex;
constexpr uint32_t kMaxMaterials=256, kLights=16;
void vkcheck(VkResult code,const char* name) {
    if(code!=VK_SUCCESS)throw std::runtime_error(std::string(name)+" Vulkan error "+std::to_string(int(code)));
}
struct Buffer {
    VkBuffer handle=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    void* mapped=nullptr;
    VkDeviceSize size=0,allocation=0;
    VkMemoryPropertyFlags flags=0;
};
struct Texture {
    VkImage image=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    VkImageView view=VK_NULL_HANDLE;
    VkFormat format=VK_FORMAT_UNDEFINED;
    uint32_t width=0,height=0,depth=1,layers=0,mips=1;
};
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
    VkInstance instance_=VK_NULL_HANDLE;
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
    const float* shadowData_=nullptr;
    uint32_t shadowSize_=0;
    float cachedSolarHour_=std::numeric_limits<float>::quiet_NaN();
    std::string adapter_;
    bool bgraReadback_=false;
    bool srgbColorEnabled_=false;
    VkFormat colorFormat_=VK_FORMAT_R8G8B8A8_UNORM;
    bool started_=false;
    uint32_t msaaSetting_=2;
    VkSampleCountFlagBits msaaSamples_=VK_SAMPLE_COUNT_1_BIT;
    VkFormat depthFormat_=VK_FORMAT_D32_SFLOAT;
    static double ms(Clock::time_point start,Clock::time_point end) {
        return std::chrono::duration<double,std::milli>(end-start).count();
    }
    uint32_t type(uint32_t bits,VkMemoryPropertyFlags required,bool cache=false)const {
        int best=-1;uint32_t candidate=UINT32_MAX;
        for(uint32_t i=0;i<memoryTypes_.memoryTypeCount;i++) {
            if((bits & (1u<<i))==0)continue;
            auto flags=memoryTypes_.memoryTypes[i].propertyFlags;
            if((flags&required)!=required)continue;
            int score=(cache&&(flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)?64:0)
                +(flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT?8:0)
                +(flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT?4:0);
            if(score>best){best=score;candidate=i;}
        }
        if(candidate==UINT32_MAX)throw std::runtime_error("No suitable Vulkan memory type");
        return candidate;
    }
    Buffer buffer(VkDeviceSize bytes,VkBufferUsageFlags usage,VkMemoryPropertyFlags flags,bool preferCached=false){
        Buffer b;b.size=bytes;
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size=bytes;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkcheck(vkCreateBuffer(device_,&ci,nullptr,&b.handle),"create buffer");
        VkMemoryRequirements req{};vkGetBufferMemoryRequirements(device_,b.handle,&req);
        const uint32_t index=type(req.memoryTypeBits,flags,preferCached);
        b.flags=memoryTypes_.memoryTypes[index].propertyFlags;
        b.allocation=req.size;
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        a.allocationSize=req.size;a.memoryTypeIndex=index;
        vkcheck(vkAllocateMemory(device_,&a,nullptr,&b.memory),"allocate buffer");
        vkcheck(vkBindBufferMemory(device_,b.handle,b.memory,0),"bind buffer");
        if(b.flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            vkcheck(vkMapMemory(device_,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped),"map persistent buffer");
        return b;
    }
    void flush(Buffer& b){
        if((b.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)||!b.mapped)return;
        VkMappedMemoryRange mr{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        mr.memory=b.memory;mr.size=VK_WHOLE_SIZE;
        vkcheck(vkFlushMappedMemoryRanges(device_,1,&mr),"flush mapped buffer");
    }
    void invalidate(Buffer& b){
        if((b.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)||!b.mapped)return;
        VkMappedMemoryRange mr{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        mr.memory=b.memory;mr.size=VK_WHOLE_SIZE;
        vkcheck(vkInvalidateMappedMemoryRanges(device_,1,&mr),"invalidate mapped buffer");
    }
    void free(Buffer& b){
        if(b.mapped)vkUnmapMemory(device_,b.memory);
        if(b.handle)vkDestroyBuffer(device_,b.handle,nullptr);
        if(b.memory)vkFreeMemory(device_,b.memory,nullptr);
        b={};
    }
    Texture image(uint32_t w,uint32_t h,uint32_t layers,VkFormat format,
                  VkImageUsageFlags usage,VkImageAspectFlags aspect,bool forceArrayView=false,uint32_t mipLevels=1,VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT){
        Texture t;t.width=w;t.height=h;t.layers=layers;t.format=format;t.mips=mipLevels;
        if(!mipLevels||mipLevels>vkmips::levelCount(std::max(w,h)))
            throw std::runtime_error("Invalid Vulkan texture mip count");
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={w,h,1};
        ci.mipLevels=mipLevels;ci.arrayLayers=layers;ci.samples=samples;
        ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkcheck(vkCreateImage(device_,&ci,nullptr,&t.image),"create image");
        VkMemoryRequirements req{};vkGetImageMemoryRequirements(device_,t.image,&req);
        VkMemoryAllocateInfo mem{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mem.allocationSize=req.size;mem.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkcheck(vkAllocateMemory(device_,&mem,nullptr,&t.memory),"allocate image");
        vkcheck(vkBindImageMemory(device_,t.image,t.memory,0),"bind image");
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=t.image;vi.viewType=(forceArrayView||layers>1)?VK_IMAGE_VIEW_TYPE_2D_ARRAY:VK_IMAGE_VIEW_TYPE_2D;
        vi.format=format;vi.subresourceRange.aspectMask=aspect;
        vi.subresourceRange.levelCount=mipLevels;vi.subresourceRange.layerCount=layers;
        vkcheck(vkCreateImageView(device_,&vi,nullptr,&t.view),"create view");
        return t;
    }

    Texture image3D(uint32_t w,uint32_t h,uint32_t d){
        Texture t;t.width=w;t.height=h;t.depth=d;t.layers=1;t.mips=1;
        t.format=VK_FORMAT_R32G32B32A32_SFLOAT;
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType=VK_IMAGE_TYPE_3D;ci.format=t.format;ci.extent={w,h,d};
        ci.mipLevels=1;ci.arrayLayers=1;ci.samples=VK_SAMPLE_COUNT_1_BIT;
        ci.tiling=VK_IMAGE_TILING_OPTIMAL;
        ci.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        vkcheck(vkCreateImage(device_,&ci,nullptr,&t.image),"GI 3D image");
        VkMemoryRequirements req{};vkGetImageMemoryRequirements(device_,t.image,&req);
        VkMemoryAllocateInfo mem{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mem.allocationSize=req.size;
        mem.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkcheck(vkAllocateMemory(device_,&mem,nullptr,&t.memory),"GI 3D memory");
        vkcheck(vkBindImageMemory(device_,t.image,t.memory,0),"GI 3D bind");
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image=t.image;vi.viewType=VK_IMAGE_VIEW_TYPE_3D;vi.format=t.format;
        vi.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount=1;vi.subresourceRange.layerCount=1;
        vkcheck(vkCreateImageView(device_,&vi,nullptr,&t.view),"GI 3D view");
        return t;
    }
    void uploadProbe(Texture& t,const float* data,size_t bytes,bool uploaded){
        Buffer stage=buffer(VkDeviceSize(bytes),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        memcpy(stage.mapped,data,bytes);flush(stage);
        begin();
        barrier(t,uploaded?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                uploaded?VK_ACCESS_SHADER_READ_BIT:0,VK_ACCESS_TRANSFER_WRITE_BIT,
                uploaded?VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount=1;
        region.imageExtent={t.width,t.height,t.depth};
        vkCmdCopyBufferToImage(cmd_,stage.handle,t.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
        barrier(t,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        submit();free(stage);
    }
    void ensureGI(const Scene& scene){
        giAvailable_=false;
        if(!rasterHybridLightingEnabled())return;
        const auto path=rasterGiCachePath();
        if(path.empty())return;
        const bool changed=path!=giPath_||giScenePointer_!=&scene||
            giSceneIdentity_!=scene.gpuCacheIdentity||
            giSceneRevision_!=scene.gpuCacheRevision||scene.gpuCacheIdentity==0;
        if(changed){
            giScenePointer_=&scene;giSceneIdentity_=scene.gpuCacheIdentity;
            giSceneRevision_=scene.gpuCacheRevision;
            const uint64_t hash=gibake::signature(scene);
            giPath_=path;giSceneHash_=hash;
            giTable_=gibake::Table{};
            gibake::Table candidate;std::string reason;
            if(gibake::readFile(path,candidate,reason)&&candidate.valid()&&
                candidate.sceneHash==hash&&candidate.nx<=128&&
                candidate.ny<=128&&candidate.nz<=128){
                VkFormatProperties p{};
                vkGetPhysicalDeviceFormatProperties(physical_,VK_FORMAT_R32G32B32A32_SFLOAT,&p);
                constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|
                    VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
                if((p.optimalTilingFeatures&required)==required)
                    giTable_=std::move(candidate);
                else std::clog<<"[CSE WARP Vulkan] GPU cannot linearly filter GI 3D float textures\n";
            }else std::clog<<"[CSE WARP Vulkan] GI cache absent, invalid or different scene\n";
            giHour0_=giHour1_=-1;
            if(giTable_.valid()&&(gi0_.width!=giTable_.nx||gi0_.height!=giTable_.ny||
                                   gi0_.depth!=giTable_.nz)){
                free(gi0_);free(gi1_);
                gi0_=image3D(giTable_.nx,giTable_.ny,giTable_.nz);
                gi1_=image3D(giTable_.nx,giTable_.ny,giTable_.nz);
                gi0Uploaded_=gi1Uploaded_=false;
            }
        }
        if(!giTable_.valid())return;
        unsigned h0=0,h1=0;float mix=0;
        gibake::hourIndices(giTable_,solarHour,h0,h1,mix);
        const size_t frame=giTable_.voxelCount()*4;
        if(giHour0_!=int(h0)){
            uploadProbe(gi0_,giTable_.rgba.data()+size_t(h0)*frame,
                        frame*sizeof(float),gi0Uploaded_);
            gi0Uploaded_=true;giHour0_=int(h0);
        }
        if(giHour1_!=int(h1)){
            uploadProbe(gi1_,giTable_.rgba.data()+size_t(h1)*frame,
                        frame*sizeof(float),gi1Uploaded_);
            gi1Uploaded_=true;giHour1_=int(h1);
        }
        giMix_=mix;giAvailable_=true;updateDescriptors();
    }
    void free(Texture& t){
        if(t.view)vkDestroyImageView(device_,t.view,nullptr);
        if(t.image)vkDestroyImage(device_,t.image,nullptr);
        if(t.memory)vkFreeMemory(device_,t.memory,nullptr);
        t={};
    }
    void begin(){
        vkcheck(vkResetCommandPool(device_,commands_,0),"reset commands");
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkcheck(vkBeginCommandBuffer(cmd_,&bi),"begin commands");
    }
    void submit(double* submissionMs=nullptr,double* waitMs=nullptr){
        vkcheck(vkEndCommandBuffer(cmd_),"end commands");
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount=1;si.pCommandBuffers=&cmd_;
        const auto start=Clock::now();
        vkcheck(vkQueueSubmit(queue_,1,&si,fence_),"submit commands");
        const auto issued=Clock::now();
        vkcheck(vkWaitForFences(device_,1,&fence_,VK_TRUE,UINT64_MAX),"wait GPU");
        const auto completed=Clock::now();
        vkcheck(vkResetFences(device_,1,&fence_),"reset fence");
        if(submissionMs)*submissionMs=ms(start,issued);
        if(waitMs)*waitMs=ms(issued,completed);
    }
    void barrier(Texture& t,VkImageLayout from,VkImageLayout to,
                 VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags srcStage,VkPipelineStageFlags dstStage){
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout=from;b.newLayout=to;
        b.srcAccessMask=src;b.dstAccessMask=dst;
        b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.image=t.image;b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount=t.mips;b.subresourceRange.layerCount=t.layers;
        vkCmdPipelineBarrier(cmd_,srcStage,dstStage,0,0,nullptr,0,nullptr,1,&b);
    }
    void uploadImage(Texture& tex,const void* src,size_t bytes){
        Buffer stage=buffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        std::memcpy(stage.mapped,src,bytes);flush(stage);
        begin();
        barrier(tex,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount=tex.layers;
        region.imageExtent={tex.width,tex.height,1};
        vkCmdCopyBufferToImage(cmd_,stage.handle,tex.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
        if(tex.mips>1){
            // Generate all layers once at texture upload. Never generate mips per frame.
            auto moveLevel=[&](uint32_t level,VkImageLayout oldLayout,
                 VkImageLayout newLayout,VkAccessFlags src,VkAccessFlags dst,
                 VkPipelineStageFlags srcStage,VkPipelineStageFlags dstStage){
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                b.oldLayout=oldLayout;b.newLayout=newLayout;
                b.srcAccessMask=src;b.dstAccessMask=dst;
                b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
                b.image=tex.image;b.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
                b.subresourceRange.baseMipLevel=level;b.subresourceRange.levelCount=1;
                b.subresourceRange.layerCount=tex.layers;
                vkCmdPipelineBarrier(cmd_,srcStage,dstStage,0,0,nullptr,0,nullptr,1,&b);
            };
            for(uint32_t i=1;i<tex.mips;i++){
                moveLevel(i-1,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
                VkImageBlit b{};
                b.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
                b.srcSubresource.mipLevel=i-1;b.srcSubresource.layerCount=tex.layers;
                b.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
                b.dstSubresource.mipLevel=i;b.dstSubresource.layerCount=tex.layers;
                b.srcOffsets[1]={int32_t(vkmips::extent(tex.width,i-1)),
                                 int32_t(vkmips::extent(tex.height,i-1)),1};
                b.dstOffsets[1]={int32_t(vkmips::extent(tex.width,i)),
                                 int32_t(vkmips::extent(tex.height,i)),1};
                vkCmdBlitImage(cmd_,tex.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               tex.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&b,VK_FILTER_LINEAR);
            }
            for(uint32_t i=0;i<tex.mips;i++){
                const bool final=i+1==tex.mips;
                moveLevel(i,final?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    final?VK_ACCESS_TRANSFER_WRITE_BIT:VK_ACCESS_TRANSFER_READ_BIT,
                    VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            }
        }else{
            barrier(tex,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        }
        submit();free(stage);
    }
    Buffer uploadVertices(const void* src,size_t bytes){
        Buffer vertex=buffer(bytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        Buffer stage=buffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        memcpy(stage.mapped,src,bytes);flush(stage);
        begin();VkBufferCopy bc{};bc.size=bytes;
        vkCmdCopyBuffer(cmd_,stage.handle,vertex.handle,1,&bc);
        VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
        b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        b.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        b.buffer=vertex.handle;b.size=VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(cmd_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,0,nullptr,1,&b,0,nullptr);
        submit();free(stage);return vertex;
    }
    void updateDescriptors(){
        if(!uniform_.handle||!albedo_.view||!surface_.view||!shadow_.view||!gi0_.view||!gi1_.view)return;
        VkDescriptorBufferInfo u{uniform_.handle,0,sizeof(Uniforms)};
        VkDescriptorImageInfo a{albedoSampler_,albedo_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo n{albedoSampler_,surface_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo s{shadowSampler_,shadow_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo highAlbedo{protectedSampler_,detailAlbedo_.view?detailAlbedo_.view:albedo_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo highSurface{protectedSampler_,detailSurface_.view?detailSurface_.view:surface_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo giLow{giSampler_,gi0_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo giHigh{giSampler_,gi1_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet,8> writes{};
        for(uint32_t i=0;i<8;i++){
            auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=descriptor_;w.dstBinding=i;w.descriptorCount=1;
            w.descriptorType=i?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        writes[0].pBufferInfo=&u;writes[1].pImageInfo=&a;writes[2].pImageInfo=&n;writes[3].pImageInfo=&s;
        writes[4].pImageInfo=&highAlbedo;writes[5].pImageInfo=&highSurface;
        writes[6].pImageInfo=&giLow;writes[7].pImageInfo=&giHigh;
        vkUpdateDescriptorSets(device_,uint32_t(writes.size()),writes.data(),0,nullptr);
    }
    // Pipeline cache is driver-specific but NEVER scene-specific. Cache is
    // optional and never allowed to change the shader/pipeline parameters.
    std::filesystem::path pipelineCacheFile() const {
#ifdef _WIN32
        wchar_t dir[32768]{};
        const DWORD len=GetEnvironmentVariableW(L"LOCALAPPDATA",dir,32768);
        if(len==0 || len>=32768)return {};
        static constexpr char hex[]="0123456789abcdef";
        std::string name=std::to_string(props_.vendorID)+"-"+
            std::to_string(props_.deviceID)+"-"+
            std::to_string(props_.driverVersion)+"-";
        for(uint8_t b:props_.pipelineCacheUUID){
            name.push_back(hex[b>>4]);name.push_back(hex[b&15]);
        }
        return std::filesystem::path(dir)/L"CSE-WARP"/L"VulkanCache"/(name+".bin");
#else
        return {};
#endif
    }
    std::vector<uint8_t> readPipelineCache() const noexcept {
        try{
            const auto file=pipelineCacheFile();
            if(file.empty())return {};
            std::ifstream in(file,std::ios::binary|std::ios::ate);
            if(!in)return {};
            const auto n=in.tellg();
            if(n<32 || n>32*1024*1024)return {};
            std::vector<uint8_t> bytes(size_t(n),0);
            in.seekg(0);
            in.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size()));
            if(!in)return {};
            uint32_t h[4]{};
            std::memcpy(h,bytes.data(),sizeof(h));
            if(h[0]<32 || h[0]>bytes.size() ||
               h[1]!=uint32_t(VK_PIPELINE_CACHE_HEADER_VERSION_ONE) ||
               h[2]!=props_.vendorID || h[3]!=props_.deviceID ||
               std::memcmp(bytes.data()+16,props_.pipelineCacheUUID,VK_UUID_SIZE)!=0)
                return {};
            return bytes;
        }catch(...){return {};}
    }
    void writePipelineCache() const noexcept {
        if(!pipelineCache_)return;
        try{
            size_t n=0;
            if(vkGetPipelineCacheData(device_,pipelineCache_,&n,nullptr)!=VK_SUCCESS ||
               n<32 || n>32*1024*1024)return;
            std::vector<uint8_t> bytes(n);
            if(vkGetPipelineCacheData(device_,pipelineCache_,&n,bytes.data())!=VK_SUCCESS)
                return;
            bytes.resize(n);
            const auto target=pipelineCacheFile();
            if(target.empty())return;
            std::error_code ec;
            std::filesystem::create_directories(target.parent_path(),ec);
            if(ec)return;
#ifdef _WIN32
            auto temporary=target;
            temporary+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
#else
            auto temporary=target;
            temporary+=".tmp";
#endif
            {
                std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
                if(!out)return;
                out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
                if(!out)return;
            }
            std::filesystem::rename(temporary,target,ec);
            if(ec){
                std::filesystem::remove(target,ec);
                ec.clear();
                std::filesystem::rename(temporary,target,ec);
            }
            if(ec)std::filesystem::remove(temporary,ec);
        }catch(...){/* A failed cache write must never affect rendering. */}
    }
    void configure(){
        // Explicit minimum selected for this experiment: Vulkan API 1.3.204.
        // The installed SDK/loader and physical device versions are independent.
        constexpr uint32_t kMinimumVulkanVersion=vulkanVersion::minimum;
        uint32_t instanceVersion=VK_API_VERSION_1_0;
        auto enumerateVersion=reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(VK_NULL_HANDLE,"vkEnumerateInstanceVersion"));
        if(enumerateVersion)
            vkcheck(enumerateVersion(&instanceVersion),"query Vulkan loader version");
        if(!vulkanVersion::supported(instanceVersion))
            throw std::runtime_error("Vulkan loader below required API 1.3.204; update Vulkan runtime / Intel graphics driver");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName="CSE WARP V17 Vulkan Native Raster";
        app.apiVersion=kMinimumVulkanVersion;
        std::clog<<"[CSE WARP Vulkan] Loader="
                 <<VK_VERSION_MAJOR(instanceVersion)<<"."
                 <<VK_VERSION_MINOR(instanceVersion)<<"."
                 <<VK_VERSION_PATCH(instanceVersion)
                 <<" requested="<<VK_VERSION_MAJOR(app.apiVersion)<<"."
                 <<VK_VERSION_MINOR(app.apiVersion)<<"."
                 <<VK_VERSION_PATCH(app.apiVersion)<<"\n";
        VkInstanceCreateInfo i{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};i.pApplicationInfo=&app;
        vkcheck(vkCreateInstance(&i,nullptr,&instance_),"create instance");
        uint32_t n=0;vkcheck(vkEnumeratePhysicalDevices(instance_,&n,nullptr),"enumerate GPUs");
        if(!n)throw std::runtime_error("No Vulkan GPU/driver installed");
        std::vector<VkPhysicalDevice> devices(n);
        vkcheck(vkEnumeratePhysicalDevices(instance_,&n,devices.data()),"list GPUs");
        int best=-1;
        for(auto pd:devices){
            VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(pd,&props);
            if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU)continue;
            std::clog<<"[CSE WARP Vulkan] Candidate GPU "<<props.deviceName<<" API="
                     <<VK_VERSION_MAJOR(props.apiVersion)<<"."
                     <<VK_VERSION_MINOR(props.apiVersion)<<"."
                     <<VK_VERSION_PATCH(props.apiVersion)<<"\n";
            if(!vulkanVersion::supported(props.apiVersion))continue;
            uint32_t qn=0;vkGetPhysicalDeviceQueueFamilyProperties(pd,&qn,nullptr);
            std::vector<VkQueueFamilyProperties> qs(qn);
            vkGetPhysicalDeviceQueueFamilyProperties(pd,&qn,qs.data());
            for(uint32_t j=0;j<qn;j++)if(qs[j].queueFlags&VK_QUEUE_GRAPHICS_BIT){
                const int score=(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?100:
                                 props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?90:10);
                if(score>best){best=score;physical_=pd;family_=j;}
                break;
            }
        }
        if(!physical_)throw std::runtime_error("No hardware GPU exposing Vulkan API 1.3.204 or newer; the Vulkan SDK/loader version does not prove GPU compatibility. Try DirectX11 or check the Intel driver");
        vkGetPhysicalDeviceProperties(physical_,&props_);
        vkGetPhysicalDeviceMemoryProperties(physical_,&memoryTypes_);
        vkGetPhysicalDeviceFeatures(physical_,&features_);
        adapter_=props_.deviceName;
        // Optional color-format experiment: BGRA8 matches Qt ARGB32 byte order
        // on little-endian Windows. Leave RGBA8 as the default reference.
        const char* requestedBgra=std::getenv("CSE_WARP_VK_BGRA_READBACK");
        if(requestedBgra && std::strcmp(requestedBgra,"1")==0) {
            VkFormatProperties format{};
            vkGetPhysicalDeviceFormatProperties(physical_,VK_FORMAT_B8G8R8A8_UNORM,&format);
            constexpr VkFormatFeatureFlags needed=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
            if((format.optimalTilingFeatures & needed)!=needed)
                throw std::runtime_error("BGRA8 readback unsupported by physical Vulkan GPU (COLOR_ATTACHMENT, BLEND and TRANSFER_SRC); unset CSE_WARP_VK_BGRA_READBACK");
            bgraReadback_=true;
            colorFormat_=VK_FORMAT_B8G8R8A8_UNORM;
        }
        // HD620 4-scene fidelity validation: sRGB hardware target is default.
        // Explicit CSE_WARP_VK_SRGB_COLOR_TEST=0 restores old UNORM for diagnostics.
        const char* requestedSrgb=std::getenv("CSE_WARP_VK_SRGB_COLOR_TEST");
        if(requestedSrgb && std::strcmp(requestedSrgb,"0")!=0 &&
           std::strcmp(requestedSrgb,"1")!=0)
            throw std::runtime_error("CSE_WARP_VK_SRGB_COLOR_TEST accepts only 0 or 1");
        srgbColorEnabled_=!requestedSrgb || std::strcmp(requestedSrgb,"0")!=0;
        if(srgbColorEnabled_){
            const VkFormat candidate=bgraReadback_?
                VK_FORMAT_B8G8R8A8_SRGB:VK_FORMAT_R8G8B8A8_SRGB;
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physical_,candidate,&properties);
            constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
            if((properties.optimalTilingFeatures & required)!=required)
                throw std::runtime_error("GPU lacks sRGB framebuffer support; use DirectX11 or set CSE_WARP_VK_SRGB_COLOR_TEST=0 for legacy diagnostics");
            colorFormat_=candidate;
        }
        std::clog<<"[CSE WARP Vulkan] Readback color="<<(bgraReadback_?"BGRA8":"RGBA8")
                 <<" transfer="<<(srgbColorEnabled_?"sRGB hardware default":"legacy UNORM diagnostic")
                 <<" (no calibration, =0 legacy diagnostic)\n";
        // Always-on engine pipeline: no per-frame or startup A/B decision.
        // DX11 has its own independent renderer and is not modified.
        std::clog<<"[CSE WARP Vulkan] Generic opaque Z-prepass=always-on (no runtime toggle)\n";
        // Query the exact color and depth image sample counts as Khronos recommends.
        VkFormatProperties dp{};
        vkGetPhysicalDeviceFormatProperties(physical_,depthFormat_,&dp);
        if(!(dp.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
            depthFormat_=VK_FORMAT_D16_UNORM;
        auto capabilities=[&](VkFormat fmt,VkImageUsageFlags usage){
            VkImageFormatProperties result{};
            return vkGetPhysicalDeviceImageFormatProperties(physical_,fmt,VK_IMAGE_TYPE_2D,
                VK_IMAGE_TILING_OPTIMAL,usage,0,&result)==VK_SUCCESS
                ?result.sampleCounts:VkSampleCountFlags(0);
        };
        const VkSampleCountFlags supported=props_.limits.framebufferColorSampleCounts&
            props_.limits.framebufferDepthSampleCounts&
            capabilities(colorFormat_,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)&
            capabilities(depthFormat_,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        const char* msaa=std::getenv("CSE_WARP_VK_MSAA");
        if(msaa){
            if(std::strcmp(msaa,"1")==0)msaaSetting_=1;
            else if(std::strcmp(msaa,"2")==0)msaaSetting_=2;
            else if(std::strcmp(msaa,"4")==0)msaaSetting_=4;
            else throw std::runtime_error("CSE_WARP_VK_MSAA accepts 1, 2, or 4");
        }
        msaaSamples_=(msaaSetting_>=4&&(supported&VK_SAMPLE_COUNT_4_BIT))?
            VK_SAMPLE_COUNT_4_BIT:(msaaSetting_>=2&&(supported&VK_SAMPLE_COUNT_2_BIT))?
            VK_SAMPLE_COUNT_2_BIT:VK_SAMPLE_COUNT_1_BIT;
        if(!(supported&msaaSamples_))
            throw std::runtime_error("No supported Vulkan color/depth framebuffer sample count");
        std::clog<<"[CSE WARP Vulkan] MSAA requested="<<msaaSetting_
                 <<" active="<<uint32_t(msaaSamples_)<<" inline resolve\n";
        std::clog<<"[CSE WARP Vulkan] GPU="<<adapter_
                 <<" version="<<VK_VERSION_MAJOR(props_.apiVersion)<<"."
                 <<VK_VERSION_MINOR(props_.apiVersion)<<"."
                 <<VK_VERSION_PATCH(props_.apiVersion)<<" driver="<<props_.driverVersion<<"\n";
        float priority=1.f;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex=family_;queue.queueCount=1;queue.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enable{};
        enable.samplerAnisotropy=features_.samplerAnisotropy;
        VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        d.queueCreateInfoCount=1;d.pQueueCreateInfos=&queue;d.pEnabledFeatures=&enable;
        vkcheck(vkCreateDevice(physical_,&d,nullptr,&device_),"create GPU device");
        vkGetDeviceQueue(device_,family_,0,&queue_);
        VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family_;
        vkcheck(vkCreateCommandPool(device_,&cp,nullptr,&commands_),"command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ca.commandPool=commands_;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        vkcheck(vkAllocateCommandBuffers(device_,&ca,&cmd_),"command buffer");
        VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vkcheck(vkCreateFence(device_,&fc,nullptr,&fence_),"fence");
        VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=2;
        vkcheck(vkCreateQueryPool(device_,&qi,nullptr,&queries_),"GPU timestamps");
        VkDescriptorSetLayoutBinding bindings[8]{};
        for(uint32_t j=0;j<8;j++){
            bindings[j].binding=j;bindings[j].descriptorCount=1;
            bindings[j].descriptorType=j?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[j].stageFlags=j?VK_SHADER_STAGE_FRAGMENT_BIT:(VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        li.bindingCount=8;li.pBindings=bindings;
        vkcheck(vkCreateDescriptorSetLayout(device_,&li,nullptr,&descriptorLayout_),"descriptor layout");
        VkDescriptorPoolSize pools[2]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},
                                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,7}};
        VkDescriptorPoolCreateInfo p{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        p.maxSets=1;p.poolSizeCount=2;p.pPoolSizes=pools;
        vkcheck(vkCreateDescriptorPool(device_,&p,nullptr,&descriptorPool_),"descriptor pool");
        VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        da.descriptorPool=descriptorPool_;da.descriptorSetCount=1;da.pSetLayouts=&descriptorLayout_;
        vkcheck(vkAllocateDescriptorSets(device_,&da,&descriptor_),"descriptor");
        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pl.setLayoutCount=1;pl.pSetLayouts=&descriptorLayout_;
        vkcheck(vkCreatePipelineLayout(device_,&pl,nullptr,&layout_),"pipeline layout");
        VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        si.magFilter=VK_FILTER_LINEAR;si.minFilter=VK_FILTER_LINEAR;
        si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;
        si.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.anisotropyEnable=features_.samplerAnisotropy;
        si.maxAnisotropy=features_.samplerAnisotropy?std::min(8.f,props_.limits.maxSamplerAnisotropy):1.f;
        si.maxLod=32.f; // Auto tier: GPU trilinear down to last valid mip.
        vkcheck(vkCreateSampler(device_,&si,nullptr,&albedoSampler_),"texture sampler");
        si.maxLod=0.f;  // Preserve tier: never lower resolution with distance.
        vkcheck(vkCreateSampler(device_,&si,nullptr,&protectedSampler_),"protected sampler");
        si.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.anisotropyEnable=VK_FALSE;si.minFilter=VK_FILTER_NEAREST;si.magFilter=VK_FILTER_NEAREST;
        vkcheck(vkCreateSampler(device_,&si,nullptr,&shadowSampler_),"shadow sampler");
        si.minFilter=VK_FILTER_LINEAR;si.magFilter=VK_FILTER_LINEAR;
        si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.maxLod=0.f;
        vkcheck(vkCreateSampler(device_,&si,nullptr,&giSampler_),"GI sampler");
        uniform_=buffer(sizeof(Uniforms),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        VkAttachmentDescription attach[3]{};
        attach[0].format=colorFormat_;
        attach[0].samples=msaaSamples_;
        attach[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attach[0].storeOp=msaaSamples_==VK_SAMPLE_COUNT_1_BIT?VK_ATTACHMENT_STORE_OP_STORE:VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attach[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attach[0].finalLayout=msaaSamples_==VK_SAMPLE_COUNT_1_BIT?VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attach[1].format=depthFormat_;attach[1].samples=msaaSamples_;
        attach[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attach[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attach[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attach[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        // MSAA color + MSAA depth + inline single-sample color resolve.
        attach[2].format=colorFormat_;attach[2].samples=VK_SAMPLE_COUNT_1_BIT;
        attach[2].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attach[2].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attach[2].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attach[2].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkAttachmentReference refs[3]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                                        {1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
                                        {2,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount=1;sub.pColorAttachments=&refs[0];sub.pDepthStencilAttachment=&refs[1];
        sub.pResolveAttachments=msaaSamples_==VK_SAMPLE_COUNT_1_BIT?nullptr:&refs[2];
        VkSubpassDependency dep[2]{};
        dep[0].srcSubpass=VK_SUBPASS_EXTERNAL;dep[0].dstSubpass=0;
        dep[0].srcStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dep[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep[1].srcSubpass=0;dep[1].dstSubpass=VK_SUBPASS_EXTERNAL;
        dep[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
        dep[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rp.attachmentCount=msaaSamples_==VK_SAMPLE_COUNT_1_BIT?2:3;rp.pAttachments=attach;rp.subpassCount=1;rp.pSubpasses=&sub;
        rp.dependencyCount=2;rp.pDependencies=dep;
        vkcheck(vkCreateRenderPass(device_,&rp,nullptr,&renderPass_),"render pass");
        VkPipelineCacheCreateInfo pc{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        const std::vector<uint8_t> initialCache=readPipelineCache();
        pc.initialDataSize=initialCache.size();
        pc.pInitialData=initialCache.empty()?nullptr:initialCache.data();
        VkResult cacheResult=vkCreatePipelineCache(device_,&pc,nullptr,&pipelineCache_);
        if(cacheResult!=VK_SUCCESS){
            // Driver may reject stale cache for reasons beyond UUID/version.
            pc.initialDataSize=0;pc.pInitialData=nullptr;
            vkcheck(vkCreatePipelineCache(device_,&pc,nullptr,&pipelineCache_),
                    "create fresh pipeline cache");
        }
        buildPipelines();
        gi0_=image3D(1,1,1);gi1_=image3D(1,1,1);
        const float blackProbe[4]={0.f,0.f,0.f,1.f};
        uploadProbe(gi0_,blackProbe,sizeof(blackProbe),false);
        uploadProbe(gi1_,blackProbe,sizeof(blackProbe),false);
        gi0Uploaded_=gi1Uploaded_=true;
        // Pipelines are already created; cache I/O is outside the frame loop.
        writePipelineCache();
        started_=true;
    }
    void buildPipelines(){
        const auto vs=spirv(shaderFile("raster.vert.spv"));
        // Compare baseline and experimental fragment shader without modifying
        // DX11 or the default Vulkan profile. One renderer instance per process.
        const char* selected=std::getenv("CSE_WARP_VK_FAST_FRAGMENT");
        const bool fastFragment=selected && std::strcmp(selected,"1")==0;
        std::clog<<"[CSE WARP Vulkan] Fragment profile="
                 <<(fastFragment?"optimized":"baseline")<<"\n";
        const auto fs=spirv(shaderFile(fastFragment?"raster_fast.frag.spv":"raster.frag.spv"));
        VkShaderModule modules[2]{};
        for(int i=0;i<2;i++){
            const auto& code=i?fs:vs;
            VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            ci.codeSize=code.size()*4;ci.pCode=code.data();
            vkcheck(vkCreateShaderModule(device_,&ci,nullptr,&modules[i]),"shader module");
        }
        VkPipelineShaderStageCreateInfo stages[2]{};
        for(int i=0;i<2;i++){
            stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;
            stages[i].module=modules[i];stages[i].pName="main";
        }
        VkVertexInputBindingDescription vb{0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attrs[4]{
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,p)},
            {1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,n)},
            {2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,uv)},
            {3,0,VK_FORMAT_R32_UINT,offsetof(Vertex,material)}};
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&vb;
        vi.vertexAttributeDescriptionCount=4;vi.pVertexAttributeDescriptions=attrs;
        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vp.viewportCount=1;vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode=VK_POLYGON_MODE_FILL;rs.lineWidth=1.f;rs.cullMode=VK_CULL_MODE_NONE;
        rs.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples=msaaSamples_;
        VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        ds.depthTestEnable=VK_TRUE;ds.depthWriteEnable=VK_TRUE;
        ds.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
        VkPipelineColorBlendAttachmentState blend{};
        blend.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount=1;cb.pAttachments=&blend;
        VkDynamicState dynamicStates[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dy.dynamicStateCount=2;dy.pDynamicStates=dynamicStates;
        VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vi;pi.pInputAssemblyState=&ia;
        pi.pViewportState=&vp;pi.pRasterizationState=&rs;pi.pMultisampleState=&ms;
        pi.pDepthStencilState=&ds;pi.pColorBlendState=&cb;pi.pDynamicState=&dy;
        pi.layout=layout_;pi.renderPass=renderPass_;
        // Optional opaque depth-only pipeline: same vertex shader and geometry,
        // but NO fragment shader or color writes. Transparent draw is excluded.
        // State is restored exactly for the normal color pass.
        auto depthPipeline=[&](VkPipeline* result,const char* label){
            const uint32_t stagesBefore=pi.stageCount;
            const VkColorComponentFlags maskBefore=blend.colorWriteMask;
            const VkBool32 writeBefore=ds.depthWriteEnable;
            pi.stageCount=1; // vertex only; fragment shader never runs.
            blend.colorWriteMask=0;
            ds.depthWriteEnable=VK_TRUE;
            vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&pi,nullptr,result),label);
            ds.depthWriteEnable=writeBefore;
            blend.colorWriteMask=maskBefore;
            pi.stageCount=stagesBefore;
        };
        vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&pi,nullptr,&opaquePipeline_),"opaque Vulkan pipeline");
        depthPipeline(&depthOpaque_,"opaque depth-only Vulkan pipeline");
        blend.blendEnable=VK_TRUE;blend.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
        blend.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.colorBlendOp=VK_BLEND_OP_ADD;
        blend.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;blend.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blend.alphaBlendOp=VK_BLEND_OP_ADD;
        ds.depthWriteEnable=VK_FALSE;
        vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&pi,nullptr,&alphaPipeline_),"transparent Vulkan pipeline");
        // Third pipeline: original V17 instanced box geometry, no CPU tessellation per box.
        const auto boxCode=spirv(shaderFile("raster_box.vert.spv"));
        VkShaderModule boxModule=VK_NULL_HANDLE;
        VkShaderModuleCreateInfo boxCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        boxCI.codeSize=boxCode.size()*4;boxCI.pCode=boxCode.data();
        vkcheck(vkCreateShaderModule(device_,&boxCI,nullptr,&boxModule),"box shader module");
        stages[0].module=boxModule;
        std::array<VkVertexInputBindingDescription,2> boxBindings{{
            {0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX},
            {1,sizeof(vkmesh::BoxInstance),VK_VERTEX_INPUT_RATE_INSTANCE}
        }};
        std::array<VkVertexInputAttributeDescription,10> boxAttrs{};
        for(size_t i=0;i<4;i++)boxAttrs[i]=attrs[i];
        using Box=vkmesh::BoxInstance;
        boxAttrs[4]={4,1,VK_FORMAT_R32G32B32_SFLOAT,uint32_t(offsetof(Box,lo))};
        boxAttrs[5]={5,1,VK_FORMAT_R32_SFLOAT,uint32_t(offsetof(Box,uvScale))};
        boxAttrs[6]={6,1,VK_FORMAT_R32G32B32_SFLOAT,uint32_t(offsetof(Box,hi))};
        boxAttrs[7]={7,1,VK_FORMAT_R32_UINT,uint32_t(offsetof(Box,material))};
        boxAttrs[8]={8,1,VK_FORMAT_R32G32B32_SFLOAT,uint32_t(offsetof(Box,dimensions))};
        boxAttrs[9]={9,1,VK_FORMAT_R32_UINT,uint32_t(offsetof(Box,uvMode))};
        vi.vertexBindingDescriptionCount=2;vi.pVertexBindingDescriptions=boxBindings.data();
        vi.vertexAttributeDescriptionCount=uint32_t(boxAttrs.size());vi.pVertexAttributeDescriptions=boxAttrs.data();
        ds.depthWriteEnable=VK_TRUE;
        blend.blendEnable=VK_FALSE;
        vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&pi,nullptr,&boxPipeline_),"instanced box Vulkan pipeline");
        depthPipeline(&depthBox_,"instanced box depth-only Vulkan pipeline");
        vkDestroyShaderModule(device_,boxModule,nullptr);
        // Shared canopy geometry: V17 CPU-generated foliage uses a 32-byte instance.
        const auto foliageCode=spirv(shaderFile("raster_foliage.vert.spv"));
        VkShaderModule foliageModule=VK_NULL_HANDLE;
        VkShaderModuleCreateInfo folCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        folCI.codeSize=foliageCode.size()*4;folCI.pCode=foliageCode.data();
        vkcheck(vkCreateShaderModule(device_,&folCI,nullptr,&foliageModule),"foliage shader module");
        stages[0].module=foliageModule;
        std::array<VkVertexInputBindingDescription,2> folBindings{{
            {0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX},
            {1,sizeof(vkmesh::FoliageInstance),VK_VERTEX_INPUT_RATE_INSTANCE}
        }};
        std::array<VkVertexInputAttributeDescription,8> folAttrs{};
        for(size_t i=0;i<4;i++)folAttrs[i]=attrs[i];
        using Foliage=vkmesh::FoliageInstance;
        folAttrs[4]={4,1,VK_FORMAT_R32G32B32_SFLOAT,uint32_t(offsetof(Foliage,center))};
        folAttrs[5]={5,1,VK_FORMAT_R32_UINT,uint32_t(offsetof(Foliage,material))};
        folAttrs[6]={6,1,VK_FORMAT_R32G32B32_SFLOAT,uint32_t(offsetof(Foliage,radii))};
        folAttrs[7]={7,1,VK_FORMAT_R32_SFLOAT,uint32_t(offsetof(Foliage,padding))};
        vi.vertexBindingDescriptionCount=2;vi.pVertexBindingDescriptions=folBindings.data();
        vi.vertexAttributeDescriptionCount=uint32_t(folAttrs.size());vi.pVertexAttributeDescriptions=folAttrs.data();
        vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&pi,nullptr,&foliagePipeline_),"instanced foliage Vulkan pipeline");
        depthPipeline(&depthFoliage_,"instanced foliage depth-only Vulkan pipeline");
        vkDestroyShaderModule(device_,foliageModule,nullptr);

        // Dedicated fullscreen sky pass (no vertex buffer, no depth test or write).
        // Vulkan pipeline/descriptor layout follows the official Khronos contract.
        const auto skyVsCode=spirv(shaderFile("sky.vert.spv"));
        const auto skyFsCode=spirv(shaderFile("sky.frag.spv"));
        VkShaderModule skyModules[2]{};
        for(int i=0;i<2;++i){
            const auto& code=i?skyFsCode:skyVsCode;
            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize=code.size()*sizeof(uint32_t);info.pCode=code.data();
            vkcheck(vkCreateShaderModule(device_,&info,nullptr,&skyModules[i]),"sky SPIR-V shader");
        }
        VkPipelineShaderStageCreateInfo skyStages[2]={stages[0],stages[1]};
        skyStages[0].module=skyModules[0];
        skyStages[1].module=skyModules[1];
        VkPipelineVertexInputStateCreateInfo emptyVertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineDepthStencilStateCreateInfo skyDepth=ds;
        // Opaque Z-prepass is guaranteed before sky. Depth EQUAL=1.0 means
        // only background (or transparency without opaque depth) shades sky.
        // This removes cloud work hidden by buildings without changing sky pixels.
        skyDepth.depthTestEnable=VK_TRUE;
        skyDepth.depthWriteEnable=VK_FALSE;
        skyDepth.depthCompareOp=VK_COMPARE_OP_EQUAL;
        VkGraphicsPipelineCreateInfo skyPi=pi;
        skyPi.pStages=skyStages;
        skyPi.pVertexInputState=&emptyVertex;
        skyPi.pDepthStencilState=&skyDepth;
        vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&skyPi,nullptr,&skyPipeline_),
                "atmospheric sky Vulkan graphics pipeline");
        vkDestroyShaderModule(device_,skyModules[0],nullptr);
        vkDestroyShaderModule(device_,skyModules[1],nullptr);
        vkDestroyShaderModule(device_,modules[0],nullptr);
        vkDestroyShaderModule(device_,modules[1],nullptr);
    }
    void targets(uint32_t w,uint32_t h){
        if(w==width_&&h==height_)return;
        if(framebuffer_)vkDestroyFramebuffer(device_,framebuffer_,nullptr);
        framebuffer_=VK_NULL_HANDLE;
        free(color_);free(msColor_);free(depth_);free(readback_);
        width_=w;height_=h;
        color_=image(w,h,1,colorFormat_,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        if(msaaSamples_!=VK_SAMPLE_COUNT_1_BIT)
            msColor_=image(w,h,1,colorFormat_,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT,false,1,msaaSamples_);
        depth_=image(w,h,1,depthFormat_,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT,false,1,msaaSamples_);
        VkImageView views[3]={msaaSamples_==VK_SAMPLE_COUNT_1_BIT?color_.view:msColor_.view,
                              depth_.view,color_.view};
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass=renderPass_;
        fb.attachmentCount=msaaSamples_==VK_SAMPLE_COUNT_1_BIT?2:3;
        fb.pAttachments=views;fb.width=w;fb.height=h;fb.layers=1;
        vkcheck(vkCreateFramebuffer(device_,&fb,nullptr,&framebuffer_),"Vulkan framebuffer");
        readback_=buffer(VkDeviceSize(w)*h*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,true);
        std::clog<<"[CSE WARP Vulkan] Readback memory HOST_CACHED="
                 <<bool(readback_.flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                 <<" HOST_COHERENT="<<bool(readback_.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)<<"\n";
    }
    void textures(const Scene& scene){
        free(albedo_);free(surface_);
        free(detailAlbedo_);free(detailSurface_;
        protectedLayer_.fill(-1);
        const uint32_t layers=uint32_t(std::max<size_t>(1,std::min<size_t>(kMaxMaterials,scene.mat.size())));
        if(layers>props_.limits.maxImageArrayLayers)
            throw std::runtime_error("GPU maxImageArrayLayers insufficient for scene");
        // Allocate within a fixed working set on shared-memory HD620. Keep full
        // 2048 pixels when there are few materials; reduce only busy scenes.
        uint64_t budget=128ull*1024ull*1024ull;
        for(uint32_t i=0;i<memoryTypes_.memoryHeapCount;i++)
            if(memoryTypes_.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                budget=std::min(budget,std::max<uint64_t>(16ull*1024ull*1024ull,memoryTypes_.memoryHeaps[i].size/8));
        uint32_t side=128;
        for(uint32_t candidate:{256u,512u,1024u,2048u})
            if(candidate<=props_.limits.maxImageDimension2D &&
               uint64_t(candidate)*candidate*layers*8ull<=budget)side=candidate;
        if(side>props_.limits.maxImageDimension2D)
            throw std::runtime_error("GPU image dimensions too small");
        auto supportsLinearBlit=[&](VkFormat format){
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physical_,format,&properties);
            constexpr VkFormatFeatureFlags required=VK_FORMAT_FEATURE_BLIT_SRC_BIT |
               VK_FORMAT_FEATURE_BLIT_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            return (properties.optimalTilingFeatures&required)==required;
        };
        const bool supported=supportsLinearBlit(VK_FORMAT_R8G8B8A8_SRGB) &&
                             supportsLinearBlit(VK_FORMAT_R8G8B8A8_UNORM);
        uint32_t levels=1;
        if(supported){
            for(uint32_t n=2;n<=vkmips::levelCount(side);n++){
                if(vkmips::texelsForLevels(side,n)*layers*8ull<=budget)
                    levels=n;
                else break;
            }
        }
        // Never reduce the original base texture resolution to fit mips.
        // If the budget is tight, keep fewer mips; preserve level 0.
        std::clog<<"[CSE WARP Vulkan] PBR array layers="<<layers
                 <<" texture-side="<<side<<" pixels, mip-levels="<<levels
                 <<" trilinear="<<(levels>1?"yes":"unsupported")<<", budget="
                 <<budget/(1024*1024)<<" MiB\n";
        const VkImageUsageFlags usage=VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | (levels>1?VK_IMAGE_USAGE_TRANSFER_SRC_BIT:0u);
        albedo_=image(side,side,layers,VK_FORMAT_R8G8B8A8_SRGB,
                      usage,VK_IMAGE_ASPECT_COLOR_BIT,true,levels);
        surface_=image(side,side,layers,VK_FORMAT_R8G8B8A8_UNORM,
                       usage,VK_IMAGE_ASPECT_COLOR_BIT,true,levels);
        std::vector<uint32_t> alb(size_t(side)*side*layers,0xff808080u);
        std::vector<uint32_t> surf(alb.size(),0xffFF8080u); // norm (128,128,255), roughness 255
        for(uint32_t layer=0;layer<layers&&layer<scene.mat.size();layer++){
            const Material& m=scene.mat[layer];
            const ColorLevel* color=nearestLevel(m.colorMip,side);
            const Level* legacy=nearestLevel(m.mip,side);
            const Level* normal=nearestLevel(m.normalMip,side);
            const Level* rough=nearestLevel(m.roughnessMip,side);
            for(uint32_t y=0;y<side;y++)for(uint32_t x=0;x<side;x++){
                const size_t index=size_t(layer)*side*side+size_t(y)*side+x;
                uint32_t rgb=0x999999u;
                if(color&&!color->p.empty()&&color->size){
                    const size_t i=size_t(y*color->size/side)*color->size+(x*color->size/side);
                    if(i<color->p.size())rgb=color->p[i]&0xffffffu;
                }else if(legacy&&!legacy->p.empty()&&legacy->size){
                    const size_t i=size_t(y*legacy->size/side)*legacy->size+(x*legacy->size/side);
                    if(i<legacy->p.size())rgb=unpack565(legacy->p[i]);
                }
                alb[index]=rgba8(rgb);
                uint32_t n=0x8080ffu;
                if(normal&&!normal->p.empty()&&normal->size){
                    const size_t i=size_t(y*normal->size/side)*normal->size+(x*normal->size/side);
                    if(i<normal->p.size())n=unpack565(normal->p[i]);
                }
                uint32_t rv=255;
                if(rough&&!rough->p.empty()&&rough->size){
                    const size_t i=size_t(y*rough->size/side)*rough->size+(x*rough->size/side);
                    if(i<rough->p.size()){
                        const uint32_t c=unpack565(rough->p[i]);
                        rv=((c>>16&255u)+(c>>8&255u)+(c&255u))/3;
                    }
                }
                surf[index]=(n>>16&255u)|((n>>8&255u)<<8)|((n&255u)<<16)|(rv<<24);
            }
        }
        uploadImage(albedo_,alb.data(),alb.size()*sizeof(uint32_t));
        uploadImage(surface_,surf.data(),surf.size()*sizeof(uint32_t));

        // A separate GPU texture tier for any scene-authored 'preserve' material.
        // Only those materials consume high-resolution memory; default auto
        // materials retain the exact historical budget, sampling and colors.
        std::vector<uint32_t> protectedIds;
        uint32_t highSide=0;
        for(uint32_t m=0;m<scene.mat.size();++m){
            const Material& material=scene.mat[m];
            if(!material.preserveTextureDetail)continue;
            if(material.colorMip.empty()||material.colorMip.front().size<material.minimumTextureResolution)
                throw std::runtime_error("Texture preservation impossible: source mip missing for "+material.name);
            highSide=std::max(highSide,material.colorMip.front().size);
            protectedLayer_[m]=int32_t(protectedIds.size());
            protectedIds.push_back(m);
        }
        if(!protectedIds.empty()){
            if(highSide>props_.limits.maxImageDimension2D ||
               protectedIds.size()>props_.limits.maxImageArrayLayers)
                throw std::runtime_error("GPU cannot preserve requested texture resolution/layers");
            const uint64_t needed=uint64_t(highSide)*highSide*protectedIds.size()*8ull;
            // Hard guard protects smaller integrated GPUs: fail explicitly rather
            // than ever silently downscale a protected material.
            constexpr uint64_t kMaxProtectedBytes=256ull*1024ull*1024ull;
            if(needed>kMaxProtectedBytes)
                throw std::runtime_error("Protected PBR textures exceed 256 MiB budget; split assets or raise compatible GPU capacity");
            detailAlbedo_=image(highSide,highSide,uint32_t(protectedIds.size()),
                VK_FORMAT_R8G8B8A8_SRGB,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,true);
            detailSurface_=image(highSide,highSide,uint32_t(protectedIds.size()),
                VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,true);
            const size_t layerPixels=size_t(highSide)*highSide;
            std::vector<uint32_t> albedoHigh(layerPixels*protectedIds.size(),0xff808080u);
            std::vector<uint32_t> surfaceHigh(layerPixels*protectedIds.size(),0xffff8080u);
            for(size_t layer=0;layer<protectedIds.size();++layer){
                const Material& mat=scene.mat[protectedIds[layer]];
                const ColorLevel* color=nearestLevel(mat.colorMip,highSide);
                const Level* legacy=nearestLevel(mat.mip,highSide);
                const Level* normal=nearestLevel(mat.normalMip,highSide);
                const Level* rough=nearestLevel(mat.roughnessMip,highSide);
                if(!color || color->size<mat.minimumTextureResolution)
                    throw std::runtime_error("Protected color mip resolution below descriptor minimum: "+mat.name);
                for(uint32_t y=0;y<highSide;y++)for(uint32_t x=0;x<highSide;x++){
                    const size_t dst=layer*layerPixels+size_t(y)*highSide+x;
                    uint32_t rgb=0x999999u;
                    if(color&&!color->p.empty()&&color->size){
                        const size_t at=size_t(y*color->size/highSide)*color->size+(x*color->size/highSide);
                        if(at<color->p.size())rgb=color->p[at]&0xffffffu;
                    }else if(legacy&&!legacy->p.empty()&&legacy->size){
                        const size_t at=size_t(y*legacy->size/highSide)*legacy->size+(x*legacy->size/highSide);
                        if(at<legacy->p.size())rgb=unpack565(legacy->p[at]);
                    }
                    albedoHigh[dst]=rgba8(rgb);
                    uint32_t n=0x8080ffu;
                    if(normal&&!normal->p.empty()&&normal->size){
                        const size_t at=size_t(y*normal->size/highSide)*normal->size+(x*normal->size/highSide);
                        if(at<normal->p.size())n=unpack565(normal->p[at]);
                    }
                    uint32_t rv=255;
                    if(rough&&!rough->p.empty()&&rough->size){
                        const size_t at=size_t(y*rough->size/highSide)*rough->size+(x*rough->size/highSide);
                        if(at<rough->p.size()){
                            const uint32_t c=unpack565(rough->p[at]);
                            rv=((c>>16&255u)+(c>>8&255u)+(c&255u))/3;
                        }
                    }
                    surfaceHigh[dst]=(n>>16&255u)|((n>>8&255u)<<8)|((n&255u)<<16)|(rv<<24);
                }
            }
            uploadImage(detailAlbedo_,albedoHigh.data(),albedoHigh.size()*sizeof(uint32_t));
            uploadImage(detailSurface_,surfaceHigh.data(),surfaceHigh.size()*sizeof(uint32_t));
            std::clog<<"[CSE WARP Vulkan] Protected texture tier: "<<protectedIds.size()
                     <<" materials, "<<highSide<<"x"<<highSide<<", "<<needed/(1024*1024)
                     <<" MiB, no distance-based LOD reduction\n";
        }
    }
    void updateShadow(const ShadowMap& sh){
        if(sh.d.data()==shadowData_&&uint32_t(sh.size)==shadowSize_&&shadow_.image&&cachedSolarHour_==solarHour)return;
        const uint32_t size=(sh.size>0&&sh.size<=4096&&sh.d.size()==size_t(sh.size)*sh.size)?
                             uint32_t(sh.size):1;
        if(!shadow_.image||shadow_.width!=size){
            free(shadow_);
            shadow_=image(size,size,1,VK_FORMAT_R32_SFLOAT,
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        }
        // A new upload starts from UNDEFINED, as all old content is overwritten.
        const float fallbackShadowDepth=1e9f;
        uploadImage(shadow_,size==1?static_cast<const void*>(&fallbackShadowDepth):
                    static_cast<const void*>(sh.d.data()),size_t(size)*size*sizeof(float));
        shadowData_=sh.d.data();shadowSize_=uint32_t(sh.size);cachedSolarHour_=solarHour;
        updateDescriptors();
    }
    void rebuild(const Scene& scene){
        if(scene.mat.size()>kMaxMaterials)throw std::runtime_error("Vulkan port: more than 256 materials");
        free(vertices_);free(transparent_);free(boxTemplate_);free(boxInstances_);
        free(foliageTemplate_);free(foliageInstances_);
        glass_.clear();chunks_.clear();boxChunks_.clear();foliageChunks_.clear();
        boxCount_=0;foliageCount_=0;foliageTemplateCount_=0;
        std::vector<vkmesh::BoxInstance> boxData;
        std::vector<vkmesh::FoliageInstance> foliageData;
        vkmesh::MeshBuilder mesh=vkmesh::build(scene,&boxData,&foliageData);
        if(mesh.vertices.empty()&&mesh.transparent.empty()&&boxData.empty()&&foliageData.empty())
            throw std::runtime_error("Vulkan: scene contains no rasterizable geometry");
        if(mesh.vertices.size()+mesh.transparent.size()*3>12000000u)
            throw std::runtime_error("Vulkan scene exceeds 12M vertex safety limit");
        vertexCount_=uint32_t(mesh.vertices.size());
        if(vertexCount_)vertices_=uploadVertices(mesh.vertices.data(),size_t(vertexCount_)*sizeof(Vertex));
        if(!boxData.empty()){
            vkmesh::MeshBuilder cube;
            cube.box({0,0,0},{1,1,1},0,1.f);
            if(cube.vertices.size()!=36u)throw std::runtime_error("Invalid V17 box template");
            boxTemplate_=uploadVertices(cube.vertices.data(),cube.vertices.size()*sizeof(Vertex));
            boxInstances_=uploadVertices(boxData.data(),boxData.size()*sizeof(vkmesh::BoxInstance));
            boxCount_=uint32_t(boxData.size());
            constexpr size_t boxBatch=128;
            for(size_t first=0;first<boxData.size();first+=boxBatch){
                const size_t end=std::min(first+boxBatch,boxData.size());
                Chunk chunk;chunk.first=uint32_t(first);chunk.count=uint32_t(end-first);
                chunk.lo={boxData[first].lo[0],boxData[first].lo[1],boxData[first].lo[2]};
                chunk.hi={boxData[first].hi[0],boxData[first].hi[1],boxData[first].hi[2]};
                for(size_t i=first+1;i<end;i++){
                    const auto& box=boxData[i];
                    for(int ax=0;ax<3;ax++){
                        float* lo=ax==0?&chunk.lo.x:ax==1?&chunk.lo.y:&chunk.lo.z;
                        float* hi=ax==0?&chunk.hi.x:ax==1?&chunk.hi.y:&chunk.hi.z;
                        *lo=std::min(*lo,box.lo[ax]);
                        *hi=std::max(*hi,box.hi[ax]);
                    }
                }
                boxChunks_.push_back(chunk);
            }
        }
        if(!foliageData.empty()){
            auto tpl=vkmesh::makeFoliageTemplate(12,6,12);
            if(tpl.empty()||tpl.size()%3)throw std::runtime_error("Foliage template invalid");
            foliageTemplate_=uploadVertices(tpl.data(),tpl.size()*sizeof(Vertex));
            foliageInstances_=uploadVertices(foliageData.data(),foliageData.size()*sizeof(vkmesh::FoliageInstance));
            foliageTemplateCount_=uint32_t(tpl.size());foliageCount_=uint32_t(foliageData.size());
            constexpr size_t foliageBatch=128;
            for(size_t first=0;first<foliageData.size();first+=foliageBatch){
                const size_t end=std::min(first+foliageBatch,foliageData.size());
                Chunk chunk;chunk.first=uint32_t(first);chunk.count=uint32_t(end-first);
                const auto& start=foliageData[first];
                chunk.lo={start.center[0]-start.radii[0],start.center[1]-start.radii[1],start.center[2]-start.radii[2]};
                chunk.hi={start.center[0]+start.radii[0],start.center[1]+start.radii[1],start.center[2]+start.radii[2]};
                for(size_t i=first+1;i<end;i++){
                    const auto& obj=foliageData[i];
                    for(int a=0;a<3;a++){
                        float* lo=a==0?&chunk.lo.x:a==1?&chunk.lo.y:&chunk.lo.z;
                        float* hi=a==0?&chunk.hi.x:a==1?&chunk.hi.y:&chunk.hi.z;
                        *lo=std::min(*lo,obj.center[a]-obj.radii[a]*1.25f);
                        *hi=std::max(*hi,obj.center[a]+obj.radii[a]*1.25f);
                    }
                }
                foliageChunks_.push_back(chunk);
            }
        }
        glass_=std::move(mesh.transparent);
        glassOrder_.resize(glass_.size());
        transparentCount_=uint32_t(glass_.size()*3);
        if(transparentCount_)transparent_=buffer(size_t(transparentCount_)*sizeof(Vertex),
                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        constexpr size_t chunkSize=4096*3;
        for(size_t first=0;first<mesh.vertices.size();first+=chunkSize){
            const size_t end=std::min(first+chunkSize,mesh.vertices.size());
            Chunk c;c.first=uint32_t(first);c.count=uint32_t(end-first);
            const Vertex& v=mesh.vertices[first];
            c.lo=c.hi={v.p[0],v.p[1],v.p[2]};
            for(size_t i=first+1;i<end;i++)for(int axis=0;axis<3;axis++){
                float* lo=axis==0?&c.lo.x:axis==1?&c.lo.y:&c.lo.z;
                float* hi=axis==0?&c.hi.x:axis==1?&c.hi.y:&c.hi.z;
                *lo=std::min(*lo,mesh.vertices[i].p[axis]);
                *hi=std::max(*hi,mesh.vertices[i].p[axis]);
            }
            chunks_.push_back(c);
        }
        textures(scene);
        sceneIdentity_=scene.gpuCacheIdentity;
        sceneRevision_=scene.gpuCacheRevision;
        sceneObjects_=scene.obj.size();sceneMaterials_=scene.mat.size();scenePointer_=&scene;
        updateDescriptors();
    }
    bool visible(const Chunk& c,const Camera& cam) const{
        const V center=(c.lo+c.hi)*.5f;
        const float radius=length(c.hi-c.lo)*.5f+.01f;
        const V q=center-cam.eye;
        const float z=dot(q,cam.f);
        if(z+radius<.08f||z-radius>10000.f)return false;
        const float x=dot(q,cam.r),y=dot(q,cam.u);
        const float h=std::max(.001f,cam.tanHalf),v=h/std::max(.001f,cam.aspect);
        if(std::abs(x)>z*h+radius*std::sqrt(1.f+h*h))return false;
        if(std::abs(y)>z*v+radius*std::sqrt(1.f+v*v))return false;
        return true;
    }
    void updateUniforms(const Scene& scene,const ShadowMap& shadows,const Camera& cam){
        Uniforms f{};
        f.eye[0]=cam.eye.x;f.eye[1]=cam.eye.y;f.eye[2]=cam.eye.z;f.eye[3]=10000.f;
        f.forward[0]=cam.f.x;f.forward[1]=cam.f.y;f.forward[2]=cam.f.z;f.forward[3]=cam.aspect;
        f.right[0]=cam.r.x;f.right[1]=cam.r.y;f.right[2]=cam.r.z;f.right[3]=cam.tanHalf;
        f.up[0]=cam.u.x;f.up[1]=cam.u.y;f.up[2]=cam.u.z;f.up[3]=.08f;
        f.sunDir[0]=sun.x;f.sunDir[1]=sun.y;f.sunDir[2]=sun.z;f.sunDir[3]=daylight;
        const V sk=scene.underwaterEnvironment?V{7,27,39}:skyHorizon*.65f+skyZenith*.35f;
        f.sky[0]=sk.x/255.f;f.sky[1]=sk.y/255.f;f.sky[2]=sk.z/255.f;
        // Reuse the V17 weather model and DX11 atmospheric constants.
        cse::weather::CloudState weather;
        weather.coverage=.28f;weather.opacity=.65f;
        weather.windEastMetersPerSecond=12.f;
        weather.windNorthMetersPerSecond=3.5f;
        static const auto cloudEpoch=Clock::now();
        const char* animate=std::getenv("M3D_V17_ANIMATE_CLOUDS");
        const double elapsed=(animate && std::strcmp(animate,"1")==0)?
            std::chrono::duration<double>(Clock::now()-cloudEpoch).count():0.0;
        const auto clouds=cse::weather::cirrusAt(
            weather,elapsed,std::asin(clampf(sun.y,-1.f,1.f))*180.0f/3.14159265f);
        f.skyZenith[0]=skyZenith.x/255.f;
        f.skyZenith[1]=skyZenith.y/255.f;
        f.skyZenith[2]=skyZenith.z/255.f;
        f.skyZenith[3]=clouds.opacity;
        f.skyHorizon[0]=skyHorizon.x/255.f;
        f.skyHorizon[1]=skyHorizon.y/255.f;
        f.skyHorizon[2]=skyHorizon.z/255.f;
        f.skyHorizon[3]=clouds.coverage;
        f.skyWind[0]=clouds.uOffset;
        f.skyWind[1]=clouds.vOffset;
        f.skyWind[2]=solarHour;
        f.skyViewport[0]=float(width_);
        f.skyViewport[1]=float(height_);
        f.skyViewport[2]=scene.underwaterEnvironment?1.f:0.f;
        f.shadowBase[0]=shadows.base.x;f.shadowBase[1]=shadows.base.y;f.shadowBase[2]=shadows.base.z;
        f.shadowBase[3]=shadows.span;
        f.shadowRight[0]=shadows.right.x;f.shadowRight[1]=shadows.right.y;f.shadowRight[2]=shadows.right.z;
        f.shadowRight[3]=shadows.dist;
        f.shadowUp[0]=shadows.up.x;f.shadowUp[1]=shadows.up.y;f.shadowUp[2]=shadows.up.z;
        f.shadowUp[3]=shadow_.width;
        for(size_t i=0;i<scene.mat.size()&&i<kMaxMaterials;i++){
            const Material& m=scene.mat[i];
            f.textureDetail[i][0]=float(protectedLayer_[i]);
            f.textureDetail[i][1]=(m.waterSurface||
                  (!m.surfaceAuthored && m.name=="cse_pool_water"))?1.f:0.f;
            f.material[i][0]=clampf(m.metallic,0.f,1.f);
            f.material[i][1]=clampf(m.roughnessScale,.08f,1.f);
            f.material[i][2]=1.f-float(m.transmissionPercent)/100.f;
            f.material[i][3]=m.normalMip.empty()?0.f:1.f;
            V emission=m.emissionColor;
            float strength=m.emissionIntensity;
            if(!m.emissionAuthored){
                // Backwards compatibility only: explicit descriptor always wins.
                // This block will disappear when legacy scenes are migrated.
                const auto& name=m.name;
                if(name=="cse_neon_red"){emission={1.f,.09f,.035f};strength=1.65f;}
                else if(name=="cse_neon_cyan"){emission={.03f,.89f,1.f};strength=1.50f;}
                else if(name=="cse_neon_gold"){emission={1.f,.72f,.24f};strength=1.35f;}
                else if(name=="cse_pool_strip"){emission={.83f,.91f,1.f};strength=.24f;}
                else if(name=="lamp"){emission={1.f,.65f,.36f};strength=.75f+1.9f*lampFactor;}
                else if(name=="led"){emission={1.f,.85f,.65f};strength=.48f+.85f*lampFactor;}
            }
            f.emission[i][0]=emission.x;f.emission[i][1]=emission.y;f.emission[i][2]=emission.z;
            f.emission[i][3]=strength;
        }
        uint32_t lights=0;
        auto addLight=[&](const Light& l){
            if(lights>=kLights)return;
            const uint32_t j=lights++;
            f.lightPos[j][0]=l.p.x;f.lightPos[j][1]=l.p.y;f.lightPos[j][2]=l.p.z;
            f.lightPos[j][3]=l.radius;
            f.lightRgb[j][0]=l.color.x/255.f;f.lightRgb[j][1]=l.color.y/255.f;
            f.lightRgb[j][2]=l.color.z/255.f;f.lightRgb[j][3]=l.intensity;
        };
        for(const Light& l:pointLights)addLight(l);
        for(const Light& l:scene.lights)addLight(l);
        if(giAvailable_){
            f.giOrigin[0]=giTable_.origin.x;f.giOrigin[1]=giTable_.origin.y;
            f.giOrigin[2]=giTable_.origin.z;
            f.giInvSpan[0]=1.f/std::max(.001f,giTable_.span.x);
            f.giInvSpan[1]=1.f/std::max(.001f,giTable_.span.y);
            f.giInvSpan[2]=1.f/std::max(.001f,giTable_.span.z);
            f.giInfo[0]=1.f;f.giInfo[1]=giMix_;
        }
        f.misc[0]=float(lights);
        f.misc[1]=srgbColorEnabled_?1.f:0.f; // linear shader -> sRGB framebuffer output
        memcpy(uniform_.mapped,&f,sizeof(f));flush(uniform_);
    }
public:
    VulkanRaster()=default;
    ~VulkanRaster(){
        if(!device_){if(instance_)vkDestroyInstance(instance_,nullptr);return;}
        vkDeviceWaitIdle(device_);
        if(framebuffer_)vkDestroyFramebuffer(device_,framebuffer_,nullptr);
        free(readback_);free(uniform_);free(vertices_);free(transparent_);free(boxTemplate_);free(boxInstances_);free(foliageTemplate_);free(foliageInstances_);
        free(color_);free(msColor_);free(depth_);free(albedo_);free(surface_);free(shadow_);
        free(detailAlbedo_);free(detailSurface_);
        free(gi0_);free(gi1_);
        if(skyPipeline_)vkDestroyPipeline(device_,skyPipeline_,nullptr);
        if(depthFoliage_)vkDestroyPipeline(device_,depthFoliage_,nullptr);
        if(depthBox_)vkDestroyPipeline(device_,depthBox_,nullptr);
        if(depthOpaque_)vkDestroyPipeline(device_,depthOpaque_,nullptr);
        if(foliagePipeline_)vkDestroyPipeline(device_,foliagePipeline_,nullptr);
        if(boxPipeline_)vkDestroyPipeline(device_,boxPipeline_,nullptr);
        if(alphaPipeline_)vkDestroyPipeline(device_,alphaPipeline_,nullptr);
        if(opaquePipeline_)vkDestroyPipeline(device_,opaquePipeline_,nullptr);
        if(pipelineCache_)vkDestroyPipelineCache(device_,pipelineCache_,nullptr);
        if(layout_)vkDestroyPipelineLayout(device_,layout_,nullptr);
        if(renderPass_)vkDestroyRenderPass(device_,renderPass_,nullptr);
        if(albedoSampler_)vkDestroySampler(device_,albedoSampler_,nullptr);
        if(protectedSampler_)vkDestroySampler(device_,protectedSampler_,nullptr);
        if(shadowSampler_)vkDestroySampler(device_,shadowSampler_,nullptr);
        if(giSampler_)vkDestroySampler(device_,giSampler_,nullptr);
        if(descriptorPool_)vkDestroyDescriptorPool(device_,descriptorPool_,nullptr);
        if(descriptorLayout_)vkDestroyDescriptorSetLayout(device_,descriptorLayout_,nullptr);
        if(queries_)vkDestroyQueryPool(device_,queries_,nullptr);
        if(fence_)vkDestroyFence(device_,fence_,nullptr);
        if(commands_)vkDestroyCommandPool(device_,commands_,nullptr);
        vkDestroyDevice(device_,nullptr);
        if(instance_)vkDestroyInstance(instance_,nullptr);
    }
    bool render(const Scene& scene,const ShadowMap& shadows,const Camera& cam,
                int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
        const auto started=Clock::now();
        try {
            if(w<1||h<1||w>4096||h>4096)throw std::runtime_error("Invalid render size");
            if(!started_)configure();
            if(width_!=uint32_t(w)||height_!=uint32_t(h))targets(uint32_t(w),uint32_t(h));
            const bool changed=scenePointer_!=&scene||sceneObjects_!=scene.obj.size()
                 ||sceneMaterials_!=scene.mat.size()||scene.gpuCacheIdentity==0
                 ||sceneIdentity_!=scene.gpuCacheIdentity||sceneRevision_!=scene.gpuCacheRevision;
            if(changed)rebuild(scene);
            updateShadow(shadows);
            ensureGI(scene);
            updateUniforms(scene,shadows,cam);
            VkViewport viewport{0.f,0.f,float(w),float(h),0.f,1.f};
            VkRect2D scissor{{0,0},{uint32_t(w),uint32_t(h)}};
            begin();
            vkCmdResetQueryPool(cmd_,queries_,0,2);
            vkCmdWriteTimestamp(cmd_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,queries_,0);
            VkClearValue clears[3]{};
            const V sky=scene.underwaterEnvironment?V{7,27,39}:skyHorizon*.65f+skyZenith*.35f;
            clears[0].color={{sky.x/255.f,sky.y/255.f,sky.z/255.f,1.f}};
            clears[1].depthStencil={1.f,0};
            VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            pass.renderPass=renderPass_;pass.framebuffer=framebuffer_;
            pass.renderArea=scissor;pass.clearValueCount=msaaSamples_==VK_SAMPLE_COUNT_1_BIT?2:3;pass.pClearValues=clears;
            vkCmdBeginRenderPass(cmd_,&pass,VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(cmd_,0,1,&viewport);vkCmdSetScissor(cmd_,0,1,&scissor);
            vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&descriptor_,0,nullptr);
            uint64_t drawCalls=0;
            // Opaque depth is generated first. The sky then runs only for
            // background pixels (depth still == 1) through early depth tests.
            // No new visual approximation: opaque color and transparencies
            // retain their exact passes, and sky runs under transparent glass.
            // Transparent triangles are drawn once after the opaque color pass.
            auto drawOpaque=[&](bool depthOnly){
                if(vertexCount_){
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,depthOnly?depthOpaque_:opaquePipeline_);
                VkDeviceSize off=0;VkBuffer vb=vertices_.handle;
                vkCmdBindVertexBuffers(cmd_,0,1,&vb,&off);
                for(const Chunk& c:chunks_)if(visible(c,cam)){
                    vkCmdDraw(cmd_,c.count,1,c.first,0);drawCalls++;
                }
            }
            if(boxCount_){
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,depthOnly?depthBox_:boxPipeline_);
                VkBuffer boxVBs[2]={boxTemplate_.handle,boxInstances_.handle};
                VkDeviceSize boxOffsets[2]{0,0};
                vkCmdBindVertexBuffers(cmd_,0,2,boxVBs,boxOffsets);
                for(const Chunk& chunk:boxChunks_)if(visible(chunk,cam)){
                    vkCmdDraw(cmd_,36,chunk.count,0,chunk.first);
                    ++drawCalls;
                }
            }
            if(foliageCount_){
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,depthOnly?depthFoliage_:foliagePipeline_);
                VkBuffer buffers[2]={foliageTemplate_.handle,foliageInstances_.handle};
                VkDeviceSize offsets[2]{0,0};
                vkCmdBindVertexBuffers(cmd_,0,2,buffers,offsets);
                for(const Chunk& chunk:foliageChunks_)if(visible(chunk,cam)){
                    vkCmdDraw(cmd_,foliageTemplateCount_,chunk.count,0,chunk.first);
                    ++drawCalls;
                }
            }
            };
            drawOpaque(true); // Mandatory depth-only opaque pass.
            // One fullscreen triangle; sky shader skipped where opaque depth is < 1.
            vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,skyPipeline_);
            vkCmdDraw(cmd_,3,1,0,0);++drawCalls;
            drawOpaque(false); // Identical opaque color pass; glass once afterwards.
            if(transparentCount_){
                // CPU depth-sort transparent triangles every frame, then draw with depth-test but no depth-write.
                std::iota(glassOrder_.begin(),glassOrder_.end(),size_t(0));
                std::sort(glassOrder_.begin(),glassOrder_.end(),[&](size_t a,size_t b){
                    return dot(glass_[a].center-cam.eye,cam.f)>dot(glass_[b].center-cam.eye,cam.f);
                });
                auto* dest=static_cast<Vertex*>(transparent_.mapped);
                for(size_t i=0;i<glassOrder_.size();i++)
                    memcpy(dest+3*i,glass_[glassOrder_[i]].v,3*sizeof(Vertex));
                flush(transparent_);
                VkDeviceSize off=0;VkBuffer vb=transparent_.handle;
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,alphaPipeline_);
                vkCmdBindVertexBuffers(cmd_,0,1,&vb,&off);
                vkCmdDraw(cmd_,transparentCount_,1,0,0);drawCalls++;
            }
            vkCmdEndRenderPass(cmd_);
            vkCmdWriteTimestamp(cmd_,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,queries_,1);
            VkBufferImageCopy bc{};
            bc.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;bc.imageSubresource.layerCount=1;
            bc.imageExtent={uint32_t(w),uint32_t(h),1};
            vkCmdCopyImageToBuffer(cmd_,color_.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   readback_.handle,1,&bc);
            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
            barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer=readback_.handle;barrier.size=VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(cmd_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,
                                 0,nullptr,1,&barrier,0,nullptr);
            double submissionMs=0,waitMs=0;
            submit(&submissionMs,&waitMs);
            const auto copied=Clock::now();
            invalidate(readback_);
            pixels.resize(size_t(w)*h);
            const auto* mapped=static_cast<const uint32_t*>(readback_.mapped);
            vkreadback::toArgb32(mapped,pixels.data(),pixels.size(),
                bgraReadback_?vkreadback::PixelBytes::Bgra8:vkreadback::PixelBytes::Rgba8);
            stats=Stats{};
            stats.gpu=true;stats.gpuDrawCalls=drawCalls;
            stats.gpuBoxInstances=boxCount_;
            stats.gpuFoliageInstances=foliageCount_;
            stats.cpuSubmitMs=submissionMs;
            stats.cpuMapWaitMs=waitMs;
            stats.cpuPixelConvertMs=ms(copied,Clock::now());
            stats.readbackBufferCount=1;
            stats.ms=ms(started,Clock::now());
            const auto freq=props_.limits.timestampPeriod;
            uint64_t stamps[2]{};
            if(vkGetQueryPoolResults(device_,queries_,0,2,sizeof(stamps),stamps,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT)==VK_SUCCESS
               &&stamps[1]>=stamps[0]){
                stats.gpuDrawMs=double(stamps[1]-stamps[0])*double(freq)*1e-6;
                stats.gpuTotalMs=stats.gpuDrawMs;stats.gpuTimingValid=true;
            }
            error.clear();return true;
        }catch(const std::exception& e){
            const std::string what=e.what();
            error.assign(what.begin(),what.end());
            return false;
        }
    }
};
} // namespace

bool renderVulkanPreview(const Scene& scene,const ShadowMap& shadow,const Camera& camera,
                         int w,int h,std::vector<uint32_t>& out,Stats& stats,std::wstring& error){
    // Vulkan contexts may not be driven simultaneously from multiple threads.
    // Each Qt worker owns its own resources, like the existing D3D11 backend.
    thread_local VulkanRaster renderer;
    return renderer.render(scene,shadow,camera,w,h,out,stats,error);
}

} // namespace m3d
