#include "m3d_vulkan_version.h"
#include <iostream>
int main(){
    using namespace m3d::vulkanVersion;
    if(supported(VK_MAKE_API_VERSION(0,1,2,500)))return 1;
    if(supported(VK_MAKE_API_VERSION(0,1,3,203)))return 2;
    if(!supported(VK_MAKE_API_VERSION(0,1,3,204)))return 3;
    if(!supported(VK_MAKE_API_VERSION(0,1,3,260)))return 4;
    if(!supported(VK_MAKE_API_VERSION(0,1,4,0)))return 5;
    if(supported(VK_MAKE_API_VERSION(1,1,3,204)))return 6;
    std::cout<<"PASS Vulkan 1.3.204 minimum API policy\n";
    return 0;
}
