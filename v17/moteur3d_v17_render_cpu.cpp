#include "moteur3d_v17_render.h"

#include <algorithm>
#include <cstring>
#include <thread>
#include <utility>

namespace cse::v17render {

BackendResult renderCpu(const Request& request,const m3d::Camera& camera,
                        const m3d::ShadowMap& shadow,bool shadowReused){
    BackendResult result;
    const auto& scene=request.scene->renderScene;
    const unsigned workers=std::max(1u,std::min(8u,std::thread::hardware_concurrency()));

    if(request.backend==2||request.backend==3){
        std::vector<m3d::V> hdrAccum;
        unsigned completed=0;
        double totalMs=0.0;
        while(completed<request.sampleCount){
            if(request.serial->load()!=request.generation){
                result.cancelled=true;
                return result;
            }
            const unsigned batch=request.progressive
                ?std::min(std::max(1u,completed),request.sampleCount-completed)
                :request.sampleCount-completed;
            const auto pass=m3d::renderGiAccumulatePass(scene,shadow,camera,
                request.width,request.height,hdrAccum,result.pixels,completed,batch,workers,
                *request.serial,request.generation,request.maxBounces);
            if(request.serial->load()!=request.generation){
                result.cancelled=true;
                return result;
            }
            completed+=batch;
            totalMs+=pass.ms;
            std::vector<uint32_t> display=result.pixels;
            if(request.denoise&&completed>=4)
                display=m3d::denoiseBilateral(display,request.width,request.height,workers);
            if(request.edgeAA)
                display=m3d::antialiasEdges4(display,request.width,request.height,4,false);
            QImage image(request.width,request.height,QImage::Format_ARGB32);
            for(int y=0;y<request.height;++y)
                std::memcpy(image.scanLine(y),display.data()+size_t(y)*request.width,
                            size_t(request.width)*4);
            if(request.progressive&&completed<request.sampleCount){
                if(request.publishProgressiveFrame)
                    request.publishProgressiveFrame(std::move(image),completed,
                        request.sampleCount,request.width,request.height);
            }else{
                result.image=std::move(image);
            }
        }
        result.renderer=QStringLiteral("CPU GI — %1 rebonds, %2 spp, HDR%3 — %4×%5")
            .arg(request.maxBounces).arg(request.sampleCount)
            .arg(request.denoise?QStringLiteral(", débruité"):QString())
            .arg(request.width).arg(request.height);
        result.milliseconds=totalMs;
        result.ok=true;
        return result;
    }

    result.pixels.resize(size_t(request.width)*size_t(request.height));
    result.stats=m3d::render(scene,shadow,camera,request.width,request.height,result.pixels,
        workers,false,*request.serial,request.generation,request.sampleCount,
        false,false,false,3);
    if(request.serial->load()!=request.generation){
        result.cancelled=true;
        return result;
    }
    if(request.denoiseDirect)
        result.pixels=m3d::denoiseBilateral(result.pixels,request.width,request.height,workers);
    result.renderer=shadowReused
        ?QStringLiteral("CPU Ray Tracing — %1 spp, ombres en cache").arg(request.sampleCount)
        :QStringLiteral("CPU Ray Tracing — %1 spp, ombres calculées").arg(request.sampleCount);
    result.ok=true;
    return result;
}

} // namespace cse::v17render
