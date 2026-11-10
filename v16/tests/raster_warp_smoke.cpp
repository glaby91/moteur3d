// Raster-first feasibility: measure existing hardware raster pipeline without
// BVH compute shading. Run WARP explicitly in CI; Intel GPU must be tested locally.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Usage: v16_raster_smoke scene.json image.png";return 2;}
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<loaded.error;return 1;}
    auto built=m3d::v16::buildRuntime(loaded.scene,512);
    if(!built||built.runtime.cameras.isEmpty()){
        qCritical()<<"Raster scene compilation failed";return 1;
    }
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::Stats shadowStats;
    const m3d::ShadowMap shadows=m3d::buildShadow(
        built.runtime.renderScene,shadowStats);
    constexpr int w=640,h=360;
    const auto cam=m3d::v16::makeCamera(built.runtime.cameras.front(),w,h);
    std::vector<uint32_t> cold,warm;
    m3d::Stats coldStats,warmStats;
    std::wstring error;
    if(!m3d::renderDirectX11Preview(
          built.runtime.renderScene,shadows,cam,w,h,cold,coldStats,error)){
        qCritical()<<"Raster cold failed"<<QString::fromStdWString(error);
        return 1;
    }
    error.clear();
    if(!m3d::renderDirectX11Preview(
          built.runtime.renderScene,shadows,cam,w,h,warm,warmStats,error)){
        qCritical()<<"Raster warm failed"<<QString::fromStdWString(error);
        return 1;
    }
    if(!coldStats.gpu||!warmStats.gpu||cold.size()!=size_t(w)*h||
       warm.size()!=cold.size()||cold!=warm){
        qCritical()<<"Raster output not deterministic or incomplete";return 1;
    }
    std::set<uint32_t> colors(cold.begin(),cold.end());
    if(colors.size()<128){
        qCritical()<<"Raster output unexpectedly flat"<<colors.size();return 1;
    }
    QImage image(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(image.scanLine(y),warm.data()+size_t(y)*w,size_t(w)*4);
    if(!image.save(QString::fromLocal8Bit(argv[2]),"PNG")){
        qCritical()<<"Failed to write raster snapshot";return 1;
    }
    qInfo()<<"RASTER DX11 BASELINE WARP | resolution"<<w<<"x"<<h
           <<"first_ms"<<coldStats.ms<<"cached_ms"<<warmStats.ms
           <<"triangles"<<warmStats.tests<<"colors"<<colors.size();
    // Do not assert a fixed ms limit in WARP: software timings cannot prove
    // 25-ms hardware viability.
    return 0;
}
