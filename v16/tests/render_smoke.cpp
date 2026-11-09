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
    qInfo()<<"V16 CPU rendered:"<<w<<"x"<<h<<"pixels;"<<colors.size()
           <<"unique colors;"<<stats.ms<<"ms;"<<shadows.rays<<"shadow rays";
    return 0;
}
