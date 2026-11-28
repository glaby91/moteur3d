#include "../moteur3d_v17_render.h"
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <QDir>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(app.arguments().size()<2||app.arguments().size()>3)return 2;
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){
        const auto utf8=message.toUtf8();std::fprintf(stderr,"%s\n",utf8.constData());std::fflush(stderr);
    });
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","1");
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
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","0");
    auto F=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!F.ok||F.stats.zReflectActive||!F.renderer.contains("Z Escargot G F16"))return 18;
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","1");
    auto G=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!G.ok||G.stats.zReflectActive||!G.renderer.contains(QStringLiteral("Z Escargot Object 004"))||G.pixels.size()!=A.pixels.size()){
        qCritical()<<"Object004 failure:"<<G.ok<<G.renderer<<QString::fromStdWString(G.reason)<<"zReflect"<<G.stats.zReflectActive<<"pixels"<<G.pixels.size();return 9;
    }
    if(A.pixels==G.pixels||F.pixels==G.pixels){qCritical()<<"Object004 image did not differ from A or F16";return 10;}
    auto pose=*it;pose.eye.x+=.075f;pose.target.x+=.075f;
    auto moved=m3d::v16::makeCamera(pose,req.width,req.height);
    auto M=cse::v17render::renderLumaTrace(req,moved,shadow);
    if(!M.ok||M.stats.zReflectActive||!M.renderer.contains(QStringLiteral("Z Escargot Object 004"))||M.pixels==G.pixels)return 11;
    req.escargot=false;
    auto restored=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!restored.ok||restored.pixels!=A.pixels)return 12;
    qInfo()<<"Escargot exploration integration: A / F16 / Object004 / moved Object004 / A restore OK";
    // Same real HWND for both modes; no full-frame staging or QImage fallback.
    RECT bounds{0,0,req.width,req.height};
    AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"Escargot native integration",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,
        nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!hwnd){qCritical()<<"Cannot create native test HWND"<<GetLastError();return 13;}
    struct WindowGuard{HWND hwnd;~WindowGuard(){m3d::releaseDirectX11NativeForCurrentThread();DestroyWindow(hwnd);}} guard{hwnd};
    req.nativeHandle=reinterpret_cast<quintptr>(hwnd);req.directPresent=true;
    auto nativeA=cse::v17render::renderLumaTrace(req,cam,shadow);
    const auto nativeOK=[](const auto& v){return v.ok&&!v.nativeFallback&&v.nativeFailure.isEmpty()&&v.pixels.empty()&&v.stats.readbackBufferCount==0;};
    const auto nativeG=[&](const auto& v){return nativeOK(v)&&!v.stats.zReflectActive&&v.renderer.contains(QStringLiteral("Z Escargot Object 004"))&&v.renderer.contains(QStringLiteral("swapchain"));};
    if(!nativeOK(nativeA)){qCritical()<<nativeA.nativeFailure<<QString::fromStdWString(nativeA.reason);return 14;}
    req.escargot=true;
    auto nativeF=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!nativeG(nativeF)){qCritical()<<nativeF.renderer<<nativeF.nativeFailure<<QString::fromStdWString(nativeF.reason);return 15;}
    auto nativeMoved=cse::v17render::renderLumaTrace(req,moved,shadow);
    if(!nativeG(nativeMoved))return 16;
    req.escargot=false;
    auto nativeRestore=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!nativeOK(nativeRestore)||nativeRestore.stats.zReflectActive!=nativeA.stats.zReflectActive)return 17;
    qInfo()<<"Native swapchain A / Object004 / moved Object004 / A restored, zero image readback";
    if(app.arguments().size()==3){
        QDir out(app.arguments().at(2));if(!QDir().mkpath(out.absolutePath()))return 19;
        auto save=[&](const auto& r,const QString& name){return QImage(reinterpret_cast<const uchar*>(r.pixels.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath(name));};
        if(!save(A,"A.png")||!save(F,"F16.png")||!save(G,"Object004.png")||!save(M,"Object004-moved.png"))return 20;
        QJsonObject report{{"A_ms",double(A.stats.ms)},{"F16_ms",double(F.stats.ms)},{"Object004_ms",double(G.stats.ms)},{"Object004_moved_ms",double(M.stats.ms)},{"native_A_submit_ms",double(nativeA.stats.ms)},{"native_Object004_submit_ms",double(nativeF.stats.ms)},{"native_readback",int(nativeF.stats.readbackBufferCount)},{"note","Single warm/cold smoke timings on software WARP, not hardware FPS. Pixel output differs from A and F16; movement and A restoration verified, not proof of visual parity."}};
        QFile f(out.filePath("object004-summary.json"));if(!f.open(QIODevice::WriteOnly))return 21;f.write(QJsonDocument(report).toJson());
    }
    return 0;
}
