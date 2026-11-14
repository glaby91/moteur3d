#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QImage>
#include <QFileInfo>
#include <QDebug>
#include <cstdio>
#include <atomic>
#include <cstring>
#include <set>

static void ciLog(QtMsgType, const QMessageLogContext&, const QString& message) {
    const QByteArray utf8 = message.toUtf8();
    std::fprintf(stderr, "V16 TEST: %s\n", utf8.constData());
    std::fflush(stderr);
}
int main(int argc,char** argv){
    qInstallMessageHandler(ciLog);
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Usage: v16_render_smoke scene.json output.png";return 2;}
    auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<loaded.error;return 1;}
    qInfo()<<"Manifest loaded";
    auto built=m3d::v16::buildRuntime(loaded.scene,128);
    if(!built){qCritical()<<built.error;return 1;}
    if(built.runtime.cameras.isEmpty()){qCritical()<<"No cameras";return 1;}
    // This check only applies when the independent material test scene
    // defines the brick sample: mortar must have REAL normal-map relief.
    if(built.runtime.materialIndex.contains(QStringLiteral("brick_clay"))){
        const int mi=built.runtime.materialIndex.value(QStringLiteral("brick_clay"));
        const auto& m=built.runtime.renderScene.mat[size_t(mi)];
        if(m.normalMip.empty()){qCritical()<<"Missing reusable brick normal map";return 1;}
        const auto& values=m.normalMip.front().p;
        std::set<uint16_t> distinct(values.begin(),values.end());
        if(distinct.size()<12){qCritical()<<"Brick joint normals are too flat:"<<distinct.size();return 1;}
        qInfo()<<"Brick mortar bump map verified:"<<distinct.size()<<"normal values";
    }
    qInfo()<<"Geometry BVH and mips ready";
    const int w=1920,h=1080; // Full HD reference screenshot, never a low-resolution preview
    const auto cam=m3d::v16::makeCamera(built.runtime.cameras.front(),w,h);
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::Stats shadows;
    qInfo()<<"Building shadow map";
    const auto shadow=m3d::buildShadow(built.runtime.renderScene,shadows);
    qInfo()<<"Shadow map ready";
    std::vector<uint32_t> pixels(size_t(w)*size_t(h));
    std::atomic<uint64_t> generation{1};
    qInfo()<<"Starting CPU render";
    const auto stats=m3d::render(built.runtime.renderScene,shadow,cam,w,h,pixels,4,false,generation,1,1,false,false,false,3);
    qInfo()<<"CPU render returned";
    if(pixels.size()!=size_t(w)*h){qCritical()<<"Incomplete render";return 1;}
    std::set<uint32_t> colors;
    for(uint32_t x:pixels)colors.insert(x);
    if(colors.size()<128){qCritical()<<"Render has too little color variation:"<<colors.size();return 1;}
    QImage image(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;y++)std::memcpy(image.scanLine(y),pixels.data()+size_t(y)*w,w*4);
    if(!image.save(QString::fromLocal8Bit(argv[2]),"PNG")){
        qCritical()<<"Could not write PNG";return 1;
    }
    // Same geometry/normal maps under a lower, warmer late-afternoon sun:
    // the relief must change with light, without adding any triangles.
    m3d::updateLighting(17.f);
    m3d::Stats lateShadowStats;
    const auto lateShadow=m3d::buildShadow(built.runtime.renderScene,lateShadowStats);
    std::vector<uint32_t> latePixels;
    const auto lateStats=m3d::render(built.runtime.renderScene,lateShadow,cam,w,h,
        latePixels,4,false,generation,1,1,false,false,false,3);
    if(latePixels.size()!=pixels.size()||latePixels==pixels){
        qCritical()<<"Daylight 17:00 must change CPU Full HD pixels";return 1;
    }
    QImage eveningImage(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(eveningImage.scanLine(y),latePixels.data()+size_t(y)*w,w*4);
    const QFileInfo target(QString::fromLocal8Bit(argv[2]));
    const QString eveningPath=target.absolutePath()+"/"+target.completeBaseName()+"-17h.png";
    if(!eveningImage.save(eveningPath,"PNG")){
        qCritical()<<"Could not save Full HD grazing daylight preview";return 1;
    }
    m3d::updateLighting(built.runtime.daylightHour);
    qInfo()<<"V16 CPU rendered:"<<w<<"x"<<h<<"pixels;"<<colors.size()
           <<"unique colors;"<<stats.ms<<"ms;"<<shadows.rays<<"shadow rays";
    qInfo()<<"V16 low sun CPU Full HD rendered:"<<lateStats.ms<<"ms";
    return 0;
}
