#include "../moteur3d_v17_render.h"
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(app.arguments().size()!=2)return 2;
    auto loaded=m3d::v16::loadScene(app.arguments().at(1));
    if(!loaded){qCritical()<<loaded.error;return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);
    if(!built){qCritical()<<built.error;return 4;}
    cse::v17render::Request req;
    req.scene=std::make_shared<m3d::v16::Runtime>(std::move(built.runtime));
    req.sceneDatabase=std::make_shared<const m3d::v16::SceneDatabase>(loaded.scene);
    if(!cse::v17render::prepareEscargotScene(*req.scene))return 5;
    req.width=320;req.height=180;req.backend=6;
    auto& rt=*req.scene;
    auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[](const auto& p){return p.id==QStringLiteral("v7_pool_fresco");});
    if(it==rt.cameras.end())return 6;
    m3d::setRasterPeripheralMultiresOverride(0);m3d::setRasterCloudsEnabled(false);
    m3d::setRasterHybridLightingEnabled(true);
    m3d::setRasterColorEnergyBalanceEnabled(true);m3d::setRasterFilmicTonemapEnabled(true);m3d::setRasterColorExposure(.85f);
    auto quality=m3d::rasterQualityProfile(m3d::RasterQualityProfile::High);
    quality.msaaSamples=1;quality.dynamicLod=false;
    std::wstring error;if(!m3d::setRasterQualitySettings(quality,error))return 7;
    m3d::updateLighting(rt.daylightHour);m3d::Stats shadowStats;
    auto shadow=m3d::buildShadow(rt.renderScene,shadowStats);
    const auto cam=m3d::v16::makeCamera(*it,req.width,req.height);
    auto A=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!A.ok){qCritical()<<QString::fromStdWString(A.reason);return 8;}
    req.escargot=true;
    auto G=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!G.ok||G.stats.zReflectActive||!G.renderer.contains(QStringLiteral("Z Escargot G F16"))||G.pixels.size()!=A.pixels.size()){
        qCritical()<<G.renderer<<QString::fromStdWString(G.reason);return 9;
    }
    if(A.pixels==G.pixels)return 10;
    auto pose=*it;pose.eye.x+=.075f;pose.target.x+=.075f;
    auto moved=m3d::v16::makeCamera(pose,req.width,req.height);
    auto M=cse::v17render::renderLumaTrace(req,moved,shadow);
    if(!M.ok||M.stats.zReflectActive||!M.renderer.contains(QStringLiteral("Z Escargot G F16"))||M.pixels==G.pixels)return 11;
    req.escargot=false;
    auto restored=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!restored.ok||restored.pixels!=A.pixels)return 12;
    qInfo()<<"Escargot exploration integration: A / F16 / moved F16 / A restore OK";
    return 0;
}
