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
    // Critical parity control: GPU WARP smoke uses buildRuntime(scene, 512).
    // Both renderers MUST receive PBR generated at the identical base size:
    // procedural colors/normals/roughness are not invariant to texture resolution.
    // The final image remains native 1920x1080; this is material resolution.
    constexpr int referencePbrResolution=512;
    auto built=m3d::v16::buildRuntime(loaded.scene,referencePbrResolution);
    if(!built||built.runtime.cameras.isEmpty()){qCritical()<<"Invalid scene/geometry";return 1;}
    const auto& scene=built.runtime.renderScene;
    m3d::updateLighting(built.runtime.daylightHour);
    constexpr int width=1920,height=1080;
    const auto camera=m3d::v16::makeCamera(built.runtime.cameras.front(),width,height);
    m3d::Stats shadowStats;
    const auto shadows=m3d::buildShadow(scene,shadowStats);
    std::vector<uint32_t> pixels;
    std::vector<m3d::V> hdr;
    const std::atomic<uint64_t> generation{1};
    // The CI reference is now genuinely accumulated 4 spp Full HD plus an
    // optional bilateral denoise, rather than a deliberately noisy 1 spp PNG.
    const auto first=m3d::renderGiAccumulatePass(scene,shadows,camera,width,height,
        hdr,pixels,0,1,4,generation,1,2);
    const auto second=m3d::renderGiAccumulatePass(scene,shadows,camera,width,height,
        hdr,pixels,1,1,4,generation,1,2);
    const auto third=m3d::renderGiAccumulatePass(scene,shadows,camera,width,height,
        hdr,pixels,2,2,4,generation,1,2);
    if(pixels.size()!=size_t(width)*height||hdr.size()!=pixels.size()||
       first.rays+second.rays+third.rays<100000){
        qCritical()<<"GI did not trace the complete Full HD image";return 1;
    }
    // Progressive batching and single-batch accumulation must produce
    // bit-identical pixels. Test this efficiently on a separate small image;
    // all actual reference images still use native Full HD.
    constexpr int sw=256,sh=144;
    const auto smallCam=m3d::v16::makeCamera(built.runtime.cameras.front(),sw,sh);
    std::vector<m3d::V> incrementalHdr,singleHdr;
    std::vector<uint32_t> incremental,single;
    m3d::renderGiAccumulatePass(scene,shadows,smallCam,sw,sh,
        incrementalHdr,incremental,0,1,4,generation,1,2);
    m3d::renderGiAccumulatePass(scene,shadows,smallCam,sw,sh,
        incrementalHdr,incremental,1,3,4,generation,1,2);
    m3d::renderGiAccumulatePass(scene,shadows,smallCam,sw,sh,
        singleHdr,single,0,4,4,generation,1,2);
    if(incremental!=single){
        qCritical()<<"GI progressive accumulation changes pixels compared to a single batch";
        return 1;
    }
    auto clean=m3d::denoiseBilateral(pixels,width,height,4);
    std::set<uint32_t> unique(clean.begin(),clean.end());
    if(unique.size()<128||clean==pixels){
        qCritical()<<"GI denoise failed or image unexpectedly flat";return 1;
    }
    QImage img(width,height,QImage::Format_ARGB32);
    for(int y=0;y<height;++y)
        std::memcpy(img.scanLine(y),clean.data()+size_t(y)*width,size_t(width)*4);
    if(!img.save(QString::fromLocal8Bit(argv[2]),"PNG")){
        qCritical()<<"Could not save Full HD GI preview";return 1;
    }
    qInfo()<<"GI progressive Full HD 4 spp + denoise PASS: first"<<first.ms
           <<"ms, second"<<second.ms<<"ms, third"<<third.ms<<"ms";
    qInfo()<<"GI single versus 2-batch progressive: pixel-identical PASS.";
    return 0;
}
