#include "m3d_engine.h"
#include "src/m3d_v16_bridge.h"
#include "src/m3d_zreflect_runtime.h"
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <cstdlib>
#include <algorithm>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(argc!=3)return 2;
    auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));if(!loaded){qCritical()<<loaded.error;return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);
    if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){qCritical()<<built.error<<built.runtime.lumaTraceBvhError;return 4;}
    auto& r=built.runtime;if(r.renderScene.opticalPlanes.size()!=4){qCritical()<<"Expected four authorized glass plans"<<r.renderScene.opticalPlanes.size();return 5;}
    const auto it=std::find_if(r.cameras.begin(),r.cameras.end(),[](const auto& c){return c.id=="v8_stair_first_landing";});
    if(it==r.cameras.end())return 6;
    constexpr int w=320,h=180;const auto c=m3d::v16::makeCamera(*it,w,h);
    m3d::zreflect::RuntimeOptics index;index.build(r.renderScene);std::vector<m3d::zreflect::ActivePlane> selected;
    index.select(r.renderScene,c,selected);if(selected.size()!=2){qCritical()<<"Expected two glass views"<<selected.size();return 7;}
    const auto& p=index.patch(selected[0].id);const auto mirror=m3d::zreflect::mirrorCamera(c,p.normal,p.offset);
    if(std::abs(m3d::zreflect::planeDistance(c.eye,p)+m3d::zreflect::planeDistance(mirror.eye,p))>.001)return 8;
    m3d::updateLighting(12.f);m3d::updateLighting(r.daylightHour);m3d::setRasterHybridLightingEnabled(true);
    m3d::Stats s;const auto shadow=m3d::buildShadow(r.renderScene,s);
    std::vector<uint32_t> off,on,again;std::wstring err;
    auto render=[&](bool enabled,std::vector<uint32_t>& pixels,m3d::Stats& stats){
        if(_putenv_s("M3D_V17_ZREFLECT",enabled?"1":"0"))return false;
        if(!m3d::renderLumaTracePreview(r.renderScene,r.lumaTraceBvh4,shadow,c,w,h,pixels,stats,err)){
            qCritical()<<QString::fromStdWString(err);return false;}
        return pixels.size()==size_t(w*h);
    };
    m3d::Stats a,b,d;
    if(!render(false,off,a)||!render(true,on,b)||!render(false,again,d))return 9;
    if(a.zReflectActive||b.zReflectActive!=2||b.zReflectDeferred||d.zReflectActive||off!=again||off==on){
        qCritical()<<"Z Reflect A/B failed"<<a.zReflectActive<<b.zReflectActive<<b.zReflectDeferred<<d.zReflectActive;return 10;}
    int changes=0;for(size_t i=0;i<off.size();++i)changes+=off[i]!=on[i];
    qInfo()<<"Z Reflect WARP A/B: active"<<b.zReflectActive<<"candidates"<<b.zReflectCandidates<<"changed pixels"<<changes;
    const QString out=QString::fromLocal8Bit(argv[2]);
    if(!QImage(reinterpret_cast<const uchar*>(off.data()),w,h,w*4,QImage::Format_RGB32).save(out+"/zreflect-off.png")||
       !QImage(reinterpret_cast<const uchar*>(on.data()),w,h,w*4,QImage::Format_RGB32).save(out+"/zreflect-on.png"))return 11;
    return 0;
}
