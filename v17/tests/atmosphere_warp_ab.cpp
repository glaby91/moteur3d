#include "m3d_engine.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <algorithm>
#include <cstdlib>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(argc!=3)return 2;
    auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));if(!loaded){qCritical()<<loaded.error;return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);if(!built||!built.runtime.lumaTraceBvhError.isEmpty())return 4;
    auto& rt=built.runtime;
    const auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[](const auto& p){return p.id=="v7_pool_fresco";});
    if(it==rt.cameras.end())return 5;
    constexpr int w=320,h=180;auto cam=m3d::v16::makeCamera(*it,w,h);
    _putenv_s("M3D_V17_ANIMATE_CLOUDS","0");_putenv_s("M3D_V17_PERIPHERAL_MULTIREZ","0");
    _putenv_s("M3D_V17_ZATMOSPHERE_DENSITY",".012");
    m3d::setRasterHybridLightingEnabled(true);
    for(float hour:{14.75f,0.f}){
        m3d::updateLighting(hour);m3d::Stats ss;auto shadow=m3d::buildShadow(rt.renderScene,ss);
        for(bool luma:{false,true}){
            auto render=[&](bool on,std::vector<uint32_t>& pixels,m3d::Stats& stats){
                _putenv_s("M3D_V17_ZATMOSPHERE",on?"1":"0");std::wstring error;
                bool ok=luma?m3d::renderLumaTracePreview(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,w,h,pixels,stats,error):
                             m3d::renderDirectX11Preview(rt.renderScene,shadow,cam,w,h,pixels,stats,error);
                if(!ok)qCritical()<<QString::fromStdWString(error);
                return ok&&pixels.size()==size_t(w*h);
            };
            std::vector<uint32_t> a,b,c;m3d::Stats x,y,z;
            if(!render(false,a,x)||!render(true,b,y)||!render(false,c,z))return 6;
            if(a!=c||a==b||y.gpuDrawCalls!=x.gpuDrawCalls+2){qCritical()<<"Z Atmosphere A/B failed"<<luma<<hour<<x.gpuDrawCalls<<y.gpuDrawCalls;return 7;}
            QString stem=QString::fromLocal8Bit(argv[2])+"/atmosphere-"+(luma?"luma":"dx11")+(hour==0?"-night":"-day");
            if(!QImage(reinterpret_cast<const uchar*>(a.data()),w,h,w*4,QImage::Format_RGB32).save(stem+"-off.png")||
               !QImage(reinterpret_cast<const uchar*>(b.data()),w,h,w*4,QImage::Format_RGB32).save(stem+"-on.png"))return 8;
            qInfo()<<"Z Atmosphere OFF/ON/OFF parity OK"<<luma<<hour;
        }
    }
    return 0;
}
