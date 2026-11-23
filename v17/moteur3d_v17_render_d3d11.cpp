#include "moteur3d_v17_render.h"

namespace cse::v17render {

BackendResult renderD3D11Raster(const Request& request,const m3d::Camera& camera,
                                const m3d::ShadowMap& shadow){
    BackendResult result;
    const auto& scene=request.scene->renderScene;

    // DXGI and Vulkan never share device state on the persistent render worker.
    m3d::releaseVulkanDirectForCurrentThread();
    m3d::releaseVulkanPreviewForCurrentThread();

    if(request.directPresent){
        result.ok=m3d::renderDirectX11Native(scene,shadow,camera,request.width,request.height,
            reinterpret_cast<void*>(request.nativeHandle),result.stats,result.reason);
        if(!result.ok){
            const std::wstring nativeReason=result.reason;
            result.nativeFailure=QString::fromStdWString(nativeReason);
            m3d::releaseDirectX11NativeForCurrentThread();
            result.reason.clear();
            result.ok=m3d::renderDirectX11Preview(scene,shadow,camera,request.width,request.height,
                result.pixels,result.stats,result.reason);
            result.nativeFallback=result.ok;
            if(!result.ok){
                const std::wstring previewReason=result.reason;
                result.reason=L"DXGI native presentation failed: "+nativeReason+
                    L"; DirectX 11 image preview also failed: "+previewReason;
            }
        }
    }else{
        result.ok=m3d::renderDirectX11Preview(scene,shadow,camera,request.width,request.height,
            result.pixels,result.stats,result.reason);
    }
    return result;
}

} // namespace cse::v17render
