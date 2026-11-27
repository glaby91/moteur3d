#include "m3d_engine.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <cstdlib>
#include <vector>
#include <utility>
#include <cstdint>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(argc!=3)return 2;
    auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));if(!loaded){qCritical()<<loaded.error;return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);
    if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){qCritical()<<built.error<<built.runtime.lumaTraceBvhError;return 4;}
    auto& rt=built.runtime;auto& s=rt.renderScene;
    std::vector<std::pair<size_t,float>> lamps;
    for(size_t i=0;i<s.mat.size();++i){
        const auto& m=s.mat[i];
        if(m.name=="v633_opal_rib"||m.name=="v633_opal_groove"){
            if(!m.emissionAuthored||m.emissionIntensity<=0)return 5;
            lamps.emplace_back(i,m.emissionIntensity);
        }
    }
    if(lamps.size()!=2)return 6;
    _putenv_s("M3D_V17_ANIMATE_CLOUDS","0");
    _putenv_s("M3D_V17_PERIPHERAL_MULTIREZ","0");
    _putenv_s("M3D_V17_ZREFLECT","0");
    m3d::updateLighting(0.f);m3d::setRasterHybridLightingEnabled(true);
    m3d::Stats ss;auto shadow=m3d::buildShadow(s,ss);
    constexpr int w=320,h=180;
    m3d::v16::CameraPreset p;p.eye={-45.8f,3.5f,-48.f};p.target={-48.82f,3.5f,-48.f};p.fov=55.f;
    auto cam=m3d::v16::makeCamera(p,w,h);
    for(bool luma:{false,true}){
        std::vector<uint32_t> off,on;std::wstring err;m3d::Stats a,b;
        auto render=[&](bool enabled,std::vector<uint32_t>& pixels,m3d::Stats& stats){
            for(auto [i,v]:lamps)s.mat[i].emissionIntensity=enabled?v:0.f;
            s.markGpuParametersChanged();
            bool ok=luma?m3d::renderLumaTracePreview(s,rt.lumaTraceBvh4,shadow,cam,w,h,pixels,stats,err):
                m3d::renderDirectX11Preview(s,shadow,cam,w,h,pixels,stats,err);
            if(!ok)qCritical()<<QString::fromStdWString(err);
            return ok&&pixels.size()==size_t(w*h);
        };
        if(!render(false,off,a)||!render(true,on,b))return 7;
        auto rgb=[](uint32_t c){return int((c>>16)&255)+int((c>>8)&255)+int(c&255);};
        size_t brighter=0;int64_t delta=0;
        // Lamp lies on camera axis. Check central region, not sky or other objects.
        for(int y=10;y<h-10;++y)for(int x=w/2-20;x<w/2+20;++x){
            auto i=size_t(y*w+x);int diff=rgb(on[i])-rgb(off[i]);brighter+=diff>10;delta+=diff;
        }
        if(brighter<20||delta<=0){qCritical()<<"Authored emission not visible"<<luma<<brighter<<delta;return 8;}
        QString prefix=QString::fromLocal8Bit(argv[2])+(luma?"/emission-luma":"/emission-dx11");
        if(!QImage(reinterpret_cast<const uchar*>(off.data()),w,h,w*4,QImage::Format_RGB32).save(prefix+"-off.png")||
           !QImage(reinterpret_cast<const uchar*>(on.data()),w,h,w*4,QImage::Format_RGB32).save(prefix+"-on.png"))return 9;
        qInfo()<<"V17.633 authored emission night A/B"<<luma<<"brighter pixels"<<brighter<<"sum delta"<<delta;
    }
    return 0;
}
