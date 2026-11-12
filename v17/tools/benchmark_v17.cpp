// CSE WARP V17: benchmark the actual shared V16 scene through V17's D3D11
// raster frontend. These are render + GPU readback timings, not on-screen FPS.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QVector>
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
double average(const std::vector<double>& values){
    return values.empty()?0.0:std::accumulate(values.begin(),values.end(),0.0)/double(values.size());
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
    QCoreApplication::setApplicationVersion(QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER));
    QString buildNumber=QStringLiteral("0 (local)");
    QFile buildFile(QCoreApplication::applicationDirPath()
        +QStringLiteral("/BUILD_NUMBER.txt"));
    if(buildFile.open(QIODevice::ReadOnly|QIODevice::Text)){
        const auto value=QString::fromUtf8(buildFile.readAll()).trimmed();
        if(!value.isEmpty())buildNumber=value;
    }
    QString sourceRevision=QStringLiteral("local");
    QFile revFile(QCoreApplication::applicationDirPath()
        +QStringLiteral("/REVISION.txt"));
    if(revFile.open(QIODevice::ReadOnly|QIODevice::Text))
        sourceRevision=QString::fromUtf8(revFile.readAll()).trimmed().left(12);
    QTextStream out(stdout),err(stderr);
    if(argc<3){
        err<<"Usage: moteur3d_v17_benchmark.exe scene.json output_dir [--quick] [--compare-v16] [--smoke] [--readback-depth 1|2|3] [--compare-readback] [--all-cameras] [--camera-id bassin] [--hour 19]\n";
        return 2;
    }
    const QString scenePath=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString outDir=QString::fromLocal8Bit(argv[2]);
    const auto args=app.arguments();
    // Quality comes from the embedding V17 UI or directly from a host
    // application; benchmark and interactive renderer must use the same
    // native DLL settings even though their processes are different.
    auto quality=rasterQualitySettings();
    bool qualityProvided=false;
    const auto readQualityOption=[&](const char* name,uint32_t& field)->bool{
        const int index=args.indexOf(QString::fromLatin1(name));
        if(index<0)return true;
        qualityProvided=true;
        bool parsed=false;
        const int value=index+1<args.size()?args[index+1].toInt(&parsed):0;
        if(!parsed||value<0){
            err<<"Invalid raster quality parameter: "<<name<<"\n";
            return false;
        }
        field=uint32_t(value);
        return true;
    };
    uint32_t culling=quality.frustumCulling?1u:0u;
    uint32_t dynamicLod=quality.dynamicLod?1u:0u;
    if(!readQualityOption("--raster-msaa",quality.msaaSamples)||
       !readQualityOption("--raster-anisotropy",quality.anisotropy)||
       !readQualityOption("--raster-lod-percent",quality.lodQualityPercent)||
       !readQualityOption("--raster-culling",culling)||
       !readQualityOption("--raster-lod-enabled",dynamicLod)||
       culling>1||dynamicLod>1){
        err<<"Invalid quality flags; expected MSAA 1/2/4/8, anisotropy 1/2/4/8/16, LOD 50..200, culling/LOD 0/1\n";
        return 2;
    }
    quality.frustumCulling=culling!=0;
    quality.dynamicLod=dynamicLod!=0;
    if(qualityProvided){
        std::wstring qualityError;
        if(!setRasterQualitySettings(quality,qualityError)){
            err<<QString::fromStdWString(qualityError)<<"\n";
            return 2;
        }
    }
    const bool allCameras=args.contains(QStringLiteral("--all-cameras"));
    const int hourOption=args.indexOf(QStringLiteral("--hour"));
    float benchmarkHour=-1.f;
    if(hourOption>=0){
        bool ok=false;
        benchmarkHour=hourOption+1<args.size()?args[hourOption+1].toFloat(&ok):-1.f;
        if(!ok||benchmarkHour<0.f||benchmarkHour>=24.f){
            err<<"--hour expects a number from 0 to below 24\n";return 2;
        }
    }
    const int cameraOption=args.indexOf(QStringLiteral("--camera-id"));
    const QString requestedCamera=cameraOption>=0&&cameraOption+1<args.size()
        ?args[cameraOption+1]:QString();
    const bool smoke=args.contains(QStringLiteral("--smoke"));
    const bool quick=smoke||args.contains(QStringLiteral("--quick"));
    const bool compare=args.contains(QStringLiteral("--compare-v16"))&&!smoke;
    const bool compareReadback=args.contains(QStringLiteral("--compare-readback"));
    int readbackDepth=1;
    const int depthOption=args.indexOf(QStringLiteral("--readback-depth"));
    if(depthOption>=0){
        if(depthOption+1>=args.size()){
            err<<"--readback-depth expects 1, 2 or 3\n";return 2;
        }
        bool parsed=false;
        readbackDepth=args[depthOption+1].toInt(&parsed);
        if(!parsed||readbackDepth<1||readbackDepth>3){
            err<<"--readback-depth must be 1, 2 or 3\n";return 2;
        }
    }
    const QString stamp=QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString runName=QStringLiteral("CSE-WARP-V17.%1-%2").arg(CSE_WARP_BUILD_NUMBER).arg(stamp);
    if(!QDir(outDir).mkpath(runName)){
        err<<"Cannot create benchmark output directory\n";
        return 3;
    }
    QDir directory(QDir(outDir).filePath(runName));
    if(!directory.mkpath("images"))return 3;
    QFile csvFile(directory.filePath("results.csv"));
    if(!csvFile.open(QIODevice::WriteOnly|QIODevice::Text))return 3;
    QTextStream csvOut(&csvFile);
    csvOut<<"backend,camera,width,height,texture_px,readback_depth,cold_ms,frames,mean_ms,median_ms,p95_ms,p99_ms,estimated_fps,stream_mean_ms,stream_fps,submitted_triangles,cold_readback_ms,mean_readback_ms,gpu_draw_calls,instanced_boxes,instanced_ellipsoids,instanced_foliage,gpu_timing_valid,gpu_draw_ms,gpu_transfer_ms,gpu_total_ms,cpu_submit_ms,cpu_map_wait_ms,cpu_pixel_convert_ms,pixel_identical,status,error,image\n";
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
    updateLighting(benchmarkHour>=0.f?benchmarkHour:runtime.daylightHour);
    begin=Clock::now();
    Stats shadowStats;
    const auto shadow=buildShadow(runtime.renderScene,shadowStats);
    const double shadowMs=elapsedMs(begin);
    QJsonObject system{
        {"engine","CSE WARP Beta X V17 D3D11 Raster"},
        {"version",QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER)},
        {"buildNumber",QString::number(CSE_WARP_BUILD_NUMBER)},{"revision",sourceRevision},
        {"scene",scenePath},{"sceneName",runtime.title},
        {"lightingHour",benchmarkHour>=0.f?benchmarkHour:runtime.daylightHour},
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
        {"readbackDepth",readbackDepth},
        {"compareReadback",compareReadback},
        {"pixelShaderOptimized",qgetenv("M3D_V17_DISABLE_SHADER_OPT")!="1"},
        {"fastPixelSwizzle",qgetenv("M3D_V17_DISABLE_FAST_SWIZZLE")!="1"},
        {"rasterQuality",QJsonObject{
            {"msaaSamples",int(quality.msaaSamples)},
            {"anisotropy",int(quality.anisotropy)},
            {"lodQualityPercent",int(quality.lodQualityPercent)},
            {"frustumCulling",quality.frustumCulling},
            {"dynamicLod",quality.dynamicLod}
        }},
        {"timingDefinition","GPU ms: D3D11 timestamp/disjoint queries (real hardware GPU time if available). cpuMapWaitMs: blocking Map; cpuPixelConvertMs: swizzle. meanMs: submit-to-receive latency; streamMeanMs: measured wall-time/frames, including pipeline fill and drain. Excludes GUI presentation/vsync."},
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
              <<record.value("readbackDepth").toInt(1)<<','
              <<record.value("coldMs").toDouble()<<','
              <<record.value("frames").toInt()<<','
              <<record.value("meanMs").toDouble()<<','
              <<record.value("medianMs").toDouble()<<','
              <<record.value("p95Ms").toDouble()<<','
              <<record.value("p99Ms").toDouble()<<','
              <<record.value("estimatedFps").toDouble()<<','
              <<record.value("streamMeanMs").toDouble()<<','
              <<record.value("streamFps").toDouble()<<','
              <<record.value("submittedTriangles").toDouble()<<','
              <<record.value("coldReadbackMs").toDouble()<<','
              <<record.value("meanReadbackMs").toDouble()<<','
              <<record.value("gpuDrawCalls").toDouble()<<','
              <<record.value("gpuBoxInstances").toDouble()<<','
              <<record.value("gpuEllipseInstances").toDouble()<<','
              <<record.value("gpuFoliageInstances").toDouble()<<','
              <<record.value("gpuTimingValid").toBool()<<','
              <<record.value("gpuDrawMs").toDouble()<<','
              <<record.value("gpuTransferMs").toDouble()<<','
              <<record.value("gpuTotalMs").toDouble()<<','
              <<record.value("cpuSubmitMs").toDouble()<<','
              <<record.value("cpuMapWaitMs").toDouble()<<','
              <<record.value("cpuPixelConvertMs").toDouble()<<','
              <<record.value("pixelIdentical").toBool()<<','
              <<csv(status)<<','
              <<csv(record.value("error").toString())<<','
              <<csv(record.value("image").toString())<<"\n";
        csvOut.flush();
        results.append(record);
        out<<'['<<status<<"] "<<record.value("backend").toString()
           <<" / "<<record.value("camera").toString()
           <<" / "<<record.value("width").toInt()<<"x"<<record.value("height").toInt()
           <<" : "<<record.value("meanMs").toDouble()<<" ms mean"
           <<(status=="ok"?QString():QStringLiteral(" — error: ")+record.value("error").toString())
           <<"\n";
        out.flush();
    };
    const QVector<QSize> sizes=smoke?QVector<QSize>{QSize(640,360)}
        :quick?QVector<QSize>{QSize(1366,768)}
              :QVector<QSize>{QSize(1366,768),QSize(1920,1080)};
    // In visual QA mode, capture EVERY preset as genuine GPU output
    // (facade, mosaic close-up, hall/fresco, pool, changing rooms, statue).
    // Historical benchmarks retain their exact old camera count.
    const int cameras=allCameras?int(runtime.cameras.size()):
        quick?1:std::min(2,int(runtime.cameras.size()));
    QVector<int> selectedCameras;
    if(!requestedCamera.isEmpty()){
        for(int i=0;i<runtime.cameras.size();++i)
            if(runtime.cameras[i].id==requestedCamera)selectedCameras.push_back(i);
        if(selectedCameras.isEmpty()){
            err<<"Unknown --camera-id "<<requestedCamera<<"\n";return 2;
        }
    }else{
        for(int i=0;i<cameras;++i)selectedCameras.push_back(i);
    }
    const int measuredFrames=smoke?2:quick?6:12;
    for(const int cameraIndex:selectedCameras){
        const auto& preset=runtime.cameras[cameraIndex];
        for(const auto& size:sizes){
            const int w=size.width(),h=size.height();
            const auto camera=v16::makeCamera(preset,w,h);
            const QVector<int> depths=compareReadback?QVector<int>{1,2,3}
                :QVector<int>{readbackDepth};
            std::vector<uint32_t> baselinePixels;
            for(const int depth:depths){
                std::vector<uint32_t> pixels;
                Stats stats;
                std::wstring reason;
                QJsonObject rec{{"backend","V17_D3D11_Raster"},{"camera",preset.id},
                                {"width",w},{"height",h},{"frames",0},
                                {"readbackDepth",depth}};
                // Use the unchanged synchronous path for cold start. It is
                // explicitly outside the pipelined throughput measurement.
                begin=Clock::now();
                bool ok=renderDirectX11Preview(runtime.renderScene,shadow,camera,w,h,
                                               pixels,stats,reason);
                rec["coldMs"]=elapsedMs(begin);
                rec["coldReadbackMs"]=ok?stats.postMs:0.0;
                std::vector<double> latencies,readbackTimes,drawTimes,transferTimes,totalGpuTimes,
                                    cpuSubmits,cpuWaits,cpuConversions;
                auto collect=[&]{
                    latencies.push_back(stats.ms);
                    readbackTimes.push_back(stats.postMs);
                    cpuSubmits.push_back(stats.cpuSubmitMs);
                    cpuWaits.push_back(stats.cpuMapWaitMs);
                    cpuConversions.push_back(stats.cpuPixelConvertMs);
                    if(stats.gpuTimingValid){
                        drawTimes.push_back(stats.gpuDrawMs);
                        transferTimes.push_back(stats.gpuTransferMs);
                        totalGpuTimes.push_back(stats.gpuTotalMs);
                    }
                };
                double streamDuration=0.;
                if(ok){
                    const auto streamStarted=Clock::now();
                    if(depth==1){
                        for(int i=0;i<measuredFrames;++i){
                            reason.clear();
                            ok=renderDirectX11Preview(runtime.renderScene,shadow,camera,w,h,
                                                      pixels,stats,reason);
                            if(!ok)break;
                            collect();
                        }
                    }else{
                        // Submit depth frames ahead and receive the oldest,
                        // then drain. The COPY commands use distinct staging
                        // textures; the shared render target is safely reused
                        // by ordered D3D11 immediate-context commands.
                        int queued=0,received=0;
                        for(int i=0;i<measuredFrames;++i){
                            reason.clear();
                            ok=enqueueDirectX11Preview(runtime.renderScene,shadow,camera,
                                                       w,h,unsigned(depth),reason);
                            if(!ok)break;
                            ++queued;
                            if(queued-received>=depth){
                                ok=receiveDirectX11Preview(pixels,stats,reason);
                                if(!ok)break;
                                ++received;
                                collect();
                            }
                        }
                        // Drain even after submission errors: no stale frames
                        // may survive into another camera/depth benchmark.
                        while(received<queued){
                            std::wstring receiveError;
                            const bool drained=receiveDirectX11Preview(pixels,stats,receiveError);
                            if(!drained){
                                ok=false;
                                if(reason.empty())reason=receiveError;
                                break;
                            }
                            ++received;
                            collect();
                        }
                    }
                    streamDuration=elapsedMs(streamStarted);
                }
                addTimings(rec,latencies);
                rec["meanReadbackMs"]=average(readbackTimes);
                rec["gpuTimingValid"]=!totalGpuTimes.empty();
                rec["gpuDrawMs"]=average(drawTimes);
                rec["gpuTransferMs"]=average(transferTimes);
                rec["gpuTotalMs"]=average(totalGpuTimes);
                rec["cpuSubmitMs"]=average(cpuSubmits);
                rec["cpuMapWaitMs"]=average(cpuWaits);
                rec["cpuPixelConvertMs"]=average(cpuConversions);
                if(!latencies.empty()&&streamDuration>0){
                    rec["streamMeanMs"]=streamDuration/double(latencies.size());
                    rec["streamFps"]=double(latencies.size())*1000.0/streamDuration;
                }
                if(ok&&compareReadback&&depth==1)baselinePixels=pixels;
                const bool identical=!compareReadback||depth==1||pixels==baselinePixels;
                rec["pixelIdentical"]=identical;
                if(ok&&!identical){ok=false;reason=L"GPU buffered readback changed pixel values";}
                if(ok){
                    const QString image=QStringLiteral("images/raster_%1_%2x%3_depth%4.png")
                        .arg(preset.id).arg(w).arg(h).arg(depth);
                    rec["image"]=image;
                    if(!writeImage(directory.filePath(image),pixels,w,h)){
                        ok=false;
                        reason=L"Cannot save PNG";
                    }
                }
                rec["submittedTriangles"]=double(stats.tests);
                rec["gpuDrawCalls"]=double(stats.gpuDrawCalls);
                rec["gpuBoxInstances"]=double(stats.gpuBoxInstances);
                rec["gpuEllipseInstances"]=double(stats.gpuEllipseInstances);
                rec["gpuFoliageInstances"]=double(stats.gpuFoliageInstances);
                rec["status"]=ok?"ok":"skipped";
                rec["error"]=ok?QString():QString::fromStdWString(reason);
                append(rec);
            }
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
                        {"version",QStringLiteral("17.%1").arg(CSE_WARP_BUILD_NUMBER)},
                        {"buildNumber",QString::number(CSE_WARP_BUILD_NUMBER)},{"revision",sourceRevision},
                        {"run",runName},{"system",system},{"cases",results},
                        {"completedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                        {"successful",succeeded},{"skipped",skipped},{"failed",failed}};
    if(!writeJson(directory.filePath("summary.json"),summary))return 5;
    out<<"Benchmark saved to "<<directory.absolutePath()<<"\n";
    out<<succeeded<<" completed, "<<skipped<<" skipped, "<<failed<<" failed.\n";
    return succeeded==0?6:failed?7:0;
}
