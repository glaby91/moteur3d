// V17: exercise the reused D3D11 raster renderer in CI (Microsoft WARP).
// This validates rendering, not the performance of Intel HD Graphics 620.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QString>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <set>
#include <vector>
static void logToStderr(QtMsgType,const QMessageLogContext&,const QString& message){
    const QByteArray utf8=message.toUtf8();
    std::fprintf(stderr,"V17 RASTER TEST: %s\\n",utf8.constData());
    std::fflush(stderr);
}
int main(int argc,char** argv){
    qInstallMessageHandler(logToStderr);
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Expected scene.json and output.png";return 2;}
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<loaded.error;return 1;}
    auto built=m3d::v16::buildRuntime(loaded.scene,512);
    if(!built || built.runtime.cameras.isEmpty()){qCritical()<<"No valid raster scene/camera";return 1;}
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::Stats shadowStats;
    const auto shadow=m3d::buildShadow(built.runtime.renderScene,shadowStats);
    constexpr int w=640,h=360;
    const auto cam=m3d::v16::makeCamera(built.runtime.cameras.front(),w,h);
    std::vector<uint32_t> first,second;
    m3d::Stats coldStats,warmStats;
    std::wstring error;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                   w,h,first,coldStats,error)){
        qCritical()<<"D3D11 raster failed:"<<QString::fromStdWString(error);
        return 1;
    }
    error.clear();
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                   w,h,second,warmStats,error)){
        qCritical()<<"Cached D3D11 raster failed:"<<QString::fromStdWString(error);
        return 1;
    }
    if(!coldStats.gpu || !warmStats.gpu || first.size()!=size_t(w)*size_t(h) ||
       first!=second || std::set<uint32_t>(first.begin(),first.end()).size()<64){
        qCritical()<<"Empty, incomplete or non-deterministic raster image";return 1;
    }
    // Regression for the V17 frustum optimization: switching culling OFF
    // must produce identical pixels. The renderer must not drop a triangle
    // that intersects the frustum or modify the underlying PBR resources.
    if(_putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","1")!=0){
        qCritical()<<"Cannot set culling A/B test variable";return 1;
    }
    std::vector<uint32_t> reference;
    m3d::Stats fullStats;
    error.clear();
    const bool fullOk=m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                                   w,h,reference,fullStats,error);
    _putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","");
    if(!fullOk||reference!=second){
        qCritical()<<"Frustum culling changed first camera pixels:"<<QString::fromStdWString(error);
        return 1;
    }
    if(built.runtime.cameras.size()>1){
        const auto alternate=m3d::v16::makeCamera(built.runtime.cameras.back(),w,h);
        std::vector<uint32_t> culledAlt,referenceAlt;
        m3d::Stats culledStats,fullAltStats;
        error.clear();
        if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,alternate,
                                        w,h,culledAlt,culledStats,error)){
            qCritical()<<"Second camera culling render failed:"<<QString::fromStdWString(error);
            return 1;
        }
        _putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","1");
        const bool altOk=m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,alternate,
                                                      w,h,referenceAlt,fullAltStats,error);
        _putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","");
        if(!altOk||culledAlt!=referenceAlt){
            qCritical()<<"Frustum culling changed alternate camera pixels:"<<QString::fromStdWString(error);
            return 1;
        }
        qInfo()<<"V17 A/B camera2: triangles submitted"<<qulonglong(culledStats.tests)
               <<"of"<<qulonglong(fullAltStats.tests);
    }
    qInfo()<<"V17 A/B camera1: triangles submitted"<<qulonglong(warmStats.tests)
           <<"of"<<qulonglong(fullStats.tests);
    // Compare box instancing only; ellipsoid LOD is deliberately unchanged
    // in the reference render and is independently testable.
    if(_putenv_s("M3D_V17_DISABLE_BOX_INSTANCING","1")!=0){
        qCritical()<<"Cannot disable instancing for A/B";return 1;
    }
    std::vector<uint32_t> legacy;
    m3d::Stats legacyStats;
    error.clear();
    const bool legacyOk=m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                                     w,h,legacy,legacyStats,error);
    _putenv_s("M3D_V17_DISABLE_BOX_INSTANCING","");
    if(!legacyOk||legacy.size()!=second.size()){
        qCritical()<<"Legacy box A/B rendering failed:"<<QString::fromStdWString(error);
        return 1;
    }
    size_t changed=0;uint64_t totalDelta=0;
    for(size_t i=0;i<legacy.size();++i){
        const uint32_t a=legacy[i],b=second[i];
        const int dr=std::abs(int((a>>16)&255)-int((b>>16)&255));
        const int dg=std::abs(int((a>>8)&255)-int((b>>8)&255));
        const int db=std::abs(int(a&255)-int(b&255));
        if(std::max({dr,dg,db})>8)++changed;
        totalDelta+=uint64_t(dr+dg+db);
    }
    const double fraction=double(changed)/legacy.size();
    const double meanDelta=double(totalDelta)/(legacy.size()*3);
    if(fraction>.03||meanDelta>2.0){
        qCritical()<<"GPU instancing changed scene appearance:"
                   <<"fraction"<<fraction<<"mean per-channel delta"<<meanDelta;
        return 1;
    }
    qInfo()<<"GPU box instancing A/B valid; fraction >8 difference:"<<fraction
           <<"mean delta:"<<meanDelta;
    // Public host-application quality API must be effective and reversible
    // on the same persistent worker, including MSAA target and sampler changes.
    const auto originalQuality=m3d::rasterQualitySettings();
    for(const auto profile:{m3d::RasterQualityProfile::Performance,
                            m3d::RasterQualityProfile::Ultra}){
        auto requested=m3d::rasterQualityProfile(profile);
        error.clear();
        if(!m3d::setRasterQualitySettings(requested,error)){
            qCritical()<<"GPU quality setter rejected valid preset"<<QString::fromStdWString(error);
            return 1;
        }
        const auto actual=m3d::rasterQualitySettings();
        if(actual.msaaSamples!=requested.msaaSamples||
           actual.anisotropy!=requested.anisotropy||
           actual.dynamicLod!=requested.dynamicLod){
            qCritical()<<"Host-side raster settings were not applied atomically";return 1;
        }
        std::vector<uint32_t> profileImage;
        m3d::Stats profileStats;
        if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                         w,h,profileImage,profileStats,error) ||
           profileImage.size()!=second.size() || !profileStats.gpu){
            qCritical()<<"Changed quality preset failed on WARP"
                       <<QString::fromStdWString(error);return 1;
        }
    }
    error.clear();
    if(!m3d::setRasterQualitySettings(originalQuality,error)){
        qCritical()<<"Cannot restore raster quality settings";return 1;
    }
    std::vector<uint32_t> restored;
    m3d::Stats restoredStats;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                   restored,restoredStats,error)||restored!=second){
        qCritical()<<"Restoring quality did not reproduce reference pixels"
                   <<QString::fromStdWString(error);return 1;
    }
    // New experimental pipeline: submit N frames before reading the oldest.
    // Every returned image must match the synchronous reference exactly,
    // even across staging-slot reuse. Timestamp queries are optional on
    // WARP/older drivers and must never be confused with CPU Map wait.
    for(unsigned depth:{2u,3u}){
        constexpr int frames=6;
        int submitted=0,received=0;
        for(int i=0;i<frames;++i){
            error.clear();
            if(!m3d::enqueueDirectX11Preview(built.runtime.renderScene,shadow,
                                             cam,w,h,depth,error)){
                qCritical()<<"Buffered GPU enqueue failed, depth"<<depth
                           <<QString::fromStdWString(error);return 1;
            }
            ++submitted;
            if(submitted-received>=int(depth)){
                std::vector<uint32_t> frame;
                m3d::Stats frameStats;
                if(!m3d::receiveDirectX11Preview(frame,frameStats,error) ||
                   frame!=second||frameStats.readbackBufferCount!=depth){
                    qCritical()<<"Buffered GPU pixels differ, depth"<<depth
                               <<QString::fromStdWString(error);return 1;
                }
                ++received;
            }
        }
        while(received<submitted){
            std::vector<uint32_t> frame;
            m3d::Stats frameStats;
            error.clear();
            if(!m3d::receiveDirectX11Preview(frame,frameStats,error)||
               frame!=second||frameStats.readbackBufferCount!=depth){
                qCritical()<<"Buffered GPU drain failed, depth"<<depth
                           <<QString::fromStdWString(error);return 1;
            }
            ++received;
        }
        qInfo()<<"V17 GPU readback depth"<<depth
               <<"identical to synchronous pixels across"<<frames<<"frames";
    }
    QImage png(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(png.scanLine(y),second.data()+size_t(y)*w,size_t(w)*4);
    if(!png.save(QString::fromLocal8Bit(argv[2]),"PNG")){
        qCritical()<<"Failed to write raster PNG";return 1;
    }
    qInfo()<<"V17 DX11 raster WARP OK, cached"<<warmStats.ms
           <<"ms; size"<<w<<"x"<<h;
    return 0;
}
