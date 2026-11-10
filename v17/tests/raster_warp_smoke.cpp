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
