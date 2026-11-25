#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QDebug>
#include <QString>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
    // Match the GUI, which points GI at <scene>.m3dgi. LumaTrace must still
    // build and use its own live GPU one-bounce field if that file exists.
    const QString scenePath=QString::fromLocal8Bit(argv[1]);
    m3d::setRasterGiCachePath(std::filesystem::path(
        (scenePath+QStringLiteral(".m3dgi")).toStdWString()));
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::Stats shadowStats;
    const auto shadow=m3d::buildShadow(built.runtime.renderScene,shadowStats);
    auto camera=std::find_if(built.runtime.cameras.cbegin(),built.runtime.cameras.cend(),
        [](const m3d::v16::CameraPreset& p){return p.id==QStringLiteral("v7_pool_fresco");});
    if(camera==built.runtime.cameras.cend()){
        qCritical()<<"The pool and fresco camera is missing from the scene";return 1;
    }
    constexpr int w=1920,h=1080;
    auto poolView=*camera;
    poolView.target.y=6.8f; // Include a useful water area while retaining the fresco.
    const auto cam=m3d::v16::makeCamera(poolView,w,h);
    m3d::setRasterHybridLightingEnabled(false);
    std::vector<uint32_t> direct,bounced,cached;
    m3d::Stats directStats,bounceStats,cacheStats;
    std::wstring error;
    std::fprintf(stderr,"[reflection-smoke] direct begin\\n");std::fflush(stderr);
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cam,w,h,direct,directStats,error)){
        qCritical()<<"LumaTrace direct render failed:"<<QString::fromStdWString(error);return 1;
    }
    std::fprintf(stderr,"[reflection-smoke] direct complete\\n");std::fflush(stderr);
    m3d::setRasterHybridLightingEnabled(true);
    error.clear();
    std::fprintf(stderr,"[reflection-smoke] bounce begin\\n");std::fflush(stderr);
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cam,w,h,bounced,bounceStats,error)){
        qCritical()<<"LumaTrace one-bounce render failed:"<<QString::fromStdWString(error);return 1;
    }
    std::fprintf(stderr,"[reflection-smoke] bounce complete; cache begin\\n");std::fflush(stderr);
    error.clear();
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cam,w,h,cached,cacheStats,error)){
        qCritical()<<"Cached LumaTrace one-bounce render failed:"<<QString::fromStdWString(error);return 1;
    }
    std::fprintf(stderr,"[reflection-smoke] cache complete\\n");std::fflush(stderr);
    auto cupola=std::find_if(built.runtime.cameras.cbegin(),built.runtime.cameras.cend(),
        [](const m3d::v16::CameraPreset& p){return p.id==QStringLiteral("v523_cupola_exterieure");});
    if(cupola==built.runtime.cameras.cend()){
        qCritical()<<"The cupola camera is missing from the scene";return 1;
    }
    const auto cupolaCam=m3d::v16::makeCamera(*cupola,w,h);
    std::vector<uint32_t> cupolaPixels;
    m3d::Stats cupolaStats;
    std::fprintf(stderr,"[reflection-smoke] cupola begin\\n");std::fflush(stderr);
    error.clear();
    if(!m3d::renderLumaTracePreview(built.runtime.renderScene,built.runtime.lumaTraceBvh4,
                                    shadow,cupolaCam,w,h,cupolaPixels,cupolaStats,error)){
        qCritical()<<"LumaTrace cupola reflection render failed:"<<QString::fromStdWString(error);return 1;
    }
    std::fprintf(stderr,"[reflection-smoke] cupola complete\\n");std::fflush(stderr);
    size_t changed=0;
    for(size_t i=0;i<direct.size()&&i<bounced.size();++i)changed+=direct[i]!=bounced[i];
    // The lower part of this pool/fresco camera is the water surface.
    // Measure actual water pixels, not the mural or deck, and reject the
    // barely measurable lift accepted by the original smoke threshold.
    double poolDirect=0.0,poolBounce=0.0;
    double poolDeltaR=0.0,poolDeltaG=0.0,poolDeltaB=0.0,poolAbsDelta=0.0;
    size_t poolPixels=0,poolBrighter=0;
    if(direct.size()==size_t(w*h)&&bounced.size()==size_t(w*h))
    for(int y=h*7/10;y<h;++y)for(int x=w/40;x<w-w/40;++x){
        const size_t i=size_t(y)*w+size_t(x);
        const QRgb a=qRgb((direct[i]>>16)&255,(direct[i]>>8)&255,direct[i]&255);
        const QRgb b=qRgb((bounced[i]>>16)&255,(bounced[i]>>8)&255,bounced[i]&255);
        const int da=qGray(a),db=qGray(b);
        poolDirect+=da;poolBounce+=db;poolBrighter+=db>da;++poolPixels;
        const int dr=qRed(b)-qRed(a),dg=qGreen(b)-qGreen(a),dbb=qBlue(b)-qBlue(a);
        poolDeltaR+=dr;poolDeltaG+=dg;poolDeltaB+=dbb;
        poolAbsDelta+=std::abs(dr)+std::abs(dg)+std::abs(dbb);
    }
    const double poolLift=poolPixels?(poolBounce-poolDirect)/double(poolPixels):0.0;
    if(!directStats.gpu||!bounceStats.gpu||direct.size()!=size_t(w*h)||
       bounced.size()!=size_t(w*h)||bounced!=cached||changed<8||poolLift<6.0||
       poolBrighter<poolPixels/2){
        qCritical()<<"LumaTrace bounce failed its pool-region check; changed pixels="<<changed
                   <<"; pool mean lift="<<poolLift<<"/255; brighter pool pixels="<<poolBrighter;
        if(!error.empty())qCritical()<<QString::fromStdWString(error);
        return 1;
    }
    QImage directImage(w,h,QImage::Format_ARGB32),bounceImage(w,h,QImage::Format_ARGB32),
        deltaImage(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y){
        auto* directRow=reinterpret_cast<QRgb*>(directImage.scanLine(y));
        auto* bounceRow=reinterpret_cast<QRgb*>(bounceImage.scanLine(y));
        auto* deltaRow=reinterpret_cast<QRgb*>(deltaImage.scanLine(y));
        for(int x=0;x<w;++x){
            const size_t i=size_t(y)*w+size_t(x);
            const QRgb a=qRgb((direct[i]>>16)&255,(direct[i]>>8)&255,direct[i]&255);
            const QRgb b=qRgb((bounced[i]>>16)&255,(bounced[i]>>8)&255,bounced[i]&255);
            directRow[x]=a;bounceRow[x]=b;
            deltaRow[x]=qRgb(std::min(255,std::max(0,128+(qRed(b)-qRed(a))*8)),
                             std::min(255,std::max(0,128+(qGreen(b)-qGreen(a))*8)),
                             std::min(255,std::max(0,128+(qBlue(b)-qBlue(a))*8)));
        }
    }
    const QString output=QString::fromLocal8Bit(argv[2]);
    const QFileInfo outputInfo(output);
    const QString stem=outputInfo.path()+QDir::separator()+outputInfo.completeBaseName();
    QImage cupolaImage(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y){
        auto* row=reinterpret_cast<QRgb*>(cupolaImage.scanLine(y));
        for(int x=0;x<w;++x){
            const size_t i=size_t(y)*w+size_t(x);
            row[x]=qRgb((cupolaPixels[i]>>16)&255,(cupolaPixels[i]>>8)&255,cupolaPixels[i]&255);
        }
    }
    const bool saved=directImage.save(stem+QStringLiteral("-direct.png"),"PNG")&&
        bounceImage.save(output,"PNG")&&deltaImage.save(stem+QStringLiteral("-difference-x8.png"),"PNG")&&
        cupolaImage.save(stem+QStringLiteral("-cupola.png"),"PNG");
    if(!saved){qCritical()<<"Could not save LumaTrace comparison images";return 1;}
    std::printf("LumaTrace WARP comparison: changed=%zu/%zu; pool grayscale lift=%.3f/255; ",
        changed,direct.size(),poolLift);
    std::printf("pool mean RGB delta=(%.3f, %.3f, %.3f); mean absolute RGB delta=%.3f/channel; ",
        poolPixels?poolDeltaR/double(poolPixels):0.0,
        poolPixels?poolDeltaG/double(poolPixels):0.0,
        poolPixels?poolDeltaB/double(poolPixels):0.0,
        poolPixels?poolAbsDelta/(3.0*double(poolPixels)):0.0);
    std::printf("brighter pool pixels=%zu/%zu; cache bit-identical=%s\n",
        poolBrighter,poolPixels,bounced==cached?"yes":"no");
    std::fflush(stdout);
    m3d::setRasterHybridLightingEnabled(false);
    return 0;
}
