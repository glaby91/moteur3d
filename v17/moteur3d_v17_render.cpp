#include "moteur3d_v17_render.h"

#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QThread>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace cse::v17render {

std::shared_ptr<const m3d::ShadowMap> getOrBuildShadow(
    const Request& request,double* buildMilliseconds,bool profileRasterBuild){
    if(buildMilliseconds)*buildMilliseconds=0.0;
    if(!request.shadowCache||!request.scene)return {};
    auto& cache=*request.shadowCache;
    std::lock_guard<std::mutex> lock(cache.mutex);
    if(cache.scene.lock()==request.scene&&cache.hour==request.shadowHour&&cache.shadow)
        return cache.shadow;

    m3d::Stats shadowStats;
    const auto start=std::chrono::steady_clock::now();
    auto shadow=std::make_shared<m3d::ShadowMap>(
        m3d::buildShadow(request.scene->renderScene,shadowStats));
    const double elapsed=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-start).count();
    if(buildMilliseconds)*buildMilliseconds=elapsed;

    if(profileRasterBuild){
        const QByteArray profilePath=qgetenv("CSE_WARP_VK_SHADOW_PROFILE");
        if(!profilePath.isEmpty()){
            QFile log(QString::fromLocal8Bit(profilePath));
            if(log.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){
                const QString backend=request.backend==5
                    ?QStringLiteral("Vulkan"):QStringLiteral("DX11");
                const QString line=QStringLiteral(
                    "%1,shadow_build_ms=%2,solar_hour=%3,shadow_bin=%4,backend=%5\n")
                    .arg(QDateTime::currentMSecsSinceEpoch())
                    .arg(elapsed,0,'f',3)
                    .arg(request.renderHour,0,'f',5)
                    .arg(request.shadowHour,0,'f',2)
                    .arg(backend);
                log.write(line.toUtf8());
            }
        }
    }
    cache.scene=request.scene;
    cache.hour=request.shadowHour;
    cache.shadow=shadow;
    return shadow;
}

Result execute(const Request& request){
    Result result;
    result.requestStamp=request.requestStamp;
    result.cameraPose=request.cameraPose;
    result.generation=request.generation;
    result.backendIndex=request.backend;
    result.nativeWindowDetails=request.nativeWindowDetails;
    result.skyCompositing=request.backend>=4&&request.backend<=6;
    result.renderThread=reinterpret_cast<quintptr>(QThread::currentThreadId());
#if defined(_WIN32)
    if(!result.nativeWindowDetails.isEmpty())
        result.nativeWindowDetails+=QStringLiteral(" renderThreadWin32=%1 renderThreadQt=0x%2")
            .arg(GetCurrentThreadId())
            .arg(QString::number(qulonglong(result.renderThread),16));
#endif
    if(!request.scene||!request.serial||request.width<=0||request.height<=0){
        result.error=QStringLiteral("Paramètres de rendu incomplets.");
        return result;
    }

    m3d::updateLighting(request.renderHour);
    const auto camera=v16::makeCamera(request.cameraPose,request.width,request.height);
    double shadowBuildMs=0.0;
    bool ok=false;
    m3d::Stats stats;
    std::vector<uint32_t> pixels;

    const bool wantGpu=request.backend==1||request.backend==4||
                       request.backend==5||request.backend==6;
    const bool raster=request.backend>=4&&request.backend<=6;
    const bool globalIllumination=request.backend==2||request.backend==3;
    const bool vulkan=request.backend==5;
    const bool lumaTrace=request.backend==6;

    if(wantGpu){
        BackendResult backend;
        if(raster){
            const auto shadow=getOrBuildShadow(request,&shadowBuildMs,true);
            if(!shadow){
                result.error=QStringLiteral("Impossible de préparer les ombres de la scène.");
                return result;
            }
            if(lumaTrace)backend=renderLumaTrace(request,camera,*shadow);
            else if(vulkan)backend=renderVulkanRaster(request,camera,*shadow);
            else backend=renderD3D11Raster(request,camera,*shadow);

            ok=backend.ok;
            stats=backend.stats;
            pixels=std::move(backend.pixels);
            result.nativeFailure=std::move(backend.nativeFailure);
            result.nativeFallback=backend.nativeFallback;
            result.directPresented=ok&&request.directPresent&&!result.nativeFallback;
            if(ok){
                result.renderer=lumaTrace
                    ?(result.directPresented
                        ?QStringLiteral("V17 LumaTrace BVH4 — Direct GPU (swapchain)")
                        :QStringLiteral("V17 LumaTrace BVH4 — affichage Qt (QImage)"))
                    :vulkan
                    ?(result.directPresented
                        ?QStringLiteral("V17 Vulkan Raster — Direct GPU (swapchain)")
                        :QStringLiteral("V17 Vulkan Raster — affichage Qt (QImage)"))
                    :(result.directPresented
                        ?QStringLiteral("V17 D3D11 Raster — Direct GPU (swapchain)")
                        :QStringLiteral("V17 D3D11 Raster — affichage Qt (QImage)"));
                if(!vulkan&&!lumaTrace&&m3d::rasterHybridLightingEnabled())
                    result.renderer+=m3d::rasterGiCacheLoaded()
                        ?QStringLiteral(" — GI transport pré-calculée")
                        :QStringLiteral(" — sondes approximatives (sans table GI)");
            }
        }else{
            backend=renderD3D11Compute(request,camera);
            ok=backend.ok;
            stats=backend.stats;
            pixels=std::move(backend.pixels);
            result.renderer=std::move(backend.renderer);
        }

        if(!ok){
            result.error=QStringLiteral(
                "Échec du rendu GPU %1 : %2. Aucun basculement automatique vers le CPU.")
                .arg(lumaTrace?QStringLiteral("LumaTrace BVH4"):
                     (vulkan?QStringLiteral("Vulkan"):QStringLiteral("DirectX11")))
                .arg(QString::fromStdWString(backend.reason));
            result.renderer=lumaTrace?QStringLiteral("V17 LumaTrace BVH4 — erreur"):
                (vulkan?QStringLiteral("V17 Vulkan — erreur")
                       :QStringLiteral("V17 GPU D3D11 — erreur"));
            return result;
        }
    }

    if(!wantGpu){
        m3d::releaseDirectX11NativeForCurrentThread();
        m3d::releaseVulkanDirectForCurrentThread();
        m3d::releaseVulkanPreviewForCurrentThread();
    }

    if(!ok){
        m3d::releaseDirectX11NativeForCurrentThread();
        bool shadowReused=false;
        if(request.shadowCache){
            std::lock_guard<std::mutex> lock(request.shadowCache->mutex);
            shadowReused=request.shadowCache->scene.lock()==request.scene&&
                request.shadowCache->hour==request.shadowHour&&request.shadowCache->shadow;
        }
        const auto shadow=getOrBuildShadow(request,nullptr,false);
        if(!shadow){
            result.error=QStringLiteral("Impossible de préparer les ombres de la scène.");
            return result;
        }
        BackendResult cpu=renderCpu(request,camera,*shadow,shadowReused);
        if(cpu.cancelled||request.serial->load()!=request.generation)return result;
        if(globalIllumination){
            result.image=std::move(cpu.image);
            result.renderer=std::move(cpu.renderer);
            result.milliseconds=cpu.milliseconds;
            return result;
        }
        stats=cpu.stats;
        pixels=std::move(cpu.pixels);
        result.renderer=std::move(cpu.renderer);
    }else{
        result.gpu=true;
    }

    result.milliseconds=stats.ms+shadowBuildMs;
    result.renderer+=QStringLiteral(" — %1×%2").arg(request.width).arg(request.height);
    if(result.nativeFallback){
        result.renderer+=vulkan
            ?QStringLiteral(" — secours image Vulkan via QImage (WSI indisponible)")
            :QStringLiteral(" — secours QImage (DXGI indisponible)");
        if(!result.nativeFailure.isEmpty())
            result.renderer+=QStringLiteral(" — échec présentation native : %1")
                .arg(result.nativeFailure);
    }
    if(result.directPresented){
        result.renderer+=QStringLiteral(" — écran GPU direct (swapchain)");
        return result;
    }
    if(request.edgeAA&&!raster&&pixels.size()==size_t(request.width)*size_t(request.height))
        pixels=m3d::antialiasEdges4(pixels,request.width,request.height,4,false);
    if(pixels.size()!=size_t(request.width)*size_t(request.height)){
        result.error=QStringLiteral("Image incomplète");
        return result;
    }
    QImage image(request.width,request.height,QImage::Format_ARGB32);
    for(int y=0;y<request.height;++y)
        std::memcpy(image.scanLine(y),pixels.data()+size_t(y)*request.width,
                    size_t(request.width)*4);
    result.image=std::move(image);
    return result;
}

} // namespace cse::v17render
