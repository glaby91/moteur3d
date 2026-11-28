#include "m3d_vulkan_resources.h"

#include "m3d_vulkan_mips.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#ifdef __APPLE__
#include <iostream>
#endif

namespace m3d::vulkan::resources {
namespace {
void check(VkResult result,const char* operation) {
    if(result!=VK_SUCCESS)
        throw std::runtime_error(std::string(operation)+" Vulkan error "+
                                 std::to_string(int(result)));
}
}

uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties& properties,
                        uint32_t bits,VkMemoryPropertyFlags required,
                        bool preferCached) {
    int best=-1;
    uint32_t candidate=UINT32_MAX;
    for(uint32_t i=0;i<properties.memoryTypeCount;++i){
        if((bits&(1u<<i))==0)continue;
        const auto flags=properties.memoryTypes[i].propertyFlags;
        if((flags&required)!=required)continue;
        const int score=(preferCached&&(flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)?64:0)+
            (flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT?8:0)+
            (flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT?4:0);
        if(score>best){best=score;candidate=i;}
    }
    if(candidate==UINT32_MAX)throw std::runtime_error("No suitable Vulkan memory type");
    return candidate;
}

Buffer createBuffer(VkDevice device,const VkPhysicalDeviceMemoryProperties& properties,
                    VkDeviceSize bytes,VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags flags,bool preferCached) {
    Buffer buffer;
    buffer.size=bytes;
    VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    create.size=bytes;create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(device,&create,nullptr,&buffer.handle),"create buffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device,buffer.handle,&requirements);
    const uint32_t index=findMemoryType(properties,requirements.memoryTypeBits,flags,preferCached);
    buffer.flags=properties.memoryTypes[index].propertyFlags;
    buffer.allocation=requirements.size;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=index;
    check(vkAllocateMemory(device,&allocation,nullptr,&buffer.memory),"allocate buffer");
    check(vkBindBufferMemory(device,buffer.handle,buffer.memory,0),"bind buffer");
    if(buffer.flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        check(vkMapMemory(device,buffer.memory,0,VK_WHOLE_SIZE,0,&buffer.mapped),
              "map persistent buffer");
    return buffer;
}

void flushBuffer(VkDevice device,const Buffer& buffer) {
    if((buffer.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)||!buffer.mapped)return;
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory=buffer.memory;range.size=VK_WHOLE_SIZE;
    check(vkFlushMappedMemoryRanges(device,1,&range),"flush mapped buffer");
}

void invalidateBuffer(VkDevice device,const Buffer& buffer) {
    if((buffer.flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)||!buffer.mapped)return;
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
    range.memory=buffer.memory;range.size=VK_WHOLE_SIZE;
    check(vkInvalidateMappedMemoryRanges(device,1,&range),"invalidate mapped buffer");
}

void destroyBuffer(VkDevice device,Buffer& buffer) noexcept {
    if(buffer.mapped)vkUnmapMemory(device,buffer.memory);
    if(buffer.handle)vkDestroyBuffer(device,buffer.handle,nullptr);
    if(buffer.memory)vkFreeMemory(device,buffer.memory,nullptr);
    buffer={};
}

Texture createImage(VkDevice device,const VkPhysicalDeviceMemoryProperties& properties,
                    uint32_t width,uint32_t height,uint32_t layers,VkFormat format,
                    VkImageUsageFlags usage,VkImageAspectFlags aspect,
                    bool forceArrayView,uint32_t mipLevels,VkSampleCountFlagBits samples) {
    Texture texture;
    texture.width=width;texture.height=height;texture.layers=layers;
    texture.format=format;texture.mips=mipLevels;
    if(!mipLevels||mipLevels>vkmips::levelCount(std::max(width,height)))
        throw std::runtime_error("Invalid Vulkan texture mip count");
    VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    create.imageType=VK_IMAGE_TYPE_2D;create.format=format;
    create.extent={width,height,1};create.mipLevels=mipLevels;
    create.arrayLayers=layers;create.samples=samples;create.tiling=VK_IMAGE_TILING_OPTIMAL;
    create.usage=usage;create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateImage(device,&create,nullptr,&texture.image),"create image");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device,texture.image,&requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize=requirements.size;
    allocation.memoryTypeIndex=findMemoryType(properties,requirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    check(vkAllocateMemory(device,&allocation,nullptr,&texture.memory),"allocate image");
    check(vkBindImageMemory(device,texture.image,texture.memory,0),"bind image");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image=texture.image;
    view.viewType=(forceArrayView||layers>1)?VK_IMAGE_VIEW_TYPE_2D_ARRAY:VK_IMAGE_VIEW_TYPE_2D;
    view.format=format;view.subresourceRange.aspectMask=aspect;
    view.subresourceRange.levelCount=mipLevels;view.subresourceRange.layerCount=layers;
    check(vkCreateImageView(device,&view,nullptr,&texture.view),"create view");
    return texture;
}

Texture createImage3D(VkDevice device,const VkPhysicalDeviceMemoryProperties& properties,
                      uint32_t width,uint32_t height,uint32_t depth) {
    Texture texture;
    texture.width=width;texture.height=height;texture.depth=depth;
    texture.layers=1;texture.mips=1;
    texture.format=VK_FORMAT_R32G32B32A32_SFLOAT;
    VkImageCreateInfo create{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    create.imageType=VK_IMAGE_TYPE_3D;create.format=texture.format;
    create.extent={width,height,depth};create.mipLevels=1;create.arrayLayers=1;
    create.samples=VK_SAMPLE_COUNT_1_BIT;create.tiling=VK_IMAGE_TILING_OPTIMAL;
    create.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    try {
    check(vkCreateImage(device,&create,nullptr,&texture.image),"GI 3D image");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device,texture.image,&requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize=requirements.size;
#ifdef __APPLE__
    // Optimal-tiling 3D images are never mapped by this renderer. On MoltenVK,
    // choosing host-visible memory causes bindDeviceMemory to upload through
    // replaceRegion in the Apple paravirtual GPU driver (Build 2 EXC_BAD_ACCESS).
    // Use a strictly non-host-visible device-local type and dedicated binding.
    uint32_t privateBits=0;
    for(uint32_t i=0;i<properties.memoryTypeCount;++i){
        const auto flags=properties.memoryTypes[i].propertyFlags;
        if((flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) &&
           !(flags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT|VK_MEMORY_PROPERTY_PROTECTED_BIT)))
            privateBits|=1u<<i;
    }
    if(!(requirements.memoryTypeBits&privateBits))
        throw std::runtime_error("GI 3D: no compatible GPU-private Vulkan memory on macOS");
    allocation.memoryTypeIndex=findMemoryType(properties,requirements.memoryTypeBits&privateBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};
    dedicated.image=texture.image;
    allocation.pNext=&dedicated;
    std::clog<<"[CSE WARP Vulkan] GI 3D "<<width<<"x"<<height<<"x"<<depth
             <<" private memory type="<<allocation.memoryTypeIndex
             <<" flags="<<properties.memoryTypes[allocation.memoryTypeIndex].propertyFlags
             <<" compatibleBits="<<requirements.memoryTypeBits
             <<" bytes="<<allocation.allocationSize<<std::endl;
#else
    allocation.memoryTypeIndex=findMemoryType(properties,requirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
#endif
    check(vkAllocateMemory(device,&allocation,nullptr,&texture.memory),"GI 3D memory");
    check(vkBindImageMemory(device,texture.image,texture.memory,0),"GI 3D bind");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image=texture.image;view.viewType=VK_IMAGE_VIEW_TYPE_3D;view.format=texture.format;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount=1;view.subresourceRange.layerCount=1;
    check(vkCreateImageView(device,&view,nullptr,&texture.view),"GI 3D view");
    return texture;
    }catch(...){
        destroyTexture(device,texture);
        throw;
    }
}

void destroyTexture(VkDevice device,Texture& texture) noexcept {
    if(texture.view)vkDestroyImageView(device,texture.view,nullptr);
    if(texture.image)vkDestroyImage(device,texture.image,nullptr);
    if(texture.memory)vkFreeMemory(device,texture.memory,nullptr);
    texture={};
}

} // namespace m3d::vulkan::resources
