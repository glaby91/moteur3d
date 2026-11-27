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
    _putenv_s("M3D_V17_ZATMOSPHERE_DENSITY",".002");
    unsigned changedFrames=0;
    m3d::setRasterHybridLightingEnabled(true);
    for(float hour:{14.75f,0.f}){
        m3d::updateLighting(hour);m3d::Stats ss;auto shadow=m3d::buildShadow(rt.renderScene,ss);
        for(bool luma:{false,true})for(float offset:{0.f,.05f,.20f}){
            cam=m3d::v16::makeCamera(*it,w,h);cam.eye=cam.eye+cam.r*offset;
            auto render=[&](bool on,std::vector<uint32_t>& pixels,m3d::Stats& stats){
                _putenv_s("M3D_V17_ZATMOSPHERE",on?"1":"0");std::wstring error;
                bool ok=luma?m3d::renderLumaTracePreview(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,w,h,pixels,stats,error):
                             m3d::renderDirectX11Preview(rt.renderScene,shadow,cam,w,h,pixels,stats,error);
                if(!ok)qCritical()<<QString::fromStdWString(error);
                return ok&&pixels.size()==size_t(w*h);
            };
            std::vector<uint32_t> a,b,c;m3d::Stats x,y,z;
            if(!render(false,a,x)||!render(true,b,y)||!render(false,c,z))return 6;
            if(a!=c||y.gpuDrawCalls!=x.gpuDrawCalls+1){qCritical()<<"Z Atmosphere A/B failed"<<luma<<hour<<x.gpuDrawCalls<<y.gpuDrawCalls;return 7;}
            unsigned changed=0,maxDelta=0;double totalDelta=0;
            for(size_t i=0;i<a.size();++i){
                if(a[i]!=b[i])++changed;
                for(unsigned shift:{0u,8u,16u}){
                    int delta=int((b[i]>>shift)&255u)-int((a[i]>>shift)&255u);
                    if(delta<0||delta>3){qCritical()<<"Grain non additif ou trop fort"<<delta<<luma<<hour<<offset;return 9;}
                    maxDelta=std::max(maxDelta,unsigned(delta));totalDelta+=delta;
                }
            }
            if(changed)++changedFrames;
            if(totalDelta/(a.size()*3)>.12){qCritical()<<"Voile lumineux excessif";return 10;}
            qInfo()<<"Grain: pixels modifies, delta maximal, moyenne"<<changed<<maxDelta<<totalDelta/(a.size()*3);
            QString stem=QString::fromLocal8Bit(argv[2])+"/atmosphere-"+(luma?"luma":"dx11")+(hour==0?"-night":"-day")+"-"+QString::number(offset);
            if(!QImage(reinterpret_cast<const uchar*>(a.data()),w,h,w*4,QImage::Format_RGB32).save(stem+"-off.png")||
               !QImage(reinterpret_cast<const uchar*>(b.data()),w,h,w*4,QImage::Format_RGB32).save(stem+"-on.png"))return 8;
            qInfo()<<"Z Atmosphere OFF/ON/OFF parity OK"<<luma<<hour;
        }
    }
    if(!changedFrames){qCritical()<<"Grain invisible apres quantification sur tous les essais";return 11;}
    return 0;
}
