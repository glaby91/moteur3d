#include "../moteur3d_v17_render.h"
#include "../escargot/object_projected.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImage>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
namespace e=m3d::escargot;
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()<3)return 2;
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext&,const QString& message){
        const auto utf8=message.toUtf8();std::fprintf(stderr,"%s\n",utf8.constData());std::fflush(stderr);
    });
    _putenv_s("CSE_WARP_ESCARGOT_PROFILE","1");
    _putenv_s("CSE_WARP_ESCARGOT_OBJECT_GPU","0");_putenv_s("CSE_WARP_ESCARGOT_LEGACY_F16","0");
    _putenv_s("M3D_V17_ZATMOSPHERE","0");
    _putenv_s("CSE_WARP_DX_BENCH_PRESENT_INTERVAL","0");
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 3;
    QFile report(out.filePath("profile-frames.jsonl"));if(!report.open(QIODevice::WriteOnly|QIODevice::Truncate))return 4;
    auto write=[&](QJsonObject o){report.write(QJsonDocument(o).toJson(QJsonDocument::Compact));report.write("\n");report.flush();};
    auto begin=Clock::now();auto loaded=m3d::v16::loadScene(args[1]);if(!loaded){qCritical()<<loaded.error;return 5;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);if(!built){qCritical()<<built.error;return 6;}
    auto& rt=built.runtime;if(!cse::v17render::prepareEscargotScene(rt))return 7;
    m3d::updateLighting(rt.daylightHour);m3d::Stats shst;auto shadow=m3d::buildShadow(rt.renderScene,shst);
    m3d::setRasterPeripheralMultiresOverride(0);m3d::setRasterCloudsEnabled(false);
    m3d::setRasterHybridLightingEnabled(true);m3d::setRasterColorEnergyBalanceEnabled(true);
    m3d::setRasterFilmicTonemapEnabled(true);m3d::setRasterColorExposure(.85f);
    QJsonArray cameras;for(const auto& c:rt.cameras)cameras.append(QJsonObject{{"id",c.id},{"name",c.name},{"eye_x",c.eye.x},{"eye_y",c.eye.y},{"eye_z",c.eye.z}});
    write({{"type","metadata"},{"scene",loaded.scene.revision},{"force_warp",qEnvironmentVariable("M3D_V16_FORCE_WARP")},{"setup_ms",ms(begin)},{"cameras",cameras}});
    auto find=[&](QString id)->const m3d::v16::CameraPreset*{for(const auto& c:rt.cameras)if(c.id==id)return &c;return nullptr;};
    const auto* pool=find("v7_pool_fresco");const auto* glass=find("v7_curtain_wall_detail");if(!pool||!glass)return 8;
    const m3d::v16::CameraPreset* ext=nullptr;int score=0;
    for(const auto& c:rt.cameras){
        const auto label=(c.id+" "+c.name).toLower();int v=0;
        for(const QString k:{"exterior","extérieur","exterieur","facade","façade","plaza","square","street","parvis","front","outside"})if(label.contains(k))v+=4;
        for(const QString k:{"interior","intérieur","pool","piscine","stair","landing","curtain","verriere","detail","inside"})if(label.contains(k))v-=8;
        if(c.id==QStringLiteral("v600_garden_view"))v+=100;
        if(v>score){score=v;ext=&c;}
    }
    // Do not pretend that an interior preset is an exterior test. If a scene
    // lacks an exterior preset, create an explicitly labelled exterior pose.
    auto synthetic=*glass;
    if(!ext){
        synthetic.id=QStringLiteral("synthetic_exterior_offset");synthetic.name=QStringLiteral("Synthetic exterior from curtain preset");
        const auto dir=m3d::norm(glass->target-glass->eye);
        synthetic.eye=glass->eye-dir*18.f;synthetic.target=glass->target;ext=&synthetic;
    }
    write({{"type","view_selection"},{"pool",pool->id},{"glass",glass->id},{"exterior",ext->id},{"exterior_synthetic",ext==&synthetic}});
    const bool hardware=args.contains("--hardware");
    struct Config{int w,h,aa;};
    const std::array<Config,3> configs=hardware?std::array<Config,3>{{{640,360,1},{1280,720,1},{1280,720,8}}}:std::array<Config,3>{{{320,180,1},{960,540,1},{960,540,8}}};
    HWND hwnd=CreateWindowExW(0,L"STATIC",L"Escargot Object005 profile",WS_OVERLAPPEDWINDOW|WS_VISIBLE,0,0,1300,760,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!hwnd)return 9;
    struct Guard{HWND w;~Guard(){m3d::releaseDirectX11NativeForCurrentThread();DestroyWindow(w);}} guard{hwnd};
    uint32_t failed=0,records=0;const int reps=hardware?16:4;
    for(const auto& cfg:configs){
        auto quality=m3d::rasterQualityProfile(m3d::RasterQualityProfile::Ultra);
        quality.msaaSamples=cfg.aa;quality.anisotropy=16;quality.dynamicLod=false;
        std::wstring error;if(!m3d::setRasterQualitySettings(quality,error)){qCritical()<<QString::fromStdWString(error);return 10;}
        for(const auto* preset:{pool,glass,ext})for(int moving=0;moving<2;++moving){
            for(int mode=0;mode<5;++mode){
                const QString name=mode==0?"base-off":mode==1?"ZReflect":mode==2?"006-sky":mode==3?"006-full":"007-full";
                _putenv_s("M3D_V17_ZREFLECT",mode==1?"1":"0");
                if(!SetEnvironmentVariableA("M3D_V17_ZREFLECT",mode==1?"1":"0")||
                   !SetEnvironmentVariableA("CSE_WARP_ESCARGOT_PROFILE_BASE_OFF",mode==0?"1":"0"))return 13;
                const auto prep=Clock::now();e::projected::Session planner(rt,loaded.scene,mode==4);double prepMs=ms(prep);
                if(!planner.valid())return 11;
                for(int frame=-2;frame<reps;++frame){
                    auto pose=*preset;const float shift=moving?.055f*float(frame+2):0.f;
                    const auto original=m3d::v16::makeCamera(pose,cfg.w,cfg.h);pose.eye=pose.eye+original.r*shift;pose.target=pose.target+original.r*shift;
                    const auto cam=m3d::v16::makeCamera(pose,cfg.w,cfg.h);e::projected::Metrics m;
                    m3d::Stats st;m3d::EscargotBuildTiming bt;
                    const auto start=Clock::now();bool ok=false;
                    if(mode<2)ok=m3d::renderLumaTraceNative(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,cfg.w,cfg.h,hwnd,st,error);
                    else{
                        const auto& plan=planner.build(cam,shadow,cfg.w,cfg.h,m);
                        if(mode==2){
                            m3d::EscargotPacketBuild sky;sky.rasterProjected=true;
                            sky.projectedKey=plan.projectedKey|(uint64_t(1)<<63);
                            sky.data=plan.data;sky.projectedRanges=plan.projectedRanges;
                            for(auto& r:sky.projectedRanges)r.first=r.opaque=r.transparent=0;
                            // No geometry copy and identical cache policy to full.
                            // Planner still runs in both modes; compare GPU-only
                            // and end-to-end timings independently.
                            ok=m3d::renderLumaTraceEscargotGpuBuiltNative(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,cfg.w,cfg.h,hwnd,sky,st,bt,error);
                        }else ok=m3d::renderLumaTraceEscargotGpuBuiltNative(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,cfg.w,cfg.h,hwnd,plan,st,bt,error);
                    }
                    const double wall=ms(start);const auto& p=st.escProfile;
                    if(!ok||!p.completed||st.readbackBufferCount!=0||(mode!=1&&st.zReflectActive!=0)||(mode<2&&st.zReflectEnabled!=(mode==1))){++failed;qCritical()<<preset->id<<name<<frame<<QString::fromStdWString(error)<<"completion"<<p.completed<<"zreflect enabled"<<st.zReflectEnabled<<"active"<<st.zReflectActive<<"readback"<<st.readbackBufferCount;}
                    write({{"type","frame"},{"camera",preset->id},{"width",cfg.w},{"height",cfg.h},{"msaa",cfg.aa},{"moving",bool(moving)},{"mode",name},{"frame",frame},{"warmup",frame<0},{"ok",ok},{"completed",p.completed},{"gpu_valid",p.gpuValid},{"wall_ms",wall},{"planner_init_ms",prepMs},{"planner_ms",m.totalMs},{"receiver_ms",m.receiverMs},{"discover_ms",m.discoverMs},{"rank_ms",m.rankMs},{"cull_ms",m.cullMs},{"sort_ms",m.sortMs},{"emit_ms",m.emitMs},{"objects_tested",double(m.testedObjects)},{"triangles_tested",double(m.testedTriangles)},{"rays",int(m.rays)},{"sources",int(m.sources)},{"triangles",int(m.triangles)},{"planner_submitted_triangles",int(m.submittedTriangles)},{"omitted",int(m.omitted)},{"receivers",int(m.receivers)},{"deferred",int(m.deferred)},{"lighting_samples",int(m.lightingSamples)},{"planner_reused",m.reused},{"gpu_cached",p.cacheHit},{"map_ms",p.mapMs},{"alloc_ms",p.allocationMs},{"upload_ms",p.uploadMs},{"upload_bytes",double(p.uploadBytes)},{"resident_geometry_bytes",double(p.residentGeometryBytes)},{"resident_submitted_vertices",double(p.residentSubmittedVertices)},{"layer_pixels",double(p.layerPixels)},{"scissor_pixels",double(p.scissorPixels)},{"optical_draws",int(p.draws)},{"optical_submit_ms",p.opticalSubmitMs},{"compute_gpu_ms",p.computeGpuMs},{"optical_gpu_ms",p.opticalGpuMs},{"main_gpu_ms",p.mainGpuMs},{"copy_gpu_ms",p.copyGpuMs},{"gpu_total_ms",p.gpuTotalMs},{"wait_ms",p.waitMs},{"native_readback",int(st.readbackBufferCount)},{"zreflect_active",int(st.zReflectActive)},{"zreflect_enabled",st.zReflectEnabled}});
                    ++records;
                }
                qInfo()<<preset->id<<cfg.w<<cfg.aa<<moving<<name<<"profiled";
            }
        }
    }
    write({{"type","complete"},{"records",int(records)},{"failed",int(failed)}});
    return failed?12:0;
}
