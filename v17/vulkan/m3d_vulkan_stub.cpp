#include "m3d_engine.h"
namespace m3d {
bool renderVulkanPreview(const Scene&,const ShadowMap&,const Camera&,int,int,
                         std::vector<uint32_t>&,Stats&,std::wstring& error){
    error=L"Backend Vulkan indisponible : installer le SDK Vulkan lors de la compilation.";
    return false;
}
}
