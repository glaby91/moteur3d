#include "../escargot/debug.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cstdlib>
#include <chrono>
namespace e=m3d::escargot;
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
static bool save(const QDir& out,const QString& name,const std::vector<uint32_t>& p,int w,int h){
    return p.size()==size_t(w)*h&&QImage(reinterpret_cast<const uchar*>(p.data()),w,h,w*4,QImage::Format_RGB32).save(out.filePath(name));
}
static double quantile(std::vector<double> v,double q){std::sort(v.begin(),v.end());return v.empty()?0:v[size_t(std::ceil(q*v.size()))-1];}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();
    if(args.size()<3||args.size()>7){qCritical()<<"Usage: v17_escargot_ab scene.json outputDir [width] [height] [repeats] [rayBudget]";return 2;}
    bool ok=true;const int w=args.size()>3?args[3].toInt(&ok):640;if(!ok||w<16||w>2048)return 2;
    const int h=args.size()>4?args[4].toInt(&ok):360;if(!ok||h<16||h>2048)return 2;
    const int repeats=args.size()>5?args[5].toInt(&ok):5;if(!ok||repeats<1||repeats>120)return 2;
    e::Options opt;if(args.size()>6)opt.rayBudget=args[6].toUInt(&ok);if(!ok||!e::validOptions(opt))return 2;
    const QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 3;
    auto loaded=m3d::v16::loadScene(args[1]);if(!loaded){qCritical()<<loaded.error;return 4;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){qCritical()<<built.error<<built.runtime.lumaTraceBvhError;return 5;}
    auto& rt=built.runtime;auto& scene=rt.renderScene;
    if(!e::objectMap(rt,scene.escargotReceiverIds)){qCritical()<<"Invalid receiver ownership";return 5;}
    scene.markGpuGeometryChanged();
    e::Prepared session(rt,loaded.scene,opt.mode);
    _putenv_s("M3D_V17_ZREFLECT","1");_putenv_s("M3D_V17_ZATMOSPHERE","0");
    m3d::setRasterPeripheralMultiresOverride(0);m3d::setRasterCloudsEnabled(false);m3d::setRasterHybridLightingEnabled(true);
    m3d::setRasterColorEnergyBalanceEnabled(true);m3d::setRasterFilmicTonemapEnabled(true);m3d::setRasterColorExposure(.85f);
    auto quality=m3d::rasterQualityProfile(m3d::RasterQualityProfile::High);quality.msaaSamples=1;quality.dynamicLod=false;
    std::wstring err;if(!m3d::setRasterQualitySettings(quality,err)){qCritical()<<QString::fromStdWString(err);return 6;}
    m3d::updateLighting(rt.daylightHour);m3d::Stats shadowStats;const auto shadow=m3d::buildShadow(scene,shadowStats);
    QFile csv(out.filePath("frames.csv"));if(!csv.open(QIODevice::WriteOnly|QIODevice::Text))return 7;
    QTextStream log(&csv);log<<"camera,iteration,mode,total_ms,raster_ms,matrix_ms,composition_ms,atlas_ms,secondary_cameras,rays,candidates,deferred,complete_paths,unknown_paths,atlas_nonzero,texture_samples,reused_cells,gpu_ms,gpu_timing_valid,prepare_once_ms,candidate_ms,admission_ms,escargot_total_ms,static_candidates\n";
    QJsonArray summary;int tested=0;uint64_t allChanged=0,allActive=0,allCoverage=0,allEffective=0;
    for(const QString id:{QString("v17_showcase_pool_634"),QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==id;});if(it==rt.cameras.end()){qWarning()<<"Missing camera"<<id;continue;}
        const auto cam=m3d::v16::makeCamera(*it,w,h);std::vector<uint32_t> a,b;std::vector<double> aTimes,bTimes;
        auto runA=[&](int index,bool record)->bool{
            m3d::Stats st;const auto t=Clock::now();
            if(!m3d::renderLumaTracePreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,a,st,err)){qCritical()<<QString::fromStdWString(err);return false;}
            const auto elapsed=ms(t);if(!record)return true;
            aTimes.push_back(elapsed);allActive+=st.zReflectActive;
            log<<id<<','<<index<<",A,"<<elapsed<<','<<elapsed<<",0,0,0,"<<st.zReflectActive<<",0,"<<st.zReflectCandidates<<','<<st.zReflectDeferred<<",0,0,0,0,0,"<<st.gpuTotalMs<<','<<int(st.gpuTimingValid)<<",0,0,0,0,0\n";return true;
        };
        auto runB=[&](int index,bool record)->bool{
            const auto t=Clock::now();auto frame=session.frame(cam,w,h,opt);if(!frame.ok){qCritical()<<QString::fromStdString(frame.error);return false;}
            const auto atlasStart=Clock::now();const auto atlas=e::composeScreen(frame,w,h);
            std::vector<float> color(size_t(w)*h*4),meta(color.size());uint64_t nonzero=0;
            for(size_t i=0;i<atlas.size();++i){const auto& p=atlas[i];if(p.alpha<=0||p.weight<=0)continue;++nonzero;
                color[4*i]=p.rgb.x;color[4*i+1]=p.rgb.y;color[4*i+2]=p.rgb.z;color[4*i+3]=p.alpha;
                meta[4*i]=p.receiverDepth;meta[4*i+1]=float(p.receiverMaterial+1);meta[4*i+2]=p.weight;meta[4*i+3]=float(p.receiverObject);
            }
            const double atlasMs=ms(atlasStart);const auto rasterStart=Clock::now();m3d::Stats st;
            if(!m3d::renderLumaTraceEscargotPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,color,meta,b,st,err)){qCritical()<<QString::fromStdWString(err);return false;}
            const double rasterMs=ms(rasterStart),elapsed=ms(t);if(st.zReflectActive!=0){qCritical()<<"Escargot unexpectedly submitted secondary views";return false;}
            if(!record)return true;bTimes.push_back(elapsed);allCoverage+=nonzero;
            log<<id<<','<<index<<",B,"<<elapsed<<','<<rasterMs<<','<<frame.matrixMs<<','<<frame.compositionMs<<','<<atlasMs<<','<<st.zReflectActive<<','<<frame.rays<<','<<frame.candidates<<','<<frame.deferred<<','<<frame.completePaths<<','<<frame.unknownPaths<<','<<qulonglong(nonzero)<<','<<qulonglong(frame.textureSamples)<<','<<qulonglong(frame.reusedCells)<<','<<st.gpuTotalMs<<','<<int(st.gpuTimingValid)<<','<<frame.prepareMs<<','<<frame.candidateMs<<','<<frame.admissionMs<<','<<frame.totalMs<<','<<frame.staticCandidates<<'\n';return true;
        };
        if(!runA(-1,false)||!runB(-1,false))return 8;
        for(int j=0;j<repeats;++j){if(j%2==0){if(!runA(j,true)||!runB(j,true))return 9;}else{if(!runB(j,true)||!runA(j,true))return 9;}}
        if(a.size()!=size_t(w)*h||b.size()!=a.size())return 10;
        uint64_t changed=0;double errorSum=0;std::vector<uint32_t> diff(a.size());
        for(size_t i=0;i<a.size();++i){int dr=std::abs(int((a[i]>>16)&255)-int((b[i]>>16)&255)),dg=std::abs(int((a[i]>>8)&255)-int((b[i]>>8)&255)),db=std::abs(int(a[i]&255)-int(b[i]&255));
            changed+=(dr||dg||db);errorSum+=dr+dg+db;diff[i]=0xff000000u|(uint32_t(std::min(255,dr*4))<<16)|(uint32_t(std::min(255,dg*4))<<8)|uint32_t(std::min(255,db*4));}
        allChanged+=changed;
        // Negative control: suppress cameras AND send an empty atlas. A/B
        // difference alone could only prove A's cameras were removed.
        std::vector<float> empty(size_t(w)*h*4,0.f);std::vector<uint32_t> control;m3d::Stats controlStats;
        if(!m3d::renderLumaTraceEscargotPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,empty,empty,control,controlStats,err)||control.size()!=b.size()||controlStats.zReflectActive!=0)return 14;
        uint64_t effective=0;for(size_t i=0;i<b.size();++i)effective+=((b[i]&0xffffffu)!=(control[i]&0xffffffu));
        allEffective+=effective;
        if(!save(out,id+"-B-empty-control.png",control,w,h))return 14;
        // A must remain stable after switching back from B.
        std::vector<uint32_t> again;m3d::Stats againStats;
        if(!m3d::renderLumaTracePreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,again,againStats,err)||again!=a){qCritical()<<"A/B restore parity failed"<<id;return 15;}
        if(!save(out,id+"-A-virtual.png",a,w,h)||!save(out,id+"-B-escargot.png",b,w,h)||!save(out,id+"-difference-x4.png",diff,w,h))return 11;
        summary.append(QJsonObject{{"camera",id},{"A_p50_ms",quantile(aTimes,.50)},{"A_p95_ms",quantile(aTimes,.95)},{"B_p50_ms",quantile(bTimes,.50)},{"B_p95_ms",quantile(bTimes,.95)},{"changedPixels",double(changed)},{"escargotEffectivePixels",double(effective)},{"meanAbsChannelDifference",errorSum/(3*a.size())}});
        qInfo()<<id<<"A p50"<<quantile(aTimes,.5)<<"B p50"<<quantile(bTimes,.5)<<"changed"<<qulonglong(changed)<<"effective Escargot pixels"<<qulonglong(effective);++tested;log.flush();
    }
    if(!tested||!allActive||!allCoverage||!allChanged||!allEffective){qCritical()<<"Invalid A/B: no active virtual camera, optical data or visible changes";return 12;}
    QFile json(out.filePath("summary.json"));if(!json.open(QIODevice::WriteOnly))return 13;
    const QJsonObject report{{"scene",loaded.scene.revision},{"width",w},{"height",h},{"repeats",repeats},{"rayBudget",int(opt.rayBudget)},{"textureResolutionCap",256},{"msaaRequested",1},{"modeB","CPU matrices + native LT pixel shader, no secondary cameras"},{"receiverMatch","exact source object ID + material, approximate reconstructed depth"},{"sourceLighting","simplified proxy, not LT radiance"},{"forceWarp",qEnvironmentVariable("M3D_V16_FORCE_WARP")},{"cameras",summary}};
    const auto bytes=QJsonDocument(report).toJson();if(json.write(bytes)!=bytes.size()||log.status()!=QTextStream::Ok)return 13;
    return 0;
}
