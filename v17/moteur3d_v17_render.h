#pragma once

#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"

#include <QImage>
#include <QString>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace cse::v17render {

struct Result {
    QImage image;
    v16::CameraPreset cameraPose;
    QString renderer;
    QString error;
    double milliseconds=0.0;
    bool gpu=false;
    bool skyCompositing=false;
    bool directPresented=false;
    bool nativeFallback=false;
    QString nativeFailure;
    QString nativeWindowDetails;
    QString requestStamp;
    uint64_t generation=0;
    int backendIndex=-1;
    quintptr renderThread=0;
};

struct CpuShadowCache {
    std::mutex mutex;
    std::weak_ptr<v16::Runtime> scene;
    std::shared_ptr<const m3d::ShadowMap> shadow;
    float hour=-1.0f;
};

struct Request {
    std::shared_ptr<v16::Runtime> scene;
    std::shared_ptr<CpuShadowCache> shadowCache;
    std::shared_ptr<std::atomic<uint64_t>> serial;
    uint64_t generation=0;
    v16::CameraPreset cameraPose;
    int width=0;
    int height=0;
    int backend=0;
    bool directPresent=false;
    quintptr nativeHandle=0;
    void* sharedVulkanInstance=nullptr;
    uintptr_t sharedVulkanSurface=0;
    bool edgeAA=false;
    bool progressive=false;
    bool denoise=false;
    bool denoiseDirect=false;
    unsigned sampleCount=1;
    unsigned maxBounces=4;
    float renderHour=14.75f;
    float shadowHour=14.75f;
    QString nativeWindowDetails;
    QString requestStamp;
    std::function<void(QImage,unsigned,unsigned,int,int)> publishProgressiveFrame;
};

struct BackendResult {
    QImage image;
    QString renderer;
    double milliseconds=0.0;
    std::vector<uint32_t> pixels;
    m3d::Stats stats;
    std::wstring reason;
    QString nativeFailure;
    bool ok=false;
    bool nativeFallback=false;
    bool cancelled=false;
};

BackendResult renderD3D11Raster(const Request&,const m3d::Camera&,const m3d::ShadowMap&);
BackendResult renderVulkanRaster(const Request&,const m3d::Camera&,const m3d::ShadowMap&);
BackendResult renderLumaTrace(const Request&,const m3d::Camera&,const m3d::ShadowMap&);
BackendResult renderD3D11Compute(const Request&,const m3d::Camera&);
BackendResult renderCpu(const Request&,const m3d::Camera&,const m3d::ShadowMap&,bool shadowReused);

std::shared_ptr<const m3d::ShadowMap> getOrBuildShadow(
    const Request&,double* buildMilliseconds=nullptr,bool profileRasterBuild=false);
Result execute(const Request&);

} // namespace cse::v17render
