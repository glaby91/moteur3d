// CSE WARP V17: one executable, repeatable full-engine benchmark campaign.
// GUI quality is inherited through CLI flags. WARP CI uses --smoke; real
// performance conclusions must come from the target hardware.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib,"Psapi.lib")
#endif

using namespace m3d;
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point start){
    return std::chrono::duration<double,std::milli>(Clock::now()-start).count();
}
bool writeJson(const QString& path,const QJsonObject& value){
    QFile f(path);
    return f.open(QIODevice::WriteOnly|QIODevice::Truncate)
        &&f.write(QJsonDocument(value).toJson(QJsonDocument::Indented))>0;
}
bool writeImage(const QString& path,const std::vector<uint32_t>& pixels,int w,int h){
    if(pixels.size()!=size_t(w)*size_t(h))return false;
    QImage image(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(image.scanLine(y),pixels.data()+size_t(y)*w,size_t(w)*4);
    return image.save(path,"PNG");
}
QString escapeCsv(QString s){
    s.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+s+QStringLiteral("\"");
}
double mean(const std::vector<double>& x){
    return x.empty()?0.:std::accumulate(x.begin(),x.end(),0.)/double(x.size());
}
double pct(std::vector<double> values,double p){
    if(values.empty())return 0.;
    std::sort(values.begin(),values.end());
    const double at=p*(values.size()-1),a=std::floor(at);
    const size_t i=size_t(a),j=std::min(i+1,values.size()-1);
    return values[i]+(values[j]-values[i])*(at-a);
}
double deviation(const std::vector<double>& x){
    if(x.size()<2)return 0.;
    const double m=mean(x);
    double sum=0.;
    for(double v:x)sum+=(v-m)*(v-m);
    return std::sqrt(sum/double(x.size()-1));
}
struct Memory{
    double workingMiB=-1,peakMiB=-1;
};
Memory memoryUsage(){
    Memory out;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX info{};
    if(GetProcessMemoryInfo(GetCurrentProcess(),
           reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&info),sizeof(info))){
        out.workingMiB=double(info.WorkingSetSize)/(1024.*1024.);
        out.peakMiB=double(info.PeakWorkingSetSize)/(1024.*1024.);
    }
#endif
    return out;
}
struct Samples {
    QString scene,camera,variant;
    int w=0,h=0,depth=1,requestedMsaa=0,passes=0;
    bool ok=true,parity=true,gpuTimingValid=false;
    QString error,image;
    double coldMs=0,wallMs=0;
    uint64_t submittedTriangles=0,drawCalls=0;
    std::vector<double> latency,gpuDraw,gpuTransfer,gpuTotal,cpuSubmit,cpuWait,cpuSwizzle,blockFps;
    std::vector<uint32_t> baseline;
    void add(const Stats& s){
        latency.push_back(s.ms);
        if(s.gpuTimingValid){gpuTimingValid=true;
            gpuDraw.push_back(s.gpuDrawMs);
            gpuTransfer.push_back(s.gpuTransferMs);
            gpuTotal.push_back(s.gpuTotalMs);
        }
        cpuSubmit.push_back(s.cpuSubmitMs);
        cpuWait.push_back(s.cpuMapWaitMs);
        cpuSwizzle.push_back(s.cpuPixelConvertMs);
        submittedTriangles=s.tests;drawCalls=s.gpuDrawCalls;
    }
    QJsonObject json() const{
        const auto m=memoryUsage();
        QJsonObject out{
            {"scene",scene},{"camera",camera},{"variant",variant},
            {"width",w},{"height",h},{"readbackDepth",depth},
            {"requestedMsaa",requestedMsaa},{"roundsCompleted",passes},
            {"frames",int(latency.size())},{"coldMs",coldMs},
            {"meanMs",mean(latency)},{"medianMs",pct(latency,.5)},
            {"p95Ms",pct(latency,.95)},{"p99Ms",pct(latency,.99)},
            {"stddevMs",deviation(latency)},
            {"cvPercent",mean(latency)>0?100.*deviation(latency)/mean(latency):0.},
            {"blockFpsMedian",pct(blockFps,.5)},
            {"blockFpsMin",blockFps.empty()?0.:*std::min_element(blockFps.begin(),blockFps.end())},
            {"blockFpsMax",blockFps.empty()?0.:*std::max_element(blockFps.begin(),blockFps.end())},
            {"streamFps",wallMs>0?1000.*double(latency.size())/wallMs:0.},
            {"gpuTimingValid",gpuTimingValid},{"gpuDrawMs",mean(gpuDraw)},
            {"gpuTransferMs",mean(gpuTransfer)},{"gpuTotalMs",mean(gpuTotal)},
            {"cpuSubmitMs",mean(cpuSubmit)},{"cpuMapWaitMs",mean(cpuWait)},
            {"cpuPixelConvertMs",mean(cpuSwizzle)},
            {"submittedTriangles",double(submittedTriangles)},
            {"gpuDrawCalls",double(drawCalls)},
            {"pixelIdentical",parity},{"status",ok?"ok":"failed"},
            {"error",error},{"image",image},
            {"memoryWorkingMiB",m.workingMiB},{"memoryPeakMiB",m.peakMiB}
        };
        return out;
    }
};
struct Config{
    int rounds=6,frames=12,warmup=2;
    bool smoke=false,allScenes=false,includeCpu=false;
    RasterQualitySettings quality{};
    QString selected,baseDir;
};
bool runRasterBlock(const Scene& scene,const ShadowMap& shadows,const Camera& camera,
                    Samples& result,int round,int frames,int warmup,
                    QTextStream& frameCsv){
    std::vector<uint32_t> pixels;
    Stats stats;std::wstring reason;
    if(result.coldMs==0.){
        const auto start=Clock::now();
        if(!renderDirectX11Preview(scene,shadows,camera,result.w,result.h,pixels,stats,reason)){
            result.ok=false;result.error=QString::fromStdWString(reason);return false;
        }
        result.coldMs=ms(start);
    }
    for(int i=0;i<warmup;++i){
        if(!renderDirectX11Preview(scene,shadows,camera,result.w,result.h,pixels,stats,reason)){
            result.ok=false;result.error=QString::fromStdWString(reason);return false;
        }
    }
    const auto wall=Clock::now();
    int submitted=0,received=0;
    bool ok=true;
    for(int i=0;i<frames;++i){
        reason.clear();
        if(result.depth==1){
            ok=renderDirectX11Preview(scene,shadows,camera,result.w,result.h,pixels,stats,reason);
            ++submitted;
            if(ok){++received;result.add(stats);}
        }else{
            ok=enqueueDirectX11Preview(scene,shadows,camera,result.w,result.h,
                                       unsigned(result.depth),reason);
            if(ok)++submitted;
            if(ok&&submitted-received>=result.depth){
                ok=receiveDirectX11Preview(pixels,stats,reason);
                if(ok){++received;result.add(stats);}
            }
        }
        if(!ok)break;
    }
    // Drain the GPU queue even if a submission failed; future variants must
    // never inherit stale images or incompatible quality settings.
    while(received<submitted&&result.depth>1){
        std::wstring drainError;
        if(!receiveDirectX11Preview(pixels,stats,drainError)){
            ok=false;if(reason.empty())reason=drainError;break;
        }
        ++received;result.add(stats);
    }
    const double wallDuration=ms(wall);
    result.wallMs+=wallDuration;
    if(received){
        result.blockFps.push_back(received*1000./std::max(.001,wallDuration));
        // Frame-level rows are saved even for an interrupted campaign.
        const int first=int(result.latency.size())-received;
        for(int i=first;i<int(result.latency.size());++i){
            frameCsv<<escapeCsv(result.scene)<<','<<escapeCsv(result.camera)<<','
                <<escapeCsv(result.variant)<<','<<result.depth<<','<<round<<','
                <<(i-first)<<','<<result.latency[size_t(i)]<<'\n';
        }
        frameCsv.flush();
        // Every depth, every round, must preserve exact pixel values.
        if(result.baseline.empty())result.baseline=pixels;
        else if(result.baseline!=pixels){result.parity=false;ok=false;
            reason=L"Non-deterministic frame pixels within one configuration";
        }
    }
    if(!ok||received!=frames){
        result.ok=false;
        result.error=reason.empty()?QStringLiteral("Incomplete frame block"):QString::fromStdWString(reason);
        return false;
    }
    ++result.passes;
    return true;
}
void report(const QJsonObject& system,const std::vector<Samples>& cases,
            const QJsonArray& sideTests,QDir& dir,bool complete){
    QJsonArray rows;
    QFile csv(dir.filePath("campaign_results.csv"));
    if(csv.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)){
        QTextStream stream(&csv);
        stream<<"scene,camera,variant,width,height,readback_depth,rounds,frames,"
              "mean_ms,median_ms,p95_ms,p99_ms,stddev_ms,cv_percent,"
              "stream_fps,block_fps_median,block_fps_min,block_fps_max,"
              "gpu_draw_ms,gpu_transfer_ms,gpu_total_ms,cpu_submit_ms,"
              "cpu_map_wait_ms,cpu_pixel_convert_ms,triangles,draw_calls,"
              "pixel_identical,status,error,image\n";
        for(const auto& c:cases){
            const auto j=c.json();rows.append(j);
            stream<<escapeCsv(c.scene)<<','<<escapeCsv(c.camera)<<','
                <<escapeCsv(c.variant)<<','<<c.w<<','<<c.h<<','<<c.depth<<','
                <<c.passes<<','<<int(c.latency.size())<<','
                <<j.value("meanMs").toDouble()<<','<<j.value("medianMs").toDouble()<<','
                <<j.value("p95Ms").toDouble()<<','<<j.value("p99Ms").toDouble()<<','
                <<j.value("stddevMs").toDouble()<<','<<j.value("cvPercent").toDouble()<<','
                <<j.value("streamFps").toDouble()<<','<<j.value("blockFpsMedian").toDouble()<<','
                <<j.value("blockFpsMin").toDouble()<<','<<j.value("blockFpsMax").toDouble()<<','
                <<j.value("gpuDrawMs").toDouble()<<','<<j.value("gpuTransferMs").toDouble()<<','
                <<j.value("gpuTotalMs").toDouble()<<','<<j.value("cpuSubmitMs").toDouble()<<','
                <<j.value("cpuMapWaitMs").toDouble()<<','<<j.value("cpuPixelConvertMs").toDouble()<<','
                <<j.value("submittedTriangles").toDouble()<<','<<j.value("gpuDrawCalls").toDouble()<<','
                <<(c.parity?"true":"false")<<','<<(c.ok?"ok":"failed")<<','
                <<escapeCsv(c.error)<<','<<escapeCsv(c.image)<<'\n';
        }
    }else{
        for(const auto& c:cases)rows.append(c.json());
    }
    const QJsonObject summary{{"schemaVersion",1},{"complete",complete},
        {"system",system},{"cases",rows},{"subsystemTests",sideTests},
        {"completedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
    writeJson(dir.filePath("summary.json"),summary);
    QFile html(dir.filePath("campaign_report.html"));
    if(html.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)){
        QTextStream out(&html);
        out<<"<!doctype html><html lang=\"fr\"><meta charset=\"utf-8\">"
              "<title>CSE WARP V17 — campagne</title>"
              "<style>body{font:15px system-ui;max-width:1300px;margin:35px auto;"
              "padding:0 15px;color:#172336}table{border-collapse:collapse;width:100%}"
              "th,td{border-bottom:1px solid #ddd;padding:8px;text-align:right}"
              "td:first-child,th:first-child{text-align:left}th{background:#eaf0f8}"
              "small{color:#596579}</style><h1>CSE WARP V17 — campagne complète</h1>"
              "<p>Terminé : "<<(complete?"oui":"non")<<". Référence : GPU natif uniquement "
              "sur la machine testée. Les FPS de pipeline ne sont pas les FPS écran.</p>"
              "<table><tr><th>Scène / caméra / variante</th><th>Pixels</th>"
              "<th>Buffers</th><th>Images</th><th>FPS débit</th><th>P50 ms</th>"
              "<th>P95 ms</th><th>Dispersion %</th><th>GPU draw ms</th>"
              "<th>Pixels identiques</th></tr>";
        for(const auto& c:cases){
            auto j=c.json();
            out<<"<tr><td>"<<(c.scene+" / "+c.camera+" / "+c.variant).toHtmlEscaped()
               <<"</td><td>"<<c.w<<"×"<<c.h<<"</td><td>"<<c.depth
               <<"</td><td>"<<c.latency.size()<<"</td><td>"
               <<QString::number(j.value("streamFps").toDouble(),'f',2)
               <<"</td><td>"<<QString::number(j.value("medianMs").toDouble(),'f',2)
               <<"</td><td>"<<QString::number(j.value("p95Ms").toDouble(),'f',2)
               <<"</td><td>"<<QString::number(j.value("cvPercent").toDouble(),'f',1)
               <<"</td><td>"<<QString::number(j.value("gpuDrawMs").toDouble(),'f',2)
               <<"</td><td>"<<(c.parity?"oui":"NON")<<"</td></tr>";
        }
        out<<"</table><p><small>Comparaison valide uniquement à qualité, scène et "
              "résolution égales. P50/P95 mesurent la latence de chaque image; "
              "FPS débit inclut remplissage et vidage du pipeline. Les mesures "
              "de bloc permettent d'identifier la variation thermique.</small></p>"
              "<h2>Autres sous-systèmes</h2><pre>"
           <<QString::fromUtf8(QJsonDocument(sideTests).toJson(QJsonDocument::Indented)).toHtmlEscaped()
           <<"</pre></html>";
    }
}
QJsonObject subsystem(QString name,QString variant,double elapsed,bool ok=true,
                      QString note={}){
    return QJsonObject{{"name",name},{"variant",variant},{"elapsedMs",elapsed},
                       {"status",ok?"ok":"skipped"},{"note",note}};
}
} // namespace

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    QTextStream out(stdout),err(stderr);
    const auto args=app.arguments();
    if(args.size()<3){
        err<<"Usage: moteur3d_v17_campaign scene.json outdir [--rounds 6] "
              "[--frames 12] [--warmup 2] [--all-scenes] [--include-cpu] "
              "[--smoke] [--raster-msaa 1|2|4|8] [--raster-anisotropy 1|2|4|8|16] "
              "[--raster-lod-percent 50..200] [--raster-culling 0|1] "
              "[--raster-lod-enabled 0|1]\n";
        return 2;
    }
    Config cfg;
    cfg.selected=QFileInfo(args[1]).absoluteFilePath();
    cfg.baseDir=args[2];
    cfg.smoke=args.contains("--smoke");
    cfg.allScenes=args.contains("--all-scenes")&&!cfg.smoke;
    cfg.includeCpu=args.contains("--include-cpu")&&!cfg.smoke;
    cfg.quality=rasterQualitySettings();
    const auto parseInt=[&](QString key,int& value,int lo,int hi)->bool{
        int at=args.indexOf(key);
        if(at<0)return true;
        bool ok=false;
        int v=at+1<args.size()?args[at+1].toInt(&ok):0;
        if(!ok||v<lo||v>hi){err<<"Invalid "<<key<<" (expected "<<lo<<".."<<hi<<")\n";return false;}
        value=v;return true;
    };
    if(!parseInt("--rounds",cfg.rounds,1,30)||
       !parseInt("--frames",cfg.frames,2,240)||
       !parseInt("--warmup",cfg.warmup,0,20))return 2;
    if(cfg.smoke){cfg.rounds=2;cfg.frames=2;cfg.warmup=1;}
    int msaa=int(cfg.quality.msaaSamples),aniso=int(cfg.quality.anisotropy),
        lod=int(cfg.quality.lodQualityPercent),
        cull=int(cfg.quality.frustumCulling),dynamic=int(cfg.quality.dynamicLod);
    if(!parseInt("--raster-msaa",msaa,1,8)||
       !parseInt("--raster-anisotropy",aniso,1,16)||
       !parseInt("--raster-lod-percent",lod,50,200)||
       !parseInt("--raster-culling",cull,0,1)||
       !parseInt("--raster-lod-enabled",dynamic,0,1))return 2;
    cfg.quality.msaaSamples=uint32_t(msaa);
    cfg.quality.anisotropy=uint32_t(aniso);
    cfg.quality.lodQualityPercent=uint32_t(lod);
    cfg.quality.frustumCulling=cull!=0;
    cfg.quality.dynamicLod=dynamic!=0;
    std::wstring error;
    if(!setRasterQualitySettings(cfg.quality,error)){
        err<<"Invalid raster settings: "<<QString::fromStdWString(error)<<"\n";return 2;
    }
    const QString stamp=QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString runName=QStringLiteral("CSE-WARP-V17.%1-CAMPAGNE-%2")
        .arg(CSE_WARP_BUILD_NUMBER).arg(stamp);
    if(!QDir(cfg.baseDir).mkpath(runName)){err<<"Cannot create output directory\n";return 3;}
    QDir dir(QDir(cfg.baseDir).filePath(runName));
    dir.mkpath("images");
    QFile raw(dir.filePath("campaign_frames.csv"));
    if(!raw.open(QIODevice::WriteOnly|QIODevice::Text)){err<<"Cannot open frames CSV\n";return 3;}
    QTextStream frameCsv(&raw);
    frameCsv<<"scene,camera,variant,readback_depth,round,frame,latency_ms\n";
    QJsonObject system{
        {"engine","CSE WARP V17 complete benchmark campaign"},
        {"version",QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER)},
        {"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"os",QSysInfo::prettyProductName()},
        {"architecture",QSysInfo::currentCpuArchitecture()},
        {"cpuThreads",QThread::idealThreadCount()},
        {"qt",QStringLiteral(QT_VERSION_STR)},
        {"rounds",cfg.rounds},{"framesPerRound",cfg.frames},{"warmup",cfg.warmup},
        {"smoke",cfg.smoke},{"allScenes",cfg.allScenes},
        {"quality",QJsonObject{
            {"msaaRequested",msaa},{"anisotropy",aniso},{"lodPercent",lod},
            {"culling",cull!=0},{"lodEnabled",dynamic!=0}
        }},
        {"timings","D3D11 GPU timestamp (draw/copy); CPU Map wait and pixel conversion; per-frame and per-block statistics. Not onscreen FPS or vsync."},
        {"limitations","No precise per-shader-stage costs, VRAM usage or Qt presentation timer. CPU scaling uses a deliberately small separate workload."}
    };
    QFile revision(QCoreApplication::applicationDirPath()+"/REVISION.txt");
    if(revision.open(QIODevice::ReadOnly|QIODevice::Text))
        system["revision"]=QString::fromUtf8(revision.readAll()).trimmed().left(12);
    QJsonArray side;
    std::vector<Samples> cases;
    QStringList scenes{cfg.selected};
    if(cfg.allScenes){
        QDir appDir(QCoreApplication::applicationDirPath());
        for(const QString& item:{
            QStringLiteral("pavillon01/scene.json"),
            QStringLiteral("benchmark-civic/scene.json"),
            QStringLiteral("benchmark-civic/scene-lourd.json"),
            QStringLiteral("benchmark-civic/scene-extreme.json"),
            QStringLiteral("material-lab/scene.json"),
            QStringLiteral("pripyat-photo2scene-proof/scene.json")}){
            const QString p=appDir.absoluteFilePath(item);
            if(QFileInfo::exists(p))scenes.append(p);
        }
    }
    QSet<QString> seen;
    QStringList unique;
    for(const QString& s:scenes){
        const QString canonical=QFileInfo(s).canonicalFilePath();
        if(!canonical.isEmpty()&&!seen.contains(canonical)){
            seen.insert(canonical);unique.append(canonical);
        }
    }
    if(unique.empty()){err<<"No scene found: "<<cfg.selected<<"\n";return 4;}
    system["sceneCount"]=unique.size();
    bool allOk=true, cpuDone=false;
    for(int sceneIndex=0;sceneIndex<unique.size();++sceneIndex){
        const QString& path=unique[sceneIndex];
        const QString sceneName=QFileInfo(path).completeBaseName()+"-"
                                +QString::number(sceneIndex);
        out<<"Scene "<<(sceneIndex+1)<<"/"<<unique.size()<<": "<<path<<"\n";out.flush();
        // The previous full campaign looked stalled for more than a minute
        // while building a small scene. Show actual phase transitions and
        // durations, never invent progress or remaining-time estimates.
        out<<"  [chargement] Analyse des fichiers de scene...\n";out.flush();
        auto tick=Clock::now();
        auto loaded=m3d::v16::loadScene(path);
        const double parseMs=ms(tick);
        side.append(subsystem(sceneName,"scene_parse",parseMs,bool(loaded),loaded.error));
        out<<"  [chargement] "<<parseMs<<" ms, "<<(loaded?"OK":"ERREUR")<<"\n";out.flush();
        if(!loaded){allOk=false;continue;}
        out<<"  [construction] Geometrie et textures PBR...\n";out.flush();
        tick=Clock::now();
        const auto built=m3d::v16::buildRuntime(loaded.scene,cfg.smoke?256:2048);
        const double buildMs=ms(tick);
        side.append(subsystem(sceneName,"geometry_materials_pbr_build",
            buildMs,bool(built),built.error));
        out<<"  [construction] "<<buildMs<<" ms, "<<(built?"OK":"ERREUR")<<"\n";out.flush();
        if(!built){allOk=false;continue;}
        const auto& runtime=built.runtime;
        side.append(subsystem(sceneName,"pbr_mip_generation",runtime.pbrBuildMs));
        side.append(subsystem(sceneName,"geometry_instancing",runtime.geometryBuildMs));
        side.append(subsystem(sceneName,"bvh_build",runtime.bvhBuildMs));
        out<<"  [detail] PBR "<<runtime.pbrBuildMs<<" ms ; instanciation "
           <<runtime.geometryBuildMs<<" ms ; BVH "<<runtime.bvhBuildMs<<" ms\n";
        out.flush();
        updateLighting(runtime.daylightHour);
        out<<"  [ombres] Preparation de la carte CPU...\n";out.flush();
        tick=Clock::now();
        Stats shadowStats;
        const ShadowMap shadows=buildShadow(runtime.renderScene,shadowStats);
        const double shadowMs=ms(tick);
        side.append(subsystem(sceneName,"shadowmap_cpu_build",shadowMs));
        out<<"  [ombres] "<<shadowMs<<" ms\n";out.flush();
        const auto mem=memoryUsage();
        out<<"  [memoire] "<<mem.workingMiB<<" MiB actifs ; pic processus "
           <<mem.peakMiB<<" MiB\n";out.flush();
        side.append(QJsonObject{{"name",sceneName},{"variant","memory_after_scene_build"},
            {"workingMiB",mem.workingMiB},{"peakMiB",mem.peakMiB},
            {"triangles",double(runtime.renderScene.obj.size())},
            {"materialCount",int(runtime.renderScene.mat.size())},
            {"cameraCount",int(runtime.cameras.size())}});
        if(runtime.cameras.empty()){allOk=false;continue;}
        const int cameras=cfg.smoke?1:std::min(2,int(runtime.cameras.size()));
        const QVector<QSize> sizes=cfg.smoke
            ?QVector<QSize>{QSize(320,180)}
            :QVector<QSize>{QSize(1366,768),QSize(1920,1080)};
        for(int cameraIndex=0;cameraIndex<cameras;++cameraIndex){
            const auto& preset=runtime.cameras[cameraIndex];
            for(const QSize size:sizes){
                const auto camera=m3d::v16::makeCamera(preset,size.width(),size.height());
                const QString variant=QStringLiteral("Raster GPU qualité constante");
                const size_t index=cases.size();
                for(int depth=1;depth<=3;++depth){
                    Samples entry;
                    entry.scene=sceneName;entry.camera=preset.id;entry.variant=variant;
                    entry.w=size.width();entry.h=size.height();
                    entry.depth=depth;entry.requestedMsaa=msaa;
                    cases.push_back(std::move(entry));
                }
                // Balance run order to avoid always penalizing the last buffer
                // depth after GPU thermal ramp or boost frequency changes.
                for(int round=0;round<cfg.rounds;++round){
                    for(int order=0;order<3;++order){
                        const int depth=1+(order+round)%3;
                        Samples& entry=cases[index+size_t(depth-1)];
                        if(!entry.ok)continue;
                        if(!runRasterBlock(runtime.renderScene,shadows,camera,entry,
                                           round,cfg.frames,cfg.warmup,frameCsv)){
                            allOk=false;
                            err<<"FAILED "<<sceneName<<" depth "<<depth<<": "<<entry.error<<"\n";
                        }else{
                            out<<"  "<<preset.id<<" "<<size.width()<<"x"<<size.height()
                               <<" buffer "<<depth<<" round "<<(round+1)<<"/"<<cfg.rounds
                               <<" => "<<QString::number(entry.blockFps.back(),'f',2)
                               <<" pipeline FPS\n";out.flush();
                        }
                    }
                }
                // Cross-mode image parity checked independently from in-mode
                // per-round determinism.
                const auto& reference=cases[index].baseline;
                for(size_t j=index;j<index+3;++j){
                    auto& entry=cases[j];
                    if(reference.empty()||entry.baseline!=reference){
                        entry.parity=false;entry.ok=false;
                        entry.error=QStringLiteral("1/2/3 buffer pixel mismatch");
                        allOk=false;
                    }
                    if(!entry.baseline.empty()){
                        const QString name=QStringLiteral("images/scene%1_cam%2_%3x%4_depth%5.png")
                            .arg(sceneIndex).arg(cameraIndex).arg(entry.w).arg(entry.h).arg(entry.depth);
                        if(writeImage(dir.filePath(name),entry.baseline,entry.w,entry.h))
                            entry.image=name;
                    }
                }
                report(system,cases,side,dir,false);
            }
        }
        // CPU multi-core and Compute are separate diagnostic workloads, not
        // a misleading FPS comparison with the full-size Raster image.
        // The previous filename-based "sceneName" check never matched
        // pavillon01: scene.json becomes "scene-1". Choose the actual parent
        // directory, then only fall back to the last scene when no pavilion
        // is present in the campaign.
        const bool isPavilion=QFileInfo(path).dir().dirName().compare(
            QStringLiteral("pavillon01"),Qt::CaseInsensitive)==0;
        const bool pavilionPresent=std::any_of(unique.cbegin(),unique.cend(),
            [](const QString& p){return QFileInfo(p).dir().dirName().compare(
                QStringLiteral("pavillon01"),Qt::CaseInsensitive)==0;});
        if(cfg.includeCpu&&!cpuDone &&
           (!cfg.allScenes || isPavilion ||
            (!pavilionPresent&&sceneIndex+1==unique.size()))){
            cpuDone=true;
            out<<"  [diagnostic CPU/Compute] scene "<<sceneName
               <<" : "<<cfg.rounds<<" series de mesure par nombre de threads\n";
            out.flush();
            const auto& preset=runtime.cameras.first();
            constexpr int cpuW=320,cpuH=180;
            const auto cam=m3d::v16::makeCamera(preset,cpuW,cpuH);
            std::vector<uint32_t> pixels;
            std::atomic<uint64_t> generation{1};
            const int available=std::clamp(QThread::idealThreadCount(),1,32);
            for(int workers:{1,2,4,8,16,32}){
                if(workers>available)continue;
                std::vector<double> times;
                // CPU trend must use the same repeat count as the GPU
                // campaign instead of two noisy timing samples.
                for(int rep=0;rep<cfg.rounds;++rep){
                    tick=Clock::now();
                    const Stats result=render(runtime.renderScene,shadows,cam,cpuW,cpuH,pixels,
                        unsigned(workers),false,generation,1,1,false,false,false,3);
                    const double wallMs=ms(tick);
                    times.push_back(wallMs);
                    if(rep==0)
                        side.append(QJsonObject{{"name",sceneName},{"variant","cpu_direct_counters"},
                            {"threads",workers},{"rays",double(result.rays)},
                            {"triangleTests",double(result.tests)}});
                }
                side.append(QJsonObject{{"name",sceneName},{"variant","cpu_direct_scaling"},
                    {"threads",workers},{"resolution","320x180"},
                    {"meanMs",mean(times)},{"medianMs",pct(times,.5)},
                    {"p95Ms",pct(times,.95)},{"stddevMs",deviation(times)},
                    {"cvPercent",mean(times)>0?100.*deviation(times)/mean(times):0.},
                    {"runs",int(times.size())},{"note","No GPU comparison; fixed CPU 1 spp workload"}});
                out<<"  CPU "<<workers<<" threads "<<mean(times)<<" ms @320x180\n";out.flush();
            }
            tick=Clock::now();
            Stats computeStats;
            std::wstring computeError;
            out<<"  [Compute DirectX 11] Initialisation/test 320x180...\n";out.flush();
            bool gpuOk=renderCompute11Preview(runtime.renderScene,cam,cpuW,cpuH,
                                               pixels,computeStats,computeError);
            side.append(subsystem(sceneName,"d3d11_compute_320x180",ms(tick),
                gpuOk,QString::fromStdWString(computeError)));
        }
        report(system,cases,side,dir,false);
    }
    report(system,cases,side,dir,true);
    out<<"Campaign complete: "<<cases.size()<<" GPU cases, "
       <<side.size()<<" subsystem records; result "<<dir.absolutePath()<<"\n";
    return allOk?0:7;
}
