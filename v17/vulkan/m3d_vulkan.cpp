// CSE WARP V17 Vulkan Raster (experimental independent backend).
// Requires Vulkan API 1.3.204, native GPU raster of the existing Scene and Camera.
// Offscreen color is read back for the current Qt QImage host. No swapchain.
#include "m3d_engine.h"
#include "m3d_vulkan_mesh.h"
#include "m3d_vulkan_version.h"
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
    uint32_t width=0,height=0,layers=0;
};
struct alignas(16) Uniforms {
    float eye[4]{},forward[4]{},right[4]{},up[4]{},sunDir[4]{},sky[4]{};
    float shadowBase[4]{},shadowRight[4]{},shadowUp[4]{};
    float material[kMaxMaterials][4]{},emission[kMaxMaterials][4]{};
    float lightPos[kLights][4]{},lightRgb[kLights][4]{},misc[4]{};
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
    VkPipelineCache pipelineCache_=VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_=VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;
    VkDescriptorSet descriptor_=VK_NULL_HANDLE;
    VkSampler albedoSampler_=VK_NULL_HANDLE,shadowSampler_=VK_NULL_HANDLE;
    Texture color_,depth_,albedo_,surface_,shadow_;
    VkFramebuffer framebuffer_=VK_NULL_HANDLE;
    Buffer vertices_,transparent_,boxTemplate_,boxInstances_,foliageTemplate_,foliageInstances_,uniform_,readback_;
    uint32_t vertexCount_=0,transparentCount_=0,boxCount_=0,foliageCount_=0,foliageTemplateCount_=0;
    std::vector<vkmesh::TransparentTriangle> glass_;
    std::vector<Chunk> chunks_,boxChunks_,foliageChunks_;
    std::vector<uint32_t> stagingColors_;
    uint32_t width_=0,height_=0;
    uint64_t sceneIdentity_=0,sceneRevision_=~uint64_t(0);
    size_t sceneObjects_=0,sceneMaterials_=0;
    const Scene* scenePointer_=nullptr;
    const float* shadowData_=nullptr;
    uint32_t shadowSize_=0;
    float cachedSolarHour_=std::numeric_limits<float>::quiet_NaN();
    std::string adapter_;
    bool started_=false;
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
                  VkImageUsageFlags usage,VkImageAspectFlags aspect,bool forceArrayView=false){
        Texture t;t.width=w;t.height=h;t.layers=layers;t.format=format;
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ci.imageType=VK_IMAGE_TYPE_2D;ci.format=format;ci.extent={w,h,1};
        ci.mipLevels=1;ci.arrayLayers=layers;ci.samples=VK_SAMPLE_COUNT_1_BIT;
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
        vi.subresourceRange.levelCount=1;vi.subresourceRange.layerCount=layers;
        vkcheck(vkCreateImageView(device_,&vi,nullptr,&t.view),"create view");
        return t;
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
        b.subresourceRange.levelCount=1;b.subresourceRange.layerCount=t.layers;
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
        barrier(tex,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
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
        if(!uniform_.handle||!albedo_.view||!surface_.view||!shadow_.view)return;
        VkDescriptorBufferInfo u{uniform_.handle,0,sizeof(Uniforms)};
        VkDescriptorImageInfo a{albedoSampler_,albedo_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo n{albedoSampler_,surface_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo s{shadowSampler_,shadow_.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet,4> writes{};
        for(uint32_t i=0;i<4;i++){
            auto& w=writes[i];w.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet=descriptor_;w.dstBinding=i;w.descriptorCount=1;
            w.descriptorType=i?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        writes[0].pBufferInfo=&u;writes[1].pImageInfo=&a;writes[2].pImageInfo=&n;writes[3].pImageInfo=&s;
        vkUpdateDescriptorSets(device_,uint32_t(writes.size()),writes.data(),0,nullptr);
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
        VkDescriptorSetLayoutBinding bindings[4]{};
        for(uint32_t j=0;j<4;j++){
            bindings[j].binding=j;bindings[j].descriptorCount=1;
            bindings[j].descriptorType=j?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[j].stageFlags=j?VK_SHADER_STAGE_FRAGMENT_BIT:(VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        li.bindingCount=4;li.pBindings=bindings;
        vkcheck(vkCreateDescriptorSetLayout(device_,&li,nullptr,&descriptorLayout_),"descriptor layout");
        VkDescriptorPoolSize pools[2]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},
                                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3}};
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
        si.maxLod=0.f;
        vkcheck(vkCreateSampler(device_,&si,nullptr,&albedoSampler_),"texture sampler");
        si.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.anisotropyEnable=VK_FALSE;si.minFilter=VK_FILTER_NEAREST;si.magFilter=VK_FILTER_NEAREST;
        vkcheck(vkCreateSampler(device_,&si,nullptr,&shadowSampler_),"shadow sampler");
        uniform_=buffer(sizeof(Uniforms),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        VkAttachmentDescription attach[2]{};
        attach[0].format=VK_FORMAT_R8G8B8A8_UNORM;
        attach[0].samples=VK_SAMPLE_COUNT_1_BIT;
        attach[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attach[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attach[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attach[0].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        VkFormat depthFormat=VK_FORMAT_D32_SFLOAT;
        VkFormatProperties supported{};vkGetPhysicalDeviceFormatProperties(physical_,depthFormat,&supported);
        if(!(supported.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
            depthFormat=VK_FORMAT_D16_UNORM;
        attach[1].format=depthFormat;attach[1].samples=VK_SAMPLE_COUNT_1_BIT;
        attach[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
        attach[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attach[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        attach[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference refs[2]={{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                                        {1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};
        VkSubpassDescription sub{};sub.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount=1;sub.pColorAttachments=&refs[0];sub.pDepthStencilAttachment=&refs[1];
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
        rp.attachmentCount=2;rp.pAttachments=attach;rp.subpassCount=1;rp.pSubpasses=&sub;
        rp.dependencyCount=2;rp.pDependencies=dep;
        vkcheck(vkCreateRenderPass(device_,&rp,nullptr,&renderPass_),"render pass");
        VkPipelineCacheCreateInfo pc{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        vkcheck(vkCreatePipelineCache(device_,&pc,nullptr,&pipelineCache_),"pipeline cache");
        buildPipelines();
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
        ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
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
        vkcheck(vkCreateGraphicsPipelines(device_,pipelineCache_,1,&pi,nullptr,&opaquePipeline_),"opaque Vulkan pipeline");
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
        vkDestroyShaderModule(device_,foliageModule,nullptr);
        vkDestroyShaderModule(device_,modules[0],nullptr);
        vkDestroyShaderModule(device_,modules[1],nullptr);
    }
    void targets(uint32_t w,uint32_t h){
        if(w==width_&&h==height_)return;
        if(framebuffer_)vkDestroyFramebuffer(device_,framebuffer_,nullptr);
        framebuffer_=VK_NULL_HANDLE;
        free(color_);free(depth_);free(readback_);
        width_=w;height_=h;
        color_=image(w,h,1,VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,VK_IMAGE_ASPECT_COLOR_BIT);
        VkAttachmentDescription a{}; // depth format selected to match supported device features.
        (void)a;
        VkFormat df=VK_FORMAT_D32_SFLOAT;VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(physical_,df,&fp);
        if(!(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))df=VK_FORMAT_D16_UNORM;
        depth_=image(w,h,1,df,VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,VK_IMAGE_ASPECT_DEPTH_BIT);
        VkImageView views[2]={color_.view,depth_.view};
        VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fb.renderPass=renderPass_;fb.attachmentCount=2;fb.pAttachments=views;fb.width=w;fb.height=h;fb.layers=1;
        vkcheck(vkCreateFramebuffer(device_,&fb,nullptr,&framebuffer_),"Vulkan framebuffer");
        readback_=buffer(VkDeviceSize(w)*h*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,true);
        std::clog<<"[CSE WARP Vulkan] Readback memory HOST_CACHED="
                 <<bool(readback_.flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
                 <<" HOST_COHERENT="<<bool(readback_.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)<<"\n";
    }
    void textures(const Scene& scene){
        free(albedo_);free(surface_);
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
        std::clog<<"[CSE WARP Vulkan] PBR array layers="<<layers
                 <<" texture-side="<<side<<" pixels, budget="<<budget/(1024*1024)<<" MiB\n";

        albedo_=image(side,side,layers,VK_FORMAT_R8G8B8A8_SRGB,
                      VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_ASPECT_COLOR_BIT,true);
        surface_=image(side,side,layers,VK_FORMAT_R8G8B8A8_UNORM,
                       VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,VK_IMAGE_ASPECT_COLOR_BIT,true);
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
        f.shadowBase[0]=shadows.base.x;f.shadowBase[1]=shadows.base.y;f.shadowBase[2]=shadows.base.z;
        f.shadowBase[3]=shadows.span;
        f.shadowRight[0]=shadows.right.x;f.shadowRight[1]=shadows.right.y;f.shadowRight[2]=shadows.right.z;
        f.shadowRight[3]=shadows.dist;
        f.shadowUp[0]=shadows.up.x;f.shadowUp[1]=shadows.up.y;f.shadowUp[2]=shadows.up.z;
        f.shadowUp[3]=shadow_.width;
        for(size_t i=0;i<scene.mat.size()&&i<kMaxMaterials;i++){
            const Material& m=scene.mat[i];
            f.material[i][0]=clampf(m.metallic,0.f,1.f);
            f.material[i][1]=clampf(m.roughnessScale,.08f,1.f);
            f.material[i][2]=1.f-float(m.transmissionPercent)/100.f;
            f.material[i][3]=m.normalMip.empty()?0.f:1.f;
            const auto& name=m.name;
            float strength=0.f;V emission{0,0,0};
            if(name=="cse_neon_red"){emission={1.f,.09f,.035f};strength=1.65f;}
            else if(name=="cse_neon_cyan"){emission={.03f,.89f,1.f};strength=1.50f;}
            else if(name=="cse_neon_gold"){emission={1.f,.72f,.24f};strength=1.35f;}
            else if(name=="cse_pool_strip"){emission={.83f,.91f,1.f};strength=.24f;}
            else if(name=="lamp"){emission={1.f,.65f,.36f};strength=.75f+1.9f*lampFactor;}
            else if(name=="led"){emission={1.f,.85f,.65f};strength=.48f+.85f*lampFactor;}
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
        f.misc[0]=float(lights);
        memcpy(uniform_.mapped,&f,sizeof(f));flush(uniform_);
    }
public:
    VulkanRaster()=default;
    ~VulkanRaster(){
        if(!device_){if(instance_)vkDestroyInstance(instance_,nullptr);return;}
        vkDeviceWaitIdle(device_);
        if(framebuffer_)vkDestroyFramebuffer(device_,framebuffer_,nullptr);
        free(readback_);free(uniform_);free(vertices_);free(transparent_);free(boxTemplate_);free(boxInstances_);free(foliageTemplate_);free(foliageInstances_);
        free(color_);free(depth_);free(albedo_);free(surface_);free(shadow_);
        if(foliagePipeline_)vkDestroyPipeline(device_,foliagePipeline_,nullptr);
        if(boxPipeline_)vkDestroyPipeline(device_,boxPipeline_,nullptr);
        if(alphaPipeline_)vkDestroyPipeline(device_,alphaPipeline_,nullptr);
        if(opaquePipeline_)vkDestroyPipeline(device_,opaquePipeline_,nullptr);
        if(pipelineCache_)vkDestroyPipelineCache(device_,pipelineCache_,nullptr);
        if(layout_)vkDestroyPipelineLayout(device_,layout_,nullptr);
        if(renderPass_)vkDestroyRenderPass(device_,renderPass_,nullptr);
        if(albedoSampler_)vkDestroySampler(device_,albedoSampler_,nullptr);
        if(shadowSampler_)vkDestroySampler(device_,shadowSampler_,nullptr);
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
            updateUniforms(scene,shadows,cam);
            VkViewport viewport{0.f,0.f,float(w),float(h),0.f,1.f};
            VkRect2D scissor{{0,0},{uint32_t(w),uint32_t(h)}};
            begin();
            vkCmdResetQueryPool(cmd_,queries_,0,2);
            vkCmdWriteTimestamp(cmd_,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,queries_,0);
            VkClearValue clears[2]{};
            const V sky=scene.underwaterEnvironment?V{7,27,39}:skyHorizon*.65f+skyZenith*.35f;
            clears[0].color={{sky.x/255.f,sky.y/255.f,sky.z/255.f,1.f}};
            clears[1].depthStencil={1.f,0};
            VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            pass.renderPass=renderPass_;pass.framebuffer=framebuffer_;
            pass.renderArea=scissor;pass.clearValueCount=2;pass.pClearValues=clears;
            vkCmdBeginRenderPass(cmd_,&pass,VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(cmd_,0,1,&viewport);vkCmdSetScissor(cmd_,0,1,&scissor);
            vkCmdBindDescriptorSets(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,layout_,0,1,&descriptor_,0,nullptr);
            uint64_t drawCalls=0;
            if(vertexCount_){
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,opaquePipeline_);
                VkDeviceSize off=0;VkBuffer vb=vertices_.handle;
                vkCmdBindVertexBuffers(cmd_,0,1,&vb,&off);
                for(const Chunk& c:chunks_)if(visible(c,cam)){
                    vkCmdDraw(cmd_,c.count,1,c.first,0);drawCalls++;
                }
            }
            if(boxCount_){
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,boxPipeline_);
                VkBuffer boxVBs[2]={boxTemplate_.handle,boxInstances_.handle};
                VkDeviceSize boxOffsets[2]{0,0};
                vkCmdBindVertexBuffers(cmd_,0,2,boxVBs,boxOffsets);
                for(const Chunk& chunk:boxChunks_)if(visible(chunk,cam)){
                    vkCmdDraw(cmd_,36,chunk.count,0,chunk.first);
                    ++drawCalls;
                }
            }
            if(foliageCount_){
                vkCmdBindPipeline(cmd_,VK_PIPELINE_BIND_POINT_GRAPHICS,foliagePipeline_);
                VkBuffer buffers[2]={foliageTemplate_.handle,foliageInstances_.handle};
                VkDeviceSize offsets[2]{0,0};
                vkCmdBindVertexBuffers(cmd_,0,2,buffers,offsets);
                for(const Chunk& chunk:foliageChunks_)if(visible(chunk,cam)){
                    vkCmdDraw(cmd_,foliageTemplateCount_,chunk.count,0,chunk.first);
                    ++drawCalls;
                }
            }
            if(transparentCount_){
                // CPU depth-sort transparent triangles every frame, then draw with depth-test but no depth-write.
                std::vector<size_t> order(glass_.size());
                std::iota(order.begin(),order.end(),size_t(0));
                std::sort(order.begin(),order.end(),[&](size_t a,size_t b){
                    return dot(glass_[a].center-cam.eye,cam.f)>dot(glass_[b].center-cam.eye,cam.f);
                });
                auto* dest=static_cast<Vertex*>(transparent_.mapped);
                for(size_t i=0;i<order.size();i++)
                    memcpy(dest+3*i,glass_[order[i]].v,3*sizeof(Vertex));
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
            const auto* rgba=static_cast<const uint32_t*>(readback_.mapped);
            for(size_t i=0;i<pixels.size();i++){
                uint32_t c=rgba[i];
                pixels[i]=0xff000000u|((c&255u)<<16)|(c&0x0000ff00u)|((c>>16)&255u);
            }
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
