#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"

#include <QCoreApplication>
#include <QImage>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Expected scene.json and output.png";return 2;}
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<"Scene load failed:"<<loaded.error;return 1;}
    auto built=m3d::v16::buildRuntime(loaded.scene,512);
    if(!built||built.runtime.lumaTraceBvh4.primitiveCount==0){
        qCritical()<<"LumaTrace BVH preparation failed:"<<built.error;return 1;
    }
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::Stats shadowStats;
    const auto shadow=m3d::buildShadow(built.runtime.renderScene,shadowStats);
    auto camera=std::find_if(built.runtime.cameras.cbegin(),built.runtime.cameras.cend(),
        [](const m3d::v16::CameraPreset& p){return p.id==QStringLiteral("v7_pool_fresco");});
    if(camera==built.runtime.cameras.cend()){
        qCritical()<<"The pool and fresco camera is missing from the scene";return 1;
    }
    constexpr int w=320,h=180;
    const auto cam=m3d::v16::makeCamera(*camera,w,h);
    m3d::setRasterHybridLightingEnabled(false);
    std::vector<uint32_t> direct,bounced,cached;
    m3d::Stats directStats,bounceStats,cacheStats;
    std::wstring error;
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cam,w,h,direct,directStats,error)){
        qCritical()<<"LumaTrace direct render failed:"<<QString::fromStdWString(error);return 1;
    }
    m3d::setRasterHybridLightingEnabled(true);
    error.clear();
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cam,w,h,bounced,bounceStats,error)){
        qCritical()<<"LumaTrace one-bounce render failed:"<<QString::fromStdWString(error);return 1;
    }
    error.clear();
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cam,w,h,cached,cacheStats,error)){
        qCritical()<<"Cached LumaTrace one-bounce render failed:"<<QString::fromStdWString(error);return 1;
    }
    size_t changed=0;
    for(size_t i=0;i<direct.size()&&i<bounced.size();++i)changed+=direct[i]!=bounced[i];
    if(!directStats.gpu||!bounceStats.gpu||direct.size()!=size_t(w*h)||
       bounced.size()!=size_t(w*h)||bounced!=cached||changed<8){
        qCritical()<<"LumaTrace bounce was absent, unstable, or not GPU rendered; changed pixels="<<changed;
        if(!error.empty())qCritical()<<QString::fromStdWString(error);
        return 1;
    }
    QImage image(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(image.scanLine(y),bounced.data()+size_t(y)*w,size_t(w)*sizeof(uint32_t));
    if(!image.save(QString::fromLocal8Bit(argv[2]),"PNG")){
        qCritical()<<"Could not save LumaTrace bounce image";return 1;
    }
    qInfo()<<"LumaTrace one-bounce WARP PASS; changed pixels="<<changed
           <<"; cached image bit-identical";
    m3d::setRasterHybridLightingEnabled(false);
    return 0;
}
