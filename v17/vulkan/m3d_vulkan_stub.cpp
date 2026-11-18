#include "m3d_engine.h"
namespace m3d {
void releaseVulkanPreviewForCurrentThread(){}
void releaseVulkanDirectForCurrentThread(){}
bool renderVulkanPreview(const Scene&,const ShadowMap&,const Camera&,int,int,
                         std::vector<uint32_t>&,Stats&,std::wstring& error){
    error=L"Backend Vulkan indisponible : installer le SDK Vulkan lors de la compilation.";
    return false;
}
bool renderVulkanDirectPresent(const Scene&,const ShadowMap&,const Camera&,int,int,
                              void*,Stats&,std::wstring& error,void*,uintptr_t){
    error=L"Présentation directe Vulkan indisponible : installer le SDK Vulkan lors de la compilation.";
    return false;
}
}
