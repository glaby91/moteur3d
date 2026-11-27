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
    auto& r=built.runtime;if(r.renderScene.opticalPlanes.size()!=5){qCritical()<<"Expected five authorized optical planes including water"<<r.renderScene.opticalPlanes.size();return 5;}
    const auto it=std::find_if(r.cameras.begin(),r.cameras.end(),[](const auto& c){return c.id=="v8_stair_first_landing";});
    if(it==r.cameras.end())return 6;
    constexpr int w=320,h=180;const auto c=m3d::v16::makeCamera(*it,w,h);
    m3d::zreflect::RuntimeOptics index;index.build(r.renderScene);std::vector<m3d::zreflect::ActivePlane> selected;
    index.select(r.renderScene,c,selected);if(selected.size()!=2){qCritical()<<"Expected two glass views"<<selected.size();return 7;}
    std::vector<m3d::zreflect::ActivePlane> waterOnly;
    index.select(r.renderScene,c,waterOnly,false);
    const auto pool=std::find_if(r.cameras.begin(),r.cameras.end(),[](const auto& v){return v.id=="v7_pool_fresco";});
    if(pool==r.cameras.end())return 12;
    std::vector<m3d::zreflect::ActivePlane> poolSelection;
    const auto poolCamera=m3d::v16::makeCamera(*pool,w,h);
    index.select(r.renderScene,poolCamera,poolSelection,false);
    if(poolSelection.empty()||!index.water(poolSelection.front().id)){qCritical()<<"Visible pool plane not selected";return 13;}
    // Check exact clipping against the original vector implementation for all authored views.
    for(const auto& preset:r.cameras){
        const auto view=m3d::v16::makeCamera(preset,w,h);
        for(const auto& plane:r.renderScene.opticalPlanes)for(auto id:plane.triangles){
            const auto& tri=r.renderScene.obj[id];
            if(m3d::zreflect::projectedTriangle(tri,view)!=m3d::zreflect::projectedTriangleReference(tri,view)){
                qCritical()<<"Projection parity failed"<<preset.id<<id;return 16;}
        }
        std::vector<m3d::zreflect::ActivePlane> one,two;index.select(r.renderScene,view,one);index.select(r.renderScene,view,two);
        if(one.size()!=two.size())return 17;
        for(size_t j=0;j<one.size();++j)if(one[j].id!=two[j].id||one[j].projected!=two[j].projected)return 17;
    }
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
    if(a.zReflectActive!=waterOnly.size()||b.zReflectActive!=2||b.zReflectDeferred||d.zReflectActive!=a.zReflectActive||off!=again||off==on){
        qCritical()<<"Z Reflect A/B failed"<<a.zReflectActive<<b.zReflectActive<<b.zReflectDeferred<<d.zReflectActive;return 10;}
    int changes=0;for(size_t i=0;i<off.size();++i)changes+=off[i]!=on[i];
    qInfo()<<"Z Reflect WARP A/B: active"<<b.zReflectActive<<"candidates"<<b.zReflectCandidates<<"changed pixels"<<changes;
    const QString out=QString::fromLocal8Bit(argv[2]);
    if(!QImage(reinterpret_cast<const uchar*>(off.data()),w,h,w*4,QImage::Format_RGB32).save(out+"/zreflect-off.png")||
       !QImage(reinterpret_cast<const uchar*>(on.data()),w,h,w*4,QImage::Format_RGB32).save(out+"/zreflect-on.png"))return 11;
    // Production default: exactly the explicit ON image, without any launch script.
    std::vector<uint32_t> defaultOn;m3d::Stats defaultStats;
    if(_putenv_s("M3D_V17_ZREFLECT","")||
       !m3d::renderLumaTracePreview(r.renderScene,r.lumaTraceBvh4,shadow,c,w,h,defaultOn,defaultStats,err)||
       defaultOn!=on||!defaultStats.zReflectEnabled||defaultStats.zReflectActive!=b.zReflectActive){
        qCritical()<<"Default Z Reflect ON regression";return 26;
    }
    // Water through the shared target must be rendered even with glazing OFF.
    std::vector<uint32_t> waterPixels;m3d::Stats waterStats;
    if(_putenv_s("M3D_V17_ZREFLECT","0")||!m3d::renderLumaTracePreview(r.renderScene,r.lumaTraceBvh4,shadow,poolCamera,w,h,waterPixels,waterStats,err)||
       !waterStats.planarReflectionRendered||waterStats.zReflectActive!=1||waterStats.zReflectTotal!=5)return 14;
    if(!QImage(reinterpret_cast<const uchar*>(waterPixels.data()),w,h,w*4,QImage::Format_RGB32).save(out+"/zreflect-unified-water.png"))return 15;
    // Pixel-exact regression gate for the new depth/culling pass.
    for(const auto& name:{"v7_pool_fresco","v4_face","v7_curtain_wall_detail","v8_stair_first_landing"}){
        const auto v=std::find_if(r.cameras.begin(),r.cameras.end(),[&](const auto& p){return p.id==name;});
        if(v==r.cameras.end())return 18;
        const auto view=m3d::v16::makeCamera(*v,w,h);
        std::vector<uint32_t> ref,opt;m3d::Stats x,y;
        _putenv_s("M3D_V17_ZREFLECT","1");
        _putenv_s("M3D_V17_ZREFLECT_CPU_CULL","0");_putenv_s("M3D_V17_ZREFLECT_PREPASS","0");
        if(!m3d::renderLumaTracePreview(r.renderScene,r.lumaTraceBvh4,shadow,view,w,h,ref,x,err))return 19;
        for(bool cull:{false,true})for(bool depth:{false,true}){
            _putenv_s("M3D_V17_ZREFLECT_CPU_CULL",cull?"1":"0");_putenv_s("M3D_V17_ZREFLECT_PREPASS",depth?"1":"0");
            if(!m3d::renderLumaTracePreview(r.renderScene,r.lumaTraceBvh4,shadow,view,w,h,opt,y,err))return 20;
            if(ref!=opt||x.zReflectActive!=y.zReflectActive){qCritical()<<"Reflection matrix pixel parity failed"<<name<<cull<<depth;return 21;}
            for(uint32_t i=0;i<y.zReflectActive&&i<2;++i){
                const auto& a=x.zReflectPasses[i];const auto& b=y.zReflectPasses[i];
                if(a.planeId!=b.planeId||b.cpuCull!=cull||b.depthPrepass!=depth||
                   (!depth&&b.depthTriangles!=0)||(!cull&&a.colorTriangles!=b.colorTriangles)||
                   b.colorTriangles>a.colorTriangles){qCritical()<<"Reflection matrix counters failed"<<name<<i;return 22;}
            }
        }
        // Unset explicit options: the production default must be C, not B.
        if(_putenv_s("M3D_V17_ZREFLECT_CPU_CULL","")||
           _putenv_s("M3D_V17_ZREFLECT_PREPASS","")||
           _putenv_s("M3D_V17_DISABLE_ZREFLECT_DEPTH_OPT",""))return 23;
        if(!m3d::renderLumaTracePreview(r.renderScene,r.lumaTraceBvh4,shadow,view,w,h,opt,y,err)||
           ref!=opt||x.zReflectActive!=y.zReflectActive)return 24;
        for(uint32_t i=0;i<y.zReflectActive&&i<2;++i){
            const auto& p=y.zReflectPasses[i];
            if(!p.cpuCull||p.depthPrepass||p.depthTriangles!=0){
                qCritical()<<"Reflection default C failed"<<name<<i;return 25;
            }
        }
        qInfo()<<"Reflection pixel parity and default C OK"<<name;
    }
    qInfo()<<"Unified water pass active/total"<<waterStats.zReflectActive<<waterStats.zReflectTotal;
    return 0;
}
