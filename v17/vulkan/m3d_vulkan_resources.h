#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace m3d::vulkan::resources {

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

uint32_t findMemoryType(const VkPhysicalDeviceMemoryProperties& properties,
                        uint32_t bits,VkMemoryPropertyFlags required,
                        bool preferCached=false);
Buffer createBuffer(VkDevice device,const VkPhysicalDeviceMemoryProperties& properties,
                    VkDeviceSize bytes,VkBufferUsageFlags usage,
                    VkMemoryPropertyFlags flags,bool preferCached=false);
void flushBuffer(VkDevice device,const Buffer& buffer);
void invalidateBuffer(VkDevice device,const Buffer& buffer);
void destroyBuffer(VkDevice device,Buffer& buffer) noexcept;
Texture createImage(VkDevice device,const VkPhysicalDeviceMemoryProperties& properties,
                    uint32_t width,uint32_t height,uint32_t layers,VkFormat format,
                    VkImageUsageFlags usage,VkImageAspectFlags aspect,
                    bool forceArrayView=false,uint32_t mipLevels=1,
                    VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT);
Texture createImage3D(VkDevice device,const VkPhysicalDeviceMemoryProperties& properties,
                      uint32_t width,uint32_t height,uint32_t depth);
void destroyTexture(VkDevice device,Texture& texture) noexcept;

} // namespace m3d::vulkan::resources
