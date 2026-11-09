#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QImage>
#include <QDebug>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <set>

static void logger(QtMsgType,const QMessageLogContext&,const QString& message) {
    const QByteArray utf8=message.toUtf8();
    std::fprintf(stderr,"V16 GI FHD: %s\n",utf8.constData());
    std::fflush(stderr);
}
int main(int argc,char** argv) {
    qInstallMessageHandler(logger);
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Usage: v16_gi_smoke scene.json output.png";return 2;}
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<loaded.error;return 1;}
    // The smoke uses 256 px mip bases but keeps the reference image Full HD.
    auto built=m3d::v16::buildRuntime(loaded.scene,256);
    if(!built||built.runtime.cameras.isEmpty()){qCritical()<<"Invalid scene/geometry";return 1;}
    const auto& scene=built.runtime.renderScene;
    m3d::updateLighting(built.runtime.daylightHour);
    constexpr int width=1920,height=1080;
    const auto camera=m3d::v16::makeCamera(built.runtime.cameras.front(),width,height);
    m3d::Stats shadowStats;
    const auto shadows=m3d::buildShadow(scene,shadowStats);
    std::vector<uint32_t> pixels;
    const std::atomic<uint64_t> generation{1};
    // One sample checks the exact full-resolution GI code path; selectable
    // interactive presets use 4/16 spp to reduce Monte-Carlo noise.
    const auto stats=m3d::render(scene,shadows,camera,width,height,pixels,
                                  4,false,generation,1,1,false,false,true,2);
    if(pixels.size()!=size_t(width)*height||stats.rays<100000){
        qCritical()<<"GI failed to trace the full image"<<stats.rays;return 1;
    }
    std::set<uint32_t> unique(pixels.begin(),pixels.end());
    if(unique.size()<128){qCritical()<<"GI image unexpectedly flat"<<unique.size();return 1;}
    QImage img(width,height,QImage::Format_ARGB32);
    for(int y=0;y<height;++y)
        std::memcpy(img.scanLine(y),pixels.data()+size_t(y)*width,size_t(width)*4);
    if(!img.save(QString::fromLocal8Bit(argv[2]),"PNG")){
        qCritical()<<"Could not save Full HD GI preview";return 1;
    }
    qInfo()<<"GI path tracing Full HD validated:"<<stats.ms<<"ms, rays:"<<stats.rays;
    return 0;
}
