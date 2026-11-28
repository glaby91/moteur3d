#include "moteur3d_v17_render.h"
#ifdef _WIN32
#include "escargot/gpu_packet.h"
#include "escargot/runtime.h"
#include "escargot/object_projected.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#endif

namespace cse::v17render {

bool prepareEscargotScene(m3d::v16::Runtime& runtime){
#ifdef _WIN32
    std::vector<uint32_t> owners;
    if(!m3d::escargot::objectMap(runtime,owners))return false;
    runtime.renderScene.escargotReceiverIds=std::move(owners);
    runtime.renderScene.markGpuGeometryChanged();
    return true;
#else
    (void)runtime;
    return false;
#endif
}

#ifdef _WIN32
namespace {
namespace e=m3d::escargot;
// Cache immutable catalogue/BVH references only; never reuse an optical image.
// The request owns both inputs for the entire frame. Weak keys prevent holding
// an obsolete large scene in the render worker after a scene switch.
struct ExplorationSession {
    std::weak_ptr<m3d::v16::Runtime> runtime;
    std::weak_ptr<const m3d::v16::SceneDatabase> database;
    uint64_t revision=0;
    std::unique_ptr<e::Prepared> prepared;
    std::vector<uint32_t> objectReceivers;
    std::unique_ptr<e::projected::Session> projected;
};
BackendResult renderEscargotF16(const Request& request,const m3d::Camera& camera,
                               const m3d::ShadowMap& shadow){
    BackendResult result;
    const auto start=std::chrono::steady_clock::now();
    const auto& rt=*request.scene;
    const auto& scene=rt.renderScene;
    if(!request.sceneDatabase||scene.escargotReceiverIds.size()!=scene.obj.size()){
        result.reason=L"Escargot: missing exact receiver ownership or authored scene.";
        return result;
    }
    // Identical resident reflection path and packet construction to validated G.
    thread_local ExplorationSession session;
    e::Options opt;
    opt.rayBudget=4096;
    if(!session.prepared||session.runtime.lock()!=request.scene||
       session.database.lock()!=request.sceneDatabase||session.revision!=scene.gpuCacheRevision){
        session.prepared.reset();session.projected.reset();
        session.runtime=request.scene;
        session.database=request.sceneDatabase;
        session.revision=scene.gpuCacheRevision;
        session.prepared=std::make_unique<e::Prepared>(rt,*request.sceneDatabase,opt.mode);
        session.objectReceivers.assign(size_t(rt.objects.size())+1,0);
        for(const auto& c:e::staticCandidates(rt,*request.sceneDatabase,e::Mode::Reflection))session.objectReceivers[c.object]=1;
    }
    const char* objectEnv=std::getenv("CSE_WARP_ESCARGOT_OBJECT_GPU");
    QString objectFailure;
    const char* legacy=std::getenv("CSE_WARP_ESCARGOT_LEGACY_F16");
    if(!(objectEnv&&std::strcmp(objectEnv,"1")==0)&&!(legacy&&std::strcmp(legacy,"1")==0)){
        if(!session.projected)session.projected=std::make_unique<e::projected::Session>(rt,*request.sceneDatabase);
        if(!session.projected->valid()){result.reason=L"Object006 invalid catalogue";return result;}
        e::projected::Metrics m;
        const auto& plan=session.projected->build(camera,shadow,request.width,request.height,m);
        m3d::EscargotBuildTiming timing;
        if(request.directPresent)result.ok=m3d::renderLumaTraceEscargotGpuBuiltNative(scene,rt.lumaTraceBvh4,shadow,camera,request.width,request.height,reinterpret_cast<void*>(request.nativeHandle),plan,result.stats,timing,result.reason);
        else result.ok=m3d::renderLumaTraceEscargotGpuBuiltPreview(scene,rt.lumaTraceBvh4,shadow,camera,request.width,request.height,plan,result.pixels,result.stats,timing,result.reason);
        if(!result.ok)return result; // Never silently switch to per-pixel BVH4.
        result.stats.rays=m.rays;result.stats.primarySamples=m.rays;
        result.stats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        result.renderer=QStringLiteral("V17 LumaTrace BVH4 — Z Escargot Object 006 Resident — %1 — rays %2 / tri %3 / omitted %4 / deferred %5")
            .arg(request.directPresent?QStringLiteral("Direct GPU (swapchain)"):QStringLiteral("Qt (QImage)"))
            .arg(m.rays).arg(m.triangles).arg(m.omitted).arg(m.deferred+m.curved);
        if(!m.receivers)result.renderer+=QStringLiteral(" — catalogue %1 / patches %2 / visible %3").arg(m.catalogue).arg(m.patches).arg(m.visible);
        return result;
    }
    if(objectEnv&&std::strcmp(objectEnv,"1")==0){
        m3d::EscargotPacketBuild plan;plan.rasterDirect=true;plan.rasterReceivers=session.objectReceivers;
        m3d::EscargotBuildTiming timing;
        if(request.directPresent)result.ok=m3d::renderLumaTraceEscargotGpuBuiltNative(scene,rt.lumaTraceBvh4,shadow,camera,request.width,request.height,reinterpret_cast<void*>(request.nativeHandle),plan,result.stats,timing,result.reason);
        else result.ok=m3d::renderLumaTraceEscargotGpuBuiltPreview(scene,rt.lumaTraceBvh4,shadow,camera,request.width,request.height,plan,result.pixels,result.stats,timing,result.reason);
        if(result.ok&&!result.stats.zReflectActive&&(request.directPresent?result.stats.readbackBufferCount==0:result.pixels.size()==size_t(request.width)*request.height)){
            result.stats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            result.renderer=request.directPresent?QStringLiteral("V17 LumaTrace BVH4 — Z Escargot Object 004 — Direct GPU (swapchain)"):QStringLiteral("V17 LumaTrace BVH4 — Z Escargot Object 004 — Qt (QImage)");
            return result;
        }
        objectFailure=QString::fromStdWString(result.reason);
        if(objectFailure.isEmpty())objectFailure=QStringLiteral("Object004 output failed presentation validation");
        result=BackendResult{};
    }
    e::ResidentStats resident;
    auto frame=session.prepared->frame(camera,request.width,request.height,opt,false,true,true,nullptr,&resident);
    if(!frame.ok){result.reason=QString::fromStdString(frame.error).toStdWString();return result;}
    auto plan=e::planGpuBuild(frame,scene,rt.objects.size(),16,true);
    if(!plan.ok){result.reason=QString::fromStdString(plan.error).toStdWString();return result;}
    plan.native.capturePacket=false;
    m3d::EscargotBuildTiming timing;
    if(request.directPresent){
        result.ok=m3d::renderLumaTraceEscargotGpuBuiltNative(scene,rt.lumaTraceBvh4,shadow,camera,
            request.width,request.height,reinterpret_cast<void*>(request.nativeHandle),plan.native,result.stats,timing,result.reason);
    }else{
        result.ok=m3d::renderLumaTraceEscargotGpuBuiltPreview(scene,rt.lumaTraceBvh4,shadow,camera,
            request.width,request.height,plan.native,result.pixels,result.stats,timing,result.reason);
    }
    if(result.ok&&(result.stats.zReflectActive||(request.directPresent?
        result.stats.readbackBufferCount!=0:result.pixels.size()!=size_t(request.width)*size_t(request.height)))){
        result.ok=false;
        result.reason=L"Escargot: unexpected secondary camera or incomplete frame.";
    }
    result.stats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(result.ok)result.renderer=request.directPresent?
        QStringLiteral("V17 LumaTrace BVH4 — Z Escargot G F16 — Direct GPU (swapchain)"):
        QStringLiteral("V17 LumaTrace BVH4 — Z Escargot G F16 — Qt (QImage)");
    if(result.ok&&!objectFailure.isEmpty())result.renderer+=QStringLiteral(" — secours Object 004 : ")+objectFailure;
    return result;
}
} // namespace
#endif

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
    }
#ifdef _WIN32
    else if(request.escargot){
        if(!request.directPresent)m3d::releaseDirectX11NativeForCurrentThread();
        result=renderEscargotF16(request,camera,shadow);
        if(!result.ok){
            const QString opticalError=QString::fromStdWString(result.reason);
            result.reason.clear();
            // Optical failure must not change presentation. First recover on
            // the same native swapchain using the approved Z Reflect path.
            if(request.directPresent){
                result.ok=m3d::renderLumaTraceNative(scene,snapshot.lumaTraceBvh4,shadow,camera,
                    request.width,request.height,reinterpret_cast<void*>(request.nativeHandle),
                    result.stats,result.reason);
                if(result.ok){
                    result.renderer=QStringLiteral("V17 LumaTrace BVH4 — Z Reflect (secours Escargot : %1) — Direct GPU (swapchain)").arg(opticalError);
                    return result;
                }
                const QString nativeError=QString::fromStdWString(result.reason);
                m3d::releaseDirectX11NativeForCurrentThread();
                Request preview=request;preview.directPresent=false;
                result=renderEscargotF16(preview,camera,shadow);
                result.nativeFailure=nativeError;
                if(result.ok){result.nativeFallback=true;return result;}
            }
            result.reason.clear();
            result.ok=m3d::renderLumaTracePreview(scene,snapshot.lumaTraceBvh4,shadow,camera,
                request.width,request.height,result.pixels,result.stats,result.reason);
            result.nativeFallback=request.directPresent&&result.ok;
            if(result.ok)result.renderer=QStringLiteral("V17 LumaTrace BVH4 — Z Reflect (secours Escargot : %1) — Qt (QImage)").arg(opticalError);
            else result.reason=L"Escargot: "+opticalError.toStdWString()+L"; Z Reflect: "+result.reason;
        }
    }
#endif
    else if(request.directPresent){
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
