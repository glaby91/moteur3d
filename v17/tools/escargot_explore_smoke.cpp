#include "../moteur3d_v17_render.h"
#include "../escargot/footprint.h"
#include "../escargot/object_projected.h"
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>
#include <random>
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
    // Compiled C++ parity check against the actual legacy clipping function, with real camera axes.
    namespace proj=m3d::escargot::projected;
    using m3d::V;
    std::mt19937 rng(0x600d);std::uniform_real_distribution<float> spread(-30.f,30.f),local(-2.f,2.f),extent(.001f,3.f);
    std::uniform_int_distribution<int> sx(0,req.width-1),sy(0,req.height-1);
    uint64_t rejects=0,tests=0;
    for(int i=0;i<10000;++i){
        const int x=sx(rng),y=sy(rng);const m3d::escargot::Rect footprint{x,y,1+sx(rng)%(req.width-x),1+sy(rng)%(req.height-y)};
        const V center=cam.eye+V{spread(rng),spread(rng),spread(rng)};
        std::array<proj::Box,32> boxes;proj::Box united{{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()},{-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}};
        for(auto& b:boxes){const V lo=center+V{local(rng),local(rng),local(rng)};b={lo,lo+V{extent(rng),extent(rng),extent(rng)}};proj::extendBox(united,b);}
        const bool rejected=proj::outsideFootprint(united,cam,footprint,req.width,req.height);rejects+=rejected;
        for(const auto& b:boxes){
            const auto a=m3d::escargot::projectBounds(b.lo,b.hi,cam,req.width,req.height),f=proj::fastBounds(b,cam,req.width,req.height);++tests;
            if(a.valid()!=f.valid()||(a.valid()&&(a.x!=f.x||a.y!=f.y||a.w!=f.w||a.h!=f.h))||(rejected&&proj::intersection(a,footprint).valid())){
                qCritical()<<"006 footprint parity failure"<<i;return 33;
            }
        }
    }
    const proj::Box cameraBox{cam.eye-V{.001f,.001f,.001f},cam.eye+V{.001f,.001f,.001f}};
    if(proj::outsideFootprint(cameraBox,cam,{0,0,1,1},req.width,req.height))return 34;
    qInfo()<<"006 actual footprint parity"<<qulonglong(tests)<<"triangles, skipped groups"<<qulonglong(rejects);
    auto A=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!A.ok){qCritical()<<QString::fromStdWString(A.reason);return 8;}
    // The normal toggle disables glass only. Test the separate benchmark
    // override and restore the original process environment before A/F16/004/006.
    char priorZ[64]{};const DWORD priorLen=GetEnvironmentVariableA("M3D_V17_ZREFLECT",priorZ,DWORD(sizeof(priorZ)));
    if(priorLen>=sizeof(priorZ))return 35;
    if(!SetEnvironmentVariableA("M3D_V17_ZREFLECT","0")||
       !SetEnvironmentVariableA("CSE_WARP_ESCARGOT_PROFILE_BASE_OFF","1"))return 35;
    auto noOptics=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!SetEnvironmentVariableA("CSE_WARP_ESCARGOT_PROFILE_BASE_OFF",nullptr)||
       !SetEnvironmentVariableA("M3D_V17_ZREFLECT",priorLen?priorZ:nullptr))return 35;
    if(!noOptics.ok||noOptics.stats.zReflectActive||noOptics.stats.zReflectEnabled){
        qCritical()<<"006 base-off isolation failed"<<noOptics.stats.zReflectActive;return 35;
    }
    qInfo()<<"006 base-off isolation verified (including water)";
    req.escargot=true;
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","0");
    _putenv_s("CSE_WARP_ESCARGOT_LEGACY_F16","1");
    auto F=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!F.ok||F.stats.zReflectActive||!F.renderer.contains("Z Escargot G F16"))return 18;
    _putenv_s("CSE_WARP_ESCARGOT_LEGACY_F16","0");
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","1");
    auto G=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!G.ok||G.stats.zReflectActive||!G.renderer.contains(QStringLiteral("Z Escargot Object 004"))||G.pixels.size()!=A.pixels.size()){
        qCritical()<<"Object004 failure:"<<G.ok<<G.renderer<<QString::fromStdWString(G.reason)<<"zReflect"<<G.stats.zReflectActive<<"pixels"<<G.pixels.size();return 9;
    }
    if(A.pixels==G.pixels||F.pixels==G.pixels){qCritical()<<"Object004 image did not differ from A or F16";return 10;}
    // Prove an optical signal, not just a different frame from Z Reflect.
    // Both images have identical pipelines; only water eligibility changes.
    namespace e=m3d::escargot;
    m3d::EscargotPacketBuild emptyPlan,waterPlan;
    emptyPlan.rasterDirect=waterPlan.rasterDirect=true;
    emptyPlan.rasterReceivers.assign(size_t(rt.objects.size())+1,0);
    waterPlan.rasterReceivers=emptyPlan.rasterReceivers;
    for(const auto& c:e::staticCandidates(rt,*req.sceneDatabase,e::Mode::Reflection))
        if(e::waterMaterial(rt.renderScene.mat[c.material]))waterPlan.rasterReceivers[c.object]=1;
    std::vector<uint32_t> waterOff,waterOn;
    auto optical=[&](const m3d::EscargotPacketBuild& plan,std::vector<uint32_t>& pixels){
        m3d::Stats st;m3d::EscargotBuildTiming timing;std::wstring error;
        if(!m3d::renderLumaTraceEscargotGpuBuiltPreview(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,req.width,req.height,plan,pixels,st,timing,error)){
            qCritical()<<"Optical signal render failed"<<QString::fromStdWString(error);return false;
        }
        return pixels.size()==A.pixels.size()&&!st.zReflectActive;
    };
    if(!optical(emptyPlan,waterOff)||!optical(waterPlan,waterOn))return 22;
    uint64_t waterSignal=0,strongSignal=0;
    for(size_t j=0;j<waterOn.size();++j){
        const auto a=waterOn[j],b=waterOff[j];
        waterSignal+=(a&0xffffffu)!=(b&0xffffffu);
        int delta=0;for(int sh:{0,8,16})delta+=std::abs(int((a>>sh)&255)-int((b>>sh)&255));
        strongSignal+=delta>=12;
    }
    qInfo()<<"Water optical signal: changed pixels"<<qulonglong(waterSignal)<<"strong pixels"<<qulonglong(strongSignal);
    if(app.arguments().size()==3){
        QDir out(app.arguments().at(2));QDir().mkpath(out.absolutePath());
        QImage(reinterpret_cast<const uchar*>(waterOff.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath("water-off.png"));
        QImage(reinterpret_cast<const uchar*>(waterOn.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath("water-on.png"));
    }
    if(waterSignal<100||strongSignal<20){qCritical()<<"Water reflection missing or too weak";return 23;}
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","0");
    auto P=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!P.ok||P.stats.zReflectActive||!P.renderer.contains("Object 006")||P.stats.rays>e::projected::rayBudget||P.pixels.size()!=A.pixels.size()){
        qCritical()<<"Object006 failed"<<P.ok<<P.renderer<<QString::fromStdWString(P.reason)<<"rays"<<qulonglong(P.stats.rays);return 24;
    }
    e::projected::Session project(rt,*req.sceneDatabase);if(!project.valid())return 25;
    e::projected::Metrics pm;auto plan=project.build(cam,shadow,req.width,req.height,pm);
    if(pm.rays>e::projected::rayBudget||pm.triangles>e::projected::triangleBudget||!pm.triangles||!pm.receivers||!pm.water||
        plan.projectedVertices.size()!=size_t(pm.triangles)*30||plan.projectedRanges.size()!=pm.receivers){
        qCritical()<<"Object006 invalid planner metrics"<<pm.rays<<pm.triangles<<pm.receivers<<"catalogue"<<pm.catalogue<<"patches"<<pm.patches<<"visible"<<pm.visible<<"water"<<pm.water;return 26;
    }
    uint32_t end=0;
    for(const auto& r:plan.projectedRanges){
        if(r.first!=end||r.opaque%3||r.transparent%3||r.x<0||r.y<0||r.w<=0||r.h<=0||r.x+r.w>req.width||r.y+r.h>req.height){
            qCritical()<<"Object006 invalid projected layout"<<r.receiver;return 31;
        }
        end+=r.opaque+r.transparent;
    }
    if(size_t(end)*10!=plan.projectedVertices.size())return 32;
    auto empty=plan;empty.projectedKey=0;empty.projectedRanges.clear();empty.projectedVertices.clear();std::fill(empty.data.begin()+4,empty.data.end(),0.f);
    std::vector<uint32_t> projectedOff;
    if(!optical(empty,projectedOff))return 27;
    uint64_t projectedSignal=0;
    for(size_t j=0;j<P.pixels.size();++j){int d=0;for(int shift:{0,8,16})d+=std::abs(int((P.pixels[j]>>shift)&255)-int((projectedOff[j]>>shift)&255));projectedSignal+=d>=12;}
    // Same projected path, identical camera/light; cached geometry uses zero queries.
    auto again=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!again.ok||again.stats.rays||again.pixels!=P.pixels){qCritical()<<"Object006 cache mismatch"<<again.renderer<<qulonglong(again.stats.rays);return 28;}
        // A sky-only pass has the same receiver map, Fresnel, opacity and sky fill.
    // The sole difference from the on-plan is the projected triangle geometry.
    auto skyOnly=plan;skyOnly.projectedKey=0;skyOnly.projectedVertices.clear();
    for(auto& r:skyOnly.projectedRanges)r.first=r.opaque=r.transparent=0;
    // Limit the geometry-on/sky-only comparison specifically to the water.
    auto waterProjected=plan;waterProjected.projectedKey=0;
    for(size_t id=1;id<=size_t(rt.objects.size());++id){
        const auto& o=rt.objects[int(id)-1];
        if(o.triangleCount<=0||o.firstTriangle<0||size_t(o.firstTriangle)>=rt.renderScene.obj.size()||!e::waterMaterial(rt.renderScene.mat[rt.renderScene.obj[o.firstTriangle].mat])){
            waterProjected.data[4*(id+1)]=0;skyOnly.data[4*(id+1)]=0;
        }
    }
    std::vector<uint32_t> geometryOn,geometrySky;
    if(!optical(waterProjected,geometryOn)||!optical(skyOnly,geometrySky))return 30;
    uint64_t geometrySignal=0;
    for(size_t j=0;j<geometryOn.size();++j){
        int d=0;for(int shift:{0,8,16})d+=std::abs(int((geometryOn[j]>>shift)&255)-int((geometrySky[j]>>shift)&255));
        geometrySignal+=d>=12;
    }
    qInfo()<<"Projected geometry signal on water:"<<qulonglong(geometrySignal)<<"rays"<<pm.rays<<"triangles"<<pm.triangles<<"omitted"<<pm.omitted;
    if(app.arguments().size()==3){
        QDir out(app.arguments().at(2));QDir().mkpath(out.absolutePath());
        QImage(reinterpret_cast<const uchar*>(geometryOn.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath("Object006-water-geometry.png"));
        QImage(reinterpret_cast<const uchar*>(geometrySky.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath("Object006-water-sky-only.png"));
        QImage(reinterpret_cast<const uchar*>(P.pixels.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath("Object006.png"));
    }
    if(projectedSignal<20||geometrySignal<20){qCritical()<<"Object006 missing geometric signal"<<qulonglong(projectedSignal)<<qulonglong(geometrySignal);return 29;}
    auto pose=*it;pose.eye.x+=.075f;pose.target.x+=.075f;
    auto moved=m3d::v16::makeCamera(pose,req.width,req.height);
    auto M=cse::v17render::renderLumaTrace(req,moved,shadow);
    if(!M.ok||M.stats.zReflectActive||!M.renderer.contains(QStringLiteral("Z Escargot Object 006"))||M.pixels==P.pixels)return 11;
    req.escargot=false;
    auto restored=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!restored.ok||restored.pixels!=A.pixels)return 12;
    qInfo()<<"Escargot exploration integration: A / F16 / Object004 / Object006 / moved Object006 / A restore OK";
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
    const auto nativeG=[&](const auto& v){return nativeOK(v)&&!v.stats.zReflectActive&&v.renderer.contains(QStringLiteral("Z Escargot Object 006"))&&v.renderer.contains(QStringLiteral("swapchain"));};
    if(!nativeOK(nativeA)){qCritical()<<nativeA.nativeFailure<<QString::fromStdWString(nativeA.reason);return 14;}
    req.escargot=true;
    auto nativeF=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!nativeG(nativeF)){qCritical()<<nativeF.renderer<<nativeF.nativeFailure<<QString::fromStdWString(nativeF.reason);return 15;}
    auto nativeMoved=cse::v17render::renderLumaTrace(req,moved,shadow);
    if(!nativeG(nativeMoved))return 16;
    req.escargot=false;
    auto nativeRestore=cse::v17render::renderLumaTrace(req,cam,shadow);
    if(!nativeOK(nativeRestore)||nativeRestore.stats.zReflectActive!=nativeA.stats.zReflectActive)return 17;
    qInfo()<<"Native swapchain A / Object006 / moved Object006 / A restored, zero image readback";
    if(app.arguments().size()==3){
        QDir out(app.arguments().at(2));if(!QDir().mkpath(out.absolutePath()))return 19;
        auto save=[&](const auto& r,const QString& name){return QImage(reinterpret_cast<const uchar*>(r.pixels.data()),req.width,req.height,req.width*4,QImage::Format_RGB32).save(out.filePath(name));};
        if(!save(A,"A.png")||!save(F,"F16.png")||!save(G,"Object004.png")||!save(P,"Object006.png")||!save(M,"Object006-moved.png"))return 20;
        QJsonObject report{{"A_ms",double(A.stats.ms)},{"F16_ms",double(F.stats.ms)},{"Object004_ms",double(G.stats.ms)},{"native_A_submit_ms",double(nativeA.stats.ms)},{"native_Object006_submit_ms",double(nativeF.stats.ms)},{"native_readback",int(nativeF.stats.readbackBufferCount)},{"Object006_ms",double(P.stats.ms)},{"Object006_cached_ms",double(again.stats.ms)},{"Object006_moved_ms",double(M.stats.ms)},{"Object006_catalogue",double(pm.catalogue)},{"Object006_patches",double(pm.patches)},{"Object006_visible_patches",double(pm.visible)},{"Object006_water_receivers",double(pm.water)},{"Object006_rays",double(pm.rays)},{"Object006_triangles",double(pm.triangles)},{"Object006_omitted",double(pm.omitted)},{"Object006_signal_pixels",double(projectedSignal)},{"Object006_water_geometry_signal_pixels",double(geometrySignal)},{"water_signal_pixels",double(waterSignal)},{"water_strong_signal_pixels",double(strongSignal)},{"note","Single warm/cold smoke timings on software WARP, not hardware FPS. Pixel output differs from A and F16; movement and A restoration verified, not proof of visual parity."}};
        QFile f(out.filePath("object005-summary.json"));if(!f.open(QIODevice::WriteOnly))return 21;f.write(QJsonDocument(report).toJson());
    }
    return 0;
}
