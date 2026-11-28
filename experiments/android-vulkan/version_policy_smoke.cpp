#include "m3d_vulkan_version.h"
#include <cassert>
#include <initializer_list>
#include <iostream>
int main(){
    using namespace m3d::vulkanVersion;
    static_assert(minimum==VK_MAKE_API_VERSION(0,1,3,204));
    static_assert(!supported(VK_MAKE_API_VERSION(0,1,3,0)));
    static_assert(supported(VK_MAKE_API_VERSION(0,1,4,0)));
    int n=0;
    for(bool build:{false,true})for(bool opt:{false,true})for(bool cpu:{false,true}){
        assert(diagnosticDeviceSupported(VK_MAKE_API_VERSION(0,1,3,0),build,opt,cpu)==(build&&opt&&cpu));++n;
        assert(!diagnosticDeviceSupported(VK_MAKE_API_VERSION(0,1,2,999),build,opt,cpu));++n;
        assert(!diagnosticDeviceSupported(VK_MAKE_API_VERSION(1,1,3,0),build,opt,cpu));++n;
        assert(diagnosticDeviceSupported(minimum,build,opt,cpu));++n;
        assert(!diagnosticDeviceSupported(VK_MAKE_API_VERSION(0,1,3,203),build,opt,cpu));++n;
    }
    std::cout<<"PASS "<<n<<" Vulkan policy checks; production unchanged\n";
    return n==40?0:1;
}
