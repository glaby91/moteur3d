#include "moteur3d_v17_render.h"

namespace cse::v17render {

BackendResult renderLumaTrace(const Request& request,const m3d::Camera& camera,
                              const m3d::ShadowMap& shadow){
    BackendResult result;
    const auto& snapshot=*request.scene;
    const auto& scene=snapshot.renderScene;

    m3d::releaseVulkanDirectForCurrentThread();
    m3d::releaseVulkanPreviewForCurrentThread();
    if(!snapshot.lumaTraceBvhError.isEmpty()){
        result.reason=snapshot.lumaTraceBvhError.toStdWString();
    }else if(snapshot.lumaTraceBvh4.nodes.empty()){
        result.reason=L"Le BVH4 LumaTrace n’a pas été construit pour cette scène.";
    }else if(request.directPresent){
        result.ok=m3d::renderLumaTraceNative(scene,snapshot.lumaTraceBvh4,shadow,camera,
            request.width,request.height,reinterpret_cast<void*>(request.nativeHandle),
            result.stats,result.reason);
        if(!result.ok){
            const std::wstring nativeReason=result.reason;
            result.nativeFailure=QString::fromStdWString(nativeReason);
            m3d::releaseDirectX11NativeForCurrentThread();
            result.reason.clear();
            result.ok=m3d::renderLumaTracePreview(scene,snapshot.lumaTraceBvh4,shadow,camera,
                request.width,request.height,result.pixels,result.stats,result.reason);
            result.nativeFallback=result.ok;
            if(!result.ok){
                const std::wstring previewReason=result.reason;
                result.reason=L"LumaTrace DXGI natif en échec : "+nativeReason+
                    L" ; aperçu LumaTrace également en échec : "+previewReason;
            }
        }
    }else{
        m3d::releaseDirectX11NativeForCurrentThread();
        result.ok=m3d::renderLumaTracePreview(scene,snapshot.lumaTraceBvh4,shadow,camera,
            request.width,request.height,result.pixels,result.stats,result.reason);
    }
    return result;
}

} // namespace cse::v17render
