#include "moteur3d_v17_render.h"

namespace cse::v17render {

BackendResult renderD3D11Compute(const Request& request,const m3d::Camera& camera){
    BackendResult result;
    m3d::releaseDirectX11NativeForCurrentThread();
    m3d::releaseVulkanDirectForCurrentThread();
    m3d::releaseVulkanPreviewForCurrentThread();
    result.ok=m3d::renderCompute11Preview(request.scene->renderScene,camera,
        request.width,request.height,result.pixels,result.stats,result.reason);
    if(result.ok)result.renderer=QStringLiteral("V16 D3D11 Compute — GI adaptative");
    return result;
}

} // namespace cse::v17render
