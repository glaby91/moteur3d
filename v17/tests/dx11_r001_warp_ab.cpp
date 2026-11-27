#include "m3d_engine.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <cstdlib>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);if(argc!=2)return 2;
    auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));if(!loaded){qCritical()<<loaded.error;return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);if(!built){qCritical()<<built.error;return 4;}
    auto& r=built.runtime;
    _putenv_s("M3D_V17_ANIMATE_CLOUDS","0");
    _putenv_s("M3D_V17_STATIC_GEOMETRY_PREP","1");
    _putenv_s("M3D_V17_PERIPHERAL_MULTIREZ","0");
    constexpr int w=320,h=180;
    for(float hour:{14.75f,0.f}){
        m3d::updateLighting(hour);m3d::Stats ss;const auto shadow=m3d::buildShadow(r.renderScene,ss);
        for(const auto& id:{"v7_pool_fresco","v4_face","v7_curtain_wall_detail","v8_stair_first_landing"}){
            auto it=std::find_if(r.cameras.begin(),r.cameras.end(),[&](const auto& c){return c.id==id;});
            if(it==r.cameras.end())return 5;
            const auto c=m3d::v16::makeCamera(*it,w,h);
            std::vector<uint32_t> a,b;std::wstring err;m3d::Stats sa,sb;
            _putenv_s("M3D_V17_DX11_R001","0");
            if(!m3d::renderDirectX11Preview(r.renderScene,shadow,c,w,h,a,sa,err)){qCritical()<<QString::fromStdWString(err);return 6;}
            _putenv_s("M3D_V17_DX11_R001","1");
            if(!m3d::renderDirectX11Preview(r.renderScene,shadow,c,w,h,b,sb,err)){qCritical()<<QString::fromStdWString(err);return 7;}
            if(a!=b||!sb.dxR001Enabled||sa.dxR001Enabled||sb.dxLightSubmittedCount>sa.dxLightSubmittedCount){
                qCritical()<<"DX11 R001 parity failed"<<id<<hour;return 8;
            }
            qInfo()<<"DX11 R001 exact parity"<<id<<hour<<"triangles"<<sa.tests<<sb.tests<<"lights"<<sa.dxLightSubmittedCount<<sb.dxLightSubmittedCount;
        }
    }
    return 0;
}
