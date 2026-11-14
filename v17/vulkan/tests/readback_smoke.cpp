// GPU-free check that RGBA8 and BGRA8 produce identical Qt ARGB32 bytes.
#include "m3d_vulkan_readback.h"
#include <array>
#include <cstdint>
#include <iostream>
int main() {
    constexpr std::array<std::uint32_t, 5> rgba{
        0xff332211u, 0xff000000u, 0xffffffffu, 0x80123456u, 0x00010203u};
    constexpr std::array<std::uint32_t, 5> bgra{
        0xff112233u, 0xff000000u, 0xffffffffu, 0x80563412u, 0x00030201u};
    std::array<std::uint32_t,5> original{},optimized{};
    m3d::vkreadback::toArgb32(rgba.data(),original.data(),rgba.size(),m3d::vkreadback::PixelBytes::Rgba8);
    m3d::vkreadback::toArgb32(bgra.data(),optimized.data(),bgra.size(),m3d::vkreadback::PixelBytes::Bgra8);
    for(std::size_t i=0; i<rgba.size(); ++i){
        if(original[i]!=optimized[i]){
            std::cerr<<"FAIL Vulkan RGBA/BGRA channel parity pixel "<<i<<" RGBA->Qt=0x"
                     <<std::hex<<original[i]<<" BGRA->Qt=0x"<<optimized[i]<<std::endl;
            return 1;
        }
    }
    if(original.front()!=0xff112233u||original.back()!=0xff030201u){
        std::cerr<<"FAIL known Qt ARGB32 color values"<<std::endl;
        return 2;
    }
    std::cout<<"PASS Vulkan RGBA8/BGRA8 to Qt ARGB32 pixel parity"<<std::endl;
    return 0;
}
