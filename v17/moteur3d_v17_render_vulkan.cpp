#include "moteur3d_v17_render.h"

namespace cse::v17render {

BackendResult renderVulkanRaster(const Request& request,const m3d::Camera& camera,
                                 const m3d::ShadowMap& shadow){
    BackendResult result;
    const auto& scene=request.scene->renderScene;

    m3d::releaseDirectX11NativeForCurrentThread();
    if(request.directPresent)m3d::releaseVulkanPreviewForCurrentThread();
    else m3d::releaseVulkanDirectForCurrentThread();

    if(request.directPresent){
        result.ok=m3d::renderVulkanDirectPresent(scene,shadow,camera,request.width,request.height,
            reinterpret_cast<void*>(request.nativeHandle),result.stats,result.reason,
            request.sharedVulkanInstance,request.sharedVulkanSurface);
        if(!result.ok){
            const std::wstring nativeReason=result.reason;
            result.nativeFailure=QString::fromStdWString(nativeReason);
            m3d::releaseVulkanDirectForCurrentThread();
            result.reason.clear();
            result.ok=m3d::renderVulkanPreview(scene,shadow,camera,request.width,request.height,
                result.pixels,result.stats,result.reason);
            result.nativeFallback=result.ok;
            if(!result.ok){
                const std::wstring previewReason=result.reason;
                result.reason=L"Vulkan native presentation failed: "+nativeReason+
                    L"; Vulkan image preview also failed: "+previewReason;
            }
        }
    }else{
        result.ok=m3d::renderVulkanPreview(scene,shadow,camera,request.width,request.height,
            result.pixels,result.stats,result.reason);
    }
    return result;
}

} // namespace cse::v17render
