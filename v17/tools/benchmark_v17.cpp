// CSE WARP V17: benchmark the actual shared V16 scene through V17's D3D11
// raster frontend. These are render + GPU readback timings, not on-screen FPS.
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
#include <QSize>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

using namespace m3d;
namespace {
using Clock=std::chrono::steady_clock;
double elapsedMs(Clock::time_point t){
    return std::chrono::duration<double,std::milli>(Clock::now()-t).count();
}
bool writeJson(const QString& filename,const QJsonObject& data){
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Truncate))return false;
    return file.write(QJsonDocument(data).toJson(QJsonDocument::Indented))>0;
}
bool writeImage(const QString& filename,const std::vector<uint32_t>& pixels,int w,int h){
    if(pixels.size()!=size_t(w)*size_t(h))return false;
    QImage image(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(image.scanLine(y),pixels.data()+size_t(y)*w,size_t(w)*4);
    return image.save(filename,"PNG");
}
QString csv(QString v){
    v.replace('"',QStringLiteral("\"\""));
    return QStringLiteral("\"")+v+QStringLiteral("\"");
}
double percentile(const std::vector<double>& sorted,double fraction){
    if(sorted.empty())return 0;
    const double pos=fraction*double(sorted.size()-1);
    const size_t lo=size_t(pos),hi=std::min(sorted.size()-1,lo+1);
    return sorted[lo]+(sorted[hi]-sorted[lo])*(pos-double(lo));
}
void addTimings(QJsonObject& record,std::vector<double> values){
    if(values.empty())return;
    std::sort(values.begin(),values.end());
    const double mean=std::accumulate(values.begin(),values.end(),0.0)/values.size();
    record["frames"]=int(values.size());
    record["meanMs"]=mean;
    record["medianMs"]=percentile(values,.50);
    record["p95Ms"]=percentile(values,.95);
    record["p99Ms"]=percentile(values,.99);
    record["estimatedFps"]=mean>0?1000.0/mean:0.0;
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    QTextStream out(stdout),err(stderr);
    if(argc<3){
        err<<"Usage: moteur3d_v17_benchmark.exe scene.json output_dir [--quick] [--compare-v16] [--smoke]\n";
        return 2;
    }
    const QString scenePath=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString outDir=QString::fromLocal8Bit(argv[2]);
    const auto args=app.arguments();
    const bool smoke=args.contains(QStringLiteral("--smoke"));
    const bool quick=smoke||args.contains(QStringLiteral("--quick"));
    const bool compare=args.contains(QStringLiteral("--compare-v16"))&&!smoke;
    const QString stamp=QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString runName=QStringLiteral("CSE-WARP-V17-%1").arg(stamp);
    if(!QDir(outDir).mkpath(runName)){
        err<<"Cannot create benchmark output directory\n";
        return 3;
    }
    QDir directory(QDir(outDir).filePath(runName));
    if(!directory.mkpath("images"))return 3;
    QFile csvFile(directory.filePath("results.csv"));
    if(!csvFile.open(QIODevice::WriteOnly|QIODevice::Text))return 3;
    QTextStream csvOut(&csvFile);
    csvOut<<"backend,camera,width,height,texture_px,cold_ms,frames,mean_ms,median_ms,p95_ms,p99_ms,estimated_fps,submitted_triangles,status,error,image\n";
    auto loaded=v16::loadScene(scenePath);
    if(!loaded){err<<"Scene load failed: "<<loaded.error<<"\n";return 4;}
    // The normal and quick modes use the same full 2048 PBR assets as V17.
    // The isolated WARP CI smoke deliberately uses smaller textures.
    auto begin=Clock::now();
    auto built=v16::buildRuntime(loaded.scene,smoke?256:2048);
    const double buildMs=elapsedMs(begin);
    if(!built){err<<"Scene compilation failed: "<<built.error<<"\n";return 4;}
    const auto& runtime=built.runtime;
    if(runtime.cameras.isEmpty()){err<<"Scene has no camera\n";return 4;}
    updateLighting(runtime.daylightHour);
    begin=Clock::now();
    Stats shadowStats;
    const auto shadow=buildShadow(runtime.renderScene,shadowStats);
    const double shadowMs=elapsedMs(begin);
    QJsonObject system{
        {"engine","CSE WARP Beta X V17 D3D11 Raster"},
        {"scene",scenePath},{"sceneName",runtime.title},
        {"timestampUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"os",QSysInfo::prettyProductName()},
        {"cpuArchitecture",QSysInfo::currentCpuArchitecture()},
        {"cpuThreads",QThread::idealThreadCount()},
        {"qtVersion",QStringLiteral(QT_VERSION_STR)},
        {"materials",int(runtime.renderScene.mat.size())},
        {"triangles",int(runtime.renderScene.obj.size())},
        {"textureResolution",runtime.textureResolution},
        {"sceneBuildMs",buildMs},{"shadowBuildMs",shadowMs},
        {"mode",smoke?"ci_smoke":quick?"quick":"full"},
        {"compareV16",compare},
        {"timingDefinition","Synchronous renderDirectX11Preview: includes draw, GPU to CPU readback and buffer processing; excludes QImage GUI presentation, vsync and input latency."},
        {"hardwareNote","If M3D_V16_FORCE_WARP=1, this is a Microsoft software WARP test, not Intel HD620 GPU performance."}
    };
    if(!writeJson(directory.filePath("system.json"),system))return 3;
    QJsonArray results;
    int succeeded=0,skipped=0,failed=0;
    auto append=[&](QJsonObject record){
        const QString status=record.value("status").toString();
        if(status=="ok")++succeeded;else if(status=="skipped")++skipped;else ++failed;
        csvOut<<csv(record.value("backend").toString())<<','
              <<csv(record.value("camera").toString())<<','
              <<record.value("width").toInt()<<','<<record.value("height").toInt()<<','
              <<runtime.textureResolution<<','
              <<record.value("coldMs").toDouble()<<','
              <<record.value("frames").toInt()<<','
              <<record.value("meanMs").toDouble()<<','
              <<record.value("medianMs").toDouble()<<','
              <<record.value("p95Ms").toDouble()<<','
              <<record.value("p99Ms").toDouble()<<','
              <<record.value("estimatedFps").toDouble()<<','
              <<record.value("submittedTriangles").toDouble()<<','
              <<csv(status)<<','
              <<csv(record.value("error").toString())<<','
              <<csv(record.value("image").toString())<<"\n";
        csvOut.flush();
        results.append(record);
        out<<'['<<status<<"] "<<record.value("backend").toString()
           <<" / "<<record.value("camera").toString()
           <<" / "<<record.value("width").toInt()<<"x"<<record.value("height").toInt()
           <<" : "<<record.value("meanMs").toDouble()<<" ms mean\n";
        out.flush();
    };
    const QVector<QSize> sizes=smoke?QVector<QSize>{QSize(640,360)}
        :quick?QVector<QSize>{QSize(1366,768)}
              :QVector<QSize>{QSize(1366,768),QSize(1920,1080)};
    const int cameras=quick?1:std::min(2,int(runtime.cameras.size()));
    const int measuredFrames=smoke?2:quick?6:12;
    for(int cameraIndex=0;cameraIndex<cameras;++cameraIndex){
        const auto& preset=runtime.cameras[cameraIndex];
        for(const auto& size:sizes){
            const int w=size.width(),h=size.height();
            const auto camera=v16::makeCamera(preset,w,h);
            std::vector<uint32_t> pixels;
            Stats stats;
            std::wstring reason;
            QJsonObject rec{{"backend","V17_D3D11_Raster"},{"camera",preset.id},
                            {"width",w},{"height",h},{"frames",0}};
            begin=Clock::now();
            bool ok=renderDirectX11Preview(runtime.renderScene,shadow,camera,w,h,
                                           pixels,stats,reason);
            rec["coldMs"]=elapsedMs(begin);
            std::vector<double> frameTimes;
            if(ok){
                for(int i=0;i<measuredFrames;++i){
                    reason.clear();
                    begin=Clock::now();
                    ok=renderDirectX11Preview(runtime.renderScene,shadow,camera,w,h,
                                              pixels,stats,reason);
                    const double time=elapsedMs(begin);
                    if(!ok)break;
                    frameTimes.push_back(time);
                }
            }
            if(ok){
                const QString image=QStringLiteral("images/raster_%1_%2x%3.png").arg(preset.id).arg(w).arg(h);
                rec["image"]=image;
                if(!writeImage(directory.filePath(image),pixels,w,h)){
                    ok=false;
                    reason=L"Cannot save PNG";
                }
            }
            addTimings(rec,frameTimes);
            // Stats.tests is the number of triangles actually sent to D3D11
            // after optional opaque frustum culling, plus transparency.
            // This makes it possible to compare scene coverage and FPS.
            rec["submittedTriangles"]=double(stats.tests);
            rec["status"]=ok?"ok":"skipped";
            rec["error"]=ok?QString():QString::fromStdWString(reason);
            append(rec);
        }
        if(compare){
            const QSize size(1366,768);
            const auto camera=v16::makeCamera(preset,size.width(),size.height());
            std::vector<uint32_t> pixels;
            Stats stats;std::wstring reason;
            begin=Clock::now();
            const bool computeOk=renderCompute11Preview(runtime.renderScene,camera,
                size.width(),size.height(),pixels,stats,reason);
            const double ms=elapsedMs(begin);
            QJsonObject compute{{"backend","V16_D3D11_Compute"},{"camera",preset.id},
                {"width",size.width()},{"height",size.height()},{"coldMs",ms},
                {"status",computeOk?"ok":"skipped"},
                {"error",computeOk?QString():QString::fromStdWString(reason)}};
            addTimings(compute,{ms});
            if(computeOk){
                const QString image=QStringLiteral("images/compute_%1.png").arg(preset.id);
                if(writeImage(directory.filePath(image),pixels,size.width(),size.height()))
                    compute["image"]=image;
            }
            append(compute);
            std::atomic<uint64_t> generation{1};
            pixels.resize(size_t(size.width())*size.height());
            begin=Clock::now();
            auto cpu=render(runtime.renderScene,shadow,camera,size.width(),size.height(),
                pixels,unsigned(std::clamp(QThread::idealThreadCount(),1,8)),false,
                generation,1,1,false,false,false,3);
            const double cpuMs=elapsedMs(begin);
            QJsonObject cpuCase{{"backend","V16_CPU_Direct"},{"camera",preset.id},
                {"width",size.width()},{"height",size.height()},{"coldMs",cpuMs},
                {"status","ok"},{"engineMs",cpu.ms}};
            addTimings(cpuCase,{cpuMs});
            const QString image=QStringLiteral("images/cpu_%1.png").arg(preset.id);
            if(writeImage(directory.filePath(image),pixels,size.width(),size.height()))
                cpuCase["image"]=image;
            append(cpuCase);
        }
    }
    QJsonObject summary{{"engine","CSE WARP Beta X V17"},
                        {"run",runName},{"system",system},{"cases",results},
                        {"completedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"successful",succeeded},{"skipped",skipped},{"failed",failed}};
    if(!writeJson(directory.filePath("summary.json"),summary))return 5;
    out<<"Benchmark saved to "<<directory.absolutePath()<<"\n";
    out<<succeeded<<" completed, "<<skipped<<" skipped, "<<failed<<" failed.\n";
    return succeeded==0?6:failed?7:0;
}
