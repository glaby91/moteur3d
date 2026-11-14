#include "m3d_vulkan_mips.h"
#include <iostream>
static_assert(m3d::vkmips::levelCount(1)==1,"1x1");
static_assert(m3d::vkmips::levelCount(512)==10,"512");
static_assert(m3d::vkmips::levelCount(2048)==12,"2048");
static_assert(m3d::vkmips::extent(512,9)==1,"final");
static_assert(m3d::vkmips::extent(512,99)==1,"clamped");
static_assert(m3d::vkmips::texelsPerLayer(4)==21,"full chain");
int main(){std::cout<<"PASS Vulkan mipmap policy 512:10, 2048:12, down to 1x1\n";return 0;}
