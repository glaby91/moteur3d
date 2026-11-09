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
#include <QJsonValue>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>
#include <QtGlobal>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <set>
#include <vector>

using namespace m3d;
namespace {
QString csvField(QString s) {
    s.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"") + s + QStringLiteral("\"");
}
bool writeJson(const QString& path,const QJsonObject& value) {
    QFile f(path);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))return false;
    return f.write(QJsonDocument(value).toJson(QJsonDocument::Indented))>0;
}
bool writePng(const QString& path,const std::vector<uint32_t>& pixels,int w,int h) {
    if(pixels.size()!=size_t(w)*size_t(h))return false;
    QImage img(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)std::memcpy(img.scanLine(y),pixels.data()+size_t(y)*w,size_t(w)*4);
    return img.save(path,"PNG");
}
QJsonObject values(const Stats& st) {
    return {{"rays",double(st.rays)},{"triangleTests",double(st.tests)},
            {"aabb",double(st.aabb)},{"bvhNodes",double(st.nodes)},
            {"hits",double(st.hits)},{"sky",double(st.sky)},
            {"texels",double(st.texels)},{"shadowRays",double(st.shadowRays)},
            {"engineMs",st.ms},{"postMs",st.postMs}};
}
QString caseId(const QString& backend,const QString& camera,int w,int h,int threads,const QString& mode){
    return QStringLiteral("%1_%2_%3x%4_t%5_%6").arg(backend,camera).arg(w).arg(h).arg(threads).arg(mode);
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    QTextStream out(stdout),err(stderr);
    if(argc<3){err<<"Usage: moteur3d_v16_benchmark.exe scene.json dossier_sortie [--quick] [--cpu-only]\n";return 2;}
    const QString scenePath=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString outputRoot=QString::fromLocal8Bit(argv[2]);
    const QStringList arguments=app.arguments();
    const bool quick=arguments.contains(QStringLiteral("--quick"));
    const bool cpuOnly=arguments.contains(QStringLiteral("--cpu-only"))||quick;
    const QString runName=QStringLiteral("Pavillon01-V16-%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
    if(!QDir(outputRoot).mkpath(runName)){err<<"Cannot create benchmark output directory\n";return 3;}
    const QDir directory(QDir(outputRoot).absoluteFilePath(runName));
    if(!directory.mkpath("images")){err<<"Cannot create images directory\n";return 3;}
    QFile csvFile(directory.filePath("results.csv")), progressFile(directory.filePath("progress.jsonl"));
    if(!csvFile.open(QIODevice::WriteOnly|QIODevice::Text)||!progressFile.open(QIODevice::WriteOnly|QIODevice::Text)){
        err<<"Cannot write benchmark reports\n";return 3;
    }
    QTextStream csv(&csvFile),progress(&progressFile);
    csv<<"id,backend,mode,camera,width,height,threads,samples,status,wall_ms,engine_ms,rays,tests,aabb,hits,texels,shadow_rays,error,image\n";
    auto loaded=v16::loadScene(scenePath);
    if(!loaded){err<<"Invalid scene: "<<loaded.error<<"\n";return 4;}
    auto startBuild=std::chrono::steady_clock::now();
    auto built=v16::buildRuntime(loaded.scene,quick?128:1024);
    const double loadMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-startBuild).count();
    if(!built){err<<"Scene runtime error: "<<built.error<<"\n";return 4;}
    const auto& scene=built.runtime.renderScene;
    const auto& cameras=built.runtime.cameras;
    if(cameras.isEmpty()){err<<"No camera\n";return 4;}
    updateLighting(built.runtime.daylightHour);
    Stats shadowStats;
    const auto shadowStart=std::chrono::steady_clock::now();
    const auto shadow=buildShadow(scene,shadowStats);
    const double shadowMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-shadowStart).count();
    const auto stamp=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QJsonObject manifest{
        {"scene",scenePath},{"sceneName",built.runtime.title},{"timestampUtc",stamp},
        {"os",QSysInfo::prettyProductName()},{"cpuArch",QSysInfo::currentCpuArchitecture()},
        {"idealThreads",QThread::idealThreadCount()},{"qtVersion",QStringLiteral(QT_VERSION_STR)},
        {"geometryObjects",int(built.runtime.objects.size())},{"triangles",int(scene.obj.size())},
        {"bvhNodes",int(scene.node.size())},{"materials",int(scene.mat.size())},
        {"textureResolution",built.runtime.textureResolution},{"daylightHour",built.runtime.daylightHour},
        {"buildSceneMs",loadMs},{"buildShadowMs",shadowMs},
        {"quick",quick},{"cpuOnly",cpuOnly},
        {"note","GPU Compute is a geometry-only preview; it is not equivalent to CPU PBR."}
    };
    if(!writeJson(directory.filePath("system.json"),manifest)){err<<"Cannot write system metadata\n";return 3;}
    QJsonArray records;
    int success=0,skipped=0,failed=0;
    auto record=[&](QJsonObject r){
        const QString id=r.value("id").toString();
        QString fileName=r.value("image").toString(),status=r.value("status").toString();
        csv<<csvField(id)<<','<<csvField(r.value("backend").toString())<<','
           <<csvField(r.value("mode").toString())<<','<<csvField(r.value("camera").toString())<<','
           <<r.value("width").toInt()<<','<<r.value("height").toInt()<<','
           <<r.value("threads").toInt()<<','<<r.value("samples").toInt()<<','
           <<csvField(status)<<','<<QString::number(r.value("wallMs").toDouble(),'f',3)<<','
           <<QString::number(r.value("engineMs").toDouble(),'f',3)<<','
           <<QString::number(r.value("rays").toDouble(),'f',0)<<','
           <<QString::number(r.value("triangleTests").toDouble(),'f',0)<<','
           <<QString::number(r.value("aabb").toDouble(),'f',0)<<','
           <<QString::number(r.value("hits").toDouble(),'f',0)<<','
           <<QString::number(r.value("texels").toDouble(),'f',0)<<','
           <<QString::number(r.value("shadowRays").toDouble(),'f',0)<<','
           <<csvField(r.value("error").toString())<<','<<csvField(fileName)<<"\n";
        csv.flush();
        progress<<QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact))<<"\n";
        progress.flush();
        records.append(r);
        if(status=="ok")++success;else if(status=="skipped")++skipped;else ++failed;
        out<<QStringLiteral("[%1] %2 — %3 ms\n").arg(status,id).arg(r.value("wallMs").toDouble(),0,'f',1);
        out.flush();
    };
    const QVector<QSize> resolutions=quick?QVector<QSize>{QSize(320,180)}:
        QVector<QSize>{QSize(400,225),QSize(800,450),QSize(1280,720)};
    const QVector<int> threads=quick?QVector<int>{1}:QVector<int>{1,2,4,8,16};
    const int cameraCount=quick?1:int(cameras.size());
    const std::atomic<uint64_t> generation{1};
    for(int cameraIndex=0;cameraIndex<cameraCount;++cameraIndex){
        const auto& preset=cameras[cameraIndex];
        for(const auto& size:resolutions){
            const auto cam=v16::makeCamera(preset,size.width(),size.height());
            for(const int n:threads){
                std::vector<uint32_t> pixels(size_t(size.width())*size_t(size.height()));
                const auto t0=std::chrono::steady_clock::now();
                const auto stats=render(scene,shadow,cam,size.width(),size.height(),pixels,
                                        unsigned(n),false,generation,1,1,false,false,false,3);
                const double wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
                QJsonObject r=values(stats);
                const QString id=caseId("cpu",preset.id,size.width(),size.height(),n,"pbr");
                const bool save=quick||(n==threads.back()&&size==resolutions.back());
                QString image;
                if(save){
                    image=QStringLiteral("images/%1.png").arg(id);
                    if(!writePng(directory.filePath(image),pixels,size.width(),size.height()))
                        image.clear();
                }
                r["id"]=id;r["backend"]="cpu";r["mode"]="raytrace_pbr";r["camera"]=preset.id;
                r["width"]=size.width();r["height"]=size.height();r["threads"]=n;
                r["samples"]=1;r["wallMs"]=wallMs;r["status"]="ok";r["image"]=image;
                record(r);
            }
        }
        if(!quick){
            const QSize size(800,450);
            const auto cam=v16::makeCamera(preset,size.width(),size.height());
            std::vector<uint32_t> pixels(size_t(size.width())*size_t(size.height()));
            const auto t0=std::chrono::steady_clock::now();
            const auto stats=render(scene,shadow,cam,size.width(),size.height(),pixels,
                                    unsigned(std::max(1,QThread::idealThreadCount())),false,generation,1,4,false,false,false,3);
            const double wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
            QJsonObject r=values(stats);
            const QString id=caseId("cpu",preset.id,size.width(),size.height(),QThread::idealThreadCount(),"4spp");
            QString image=QStringLiteral("images/%1.png").arg(id);
            if(!writePng(directory.filePath(image),pixels,size.width(),size.height()))image.clear();
            r["id"]=id;r["backend"]="cpu";r["mode"]="raytrace_pbr_4spp";r["camera"]=preset.id;
            r["width"]=size.width();r["height"]=size.height();r["threads"]=QThread::idealThreadCount();
            r["samples"]=4;r["wallMs"]=wallMs;r["status"]="ok";r["image"]=image;
            record(r);
        }
        if(!cpuOnly){
            const QSize gpuSize(800,450);
            const auto cam=v16::makeCamera(preset,gpuSize.width(),gpuSize.height());
            Stats stats;std::wstring why;std::vector<uint32_t> pixels;
            const auto t0=std::chrono::steady_clock::now();
            const bool ok=renderCompute11Preview(scene,cam,gpuSize.width(),gpuSize.height(),pixels,stats,why);
            const double wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
            QJsonObject r=values(stats);
            const QString id=caseId("gpu",preset.id,gpuSize.width(),gpuSize.height(),0,"compute");
            QString image;
            if(ok){
                image=QStringLiteral("images/%1.png").arg(id);
                if(!writePng(directory.filePath(image),pixels,gpuSize.width(),gpuSize.height()))image.clear();
            }
            r["id"]=id;r["backend"]="gpu";r["mode"]="compute_geometry_preview";r["camera"]=preset.id;
            r["width"]=gpuSize.width();r["height"]=gpuSize.height();r["threads"]=0;
            r["samples"]=1;r["wallMs"]=wallMs;r["status"]=ok?"ok":"skipped";
            r["error"]=ok?QString():QString::fromStdWString(why);
            r["image"]=image;
            record(r);
        }
    }
    const QJsonObject summary{{"run",runName},{"startedUtc",stamp},
        {"completedUtc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"successful",success},{"skipped",skipped},{"failed",failed},
        {"tests",records},{"system",manifest}};
    if(!writeJson(directory.filePath("summary.json"),summary)){
        err<<"Could not write summary.json\n";return 5;
    }
    out<<"Benchmark saved to "<<directory.absolutePath()<<"\n";
    out<<success<<" completed, "<<skipped<<" skipped, "<<failed<<" failed.\n";
    return failed?6:0;
}
