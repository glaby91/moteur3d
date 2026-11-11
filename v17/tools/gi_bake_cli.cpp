// V17.312 — deterministic offline GI table generator + real CPU benchmark.
// Usage: moteur3d_v17_gibake scene.json file.m3dgi [--smoke] [--full-gi] [--bench report.json]
#include "src/m3d_gi_bake.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>
using namespace m3d;
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc<3){
        std::fprintf(stderr,"Usage: moteur3d_v17_gibake scene.json file.m3dgi [--smoke] [--full-gi] [--bench report.json]\n");
        return 2;
    }
    const QString input=QString::fromLocal8Bit(argv[1]);
    const QString output=QString::fromLocal8Bit(argv[2]);
    bool smoke=false,bench=false,fullGi=false;
    QString report;
    for(int i=3;i<argc;i++){
        const QString arg=QString::fromLocal8Bit(argv[i]);
        if(arg=="--smoke")smoke=true;
        else if(arg=="--full-gi")fullGi=true;
        else if(arg=="--bench"&&i+1<argc){bench=true;report=QString::fromLocal8Bit(argv[++i]);}
        else {qCritical()<<"Unknown GI parameter"<<arg;return 2;}
    }
    QElapsedTimer timer;timer.start();
    const auto loaded=v16::loadScene(input);
    if(!loaded){qCritical()<<"GI scene load failed"<<loaded.error;return 1;}
    const auto built=v16::buildRuntime(loaded.scene,smoke?256:2048);
    if(!built){qCritical()<<"GI runtime build failed"<<built.error;return 1;}
    const double setupMs=double(timer.elapsed());
    const Scene& scene=built.runtime.renderScene;
    gibake::Options opt;
    if(smoke&&!fullGi){opt.nx=4;opt.ny=3;opt.nz=4;opt.rays=8;
              opt.frames=5;opt.firstHour=8.f;opt.stepHours=2.5f;}
    const uint64_t hash=gibake::signature(scene);
    std::vector<double> freshTimes,cacheTimes,interpolationTimes;
    gibake::Table table;
    std::string error;
    const unsigned rounds=bench?3u:1u;
    for(unsigned round=0;round<rounds;round++){
        QElapsedTimer clock;clock.start();
        gibake::Table candidate;
        if(!gibake::bake(scene,opt,candidate,error)){
            qCritical()<<"GI offline bake failed"<<QString::fromStdString(error);return 1;
        }
        freshTimes.push_back(double(clock.nsecsElapsed())/1e6);
        if(candidate.sceneHash!=hash||!candidate.valid()){
            qCritical()<<"GI bake generated bad signature/format";return 1;
        }
        table=std::move(candidate);
    }
    if(!gibake::writeFile(std::filesystem::path(output.toStdWString()),table,error)){
        qCritical()<<"GI cache write failed"<<QString::fromStdString(error);return 1;
    }
    for(unsigned round=0;round<(bench?7u:1u);round++){
        QElapsedTimer clock;clock.start();
        gibake::Table restored;
        if(!gibake::readFile(std::filesystem::path(output.toStdWString()),restored,error)||
           restored.sceneHash!=hash||restored.rgba!=table.rgba){
            qCritical()<<"GI cache mismatch"<<QString::fromStdString(error);return 1;
        }
        cacheTimes.push_back(double(clock.nsecsElapsed())/1e6);
    }
    const V center=table.origin+table.span*.5f;
    for(unsigned round=0;round<(bench?25u:1u);++round){
        QElapsedTimer clock;clock.start();
        volatile float sink=0.f;
        for(unsigned i=0;i<200;++i){
            const V result=gibake::sample(table,center,
                table.firstHour+(float(i%100)/99.f)*float(table.frames-1)*table.stepHours);
            sink=sink+result.x+result.y+result.z;
        }
        interpolationTimes.push_back(double(clock.nsecsElapsed())/1e6/200.0);
        (void)sink;
    }
    const V morning=gibake::sample(table,center,9.f);
    const V evening=gibake::sample(table,center,17.5f);
    const auto median=[](std::vector<double> v){
        std::sort(v.begin(),v.end());return v[v.size()/2];};
    const double coldBakeMedian=median(freshTimes);
    const double cacheLoadMedian=median(cacheTimes);
    const double interpMsMedian=median(interpolationTimes);
    QJsonObject result;
    result.insert("version",QStringLiteral("V17.%1").arg(CSE_WARP_BUILD_NUMBER));
    result.insert("scene",input);
    result.insert("mode",QStringLiteral("offline CPU real ray transport"));
    result.insert("device",QStringLiteral("GitHub Windows runner CPU, not Intel HD620"));
    result.insert("preset",fullGi?QStringLiteral("full-gi-volume-small-textures"):(smoke?QStringLiteral("smoke"):QStringLiteral("full")));
    result.insert("result",QStringLiteral("PASS"));
    result.insert("sceneBuildMs",setupMs);
    result.insert("bakeColdMedianMs",coldBakeMedian);
    result.insert("diskLoadMedianMs",cacheLoadMedian);
    result.insert("sampleInterpolatedMedianMs",interpMsMedian);
    result.insert("bakeRepetitions",int(rounds));
    result.insert("cacheLoadRepetitions",int(cacheTimes.size()));
    result.insert("interpolationRounds",int(interpolationTimes.size()));
    result.insert("probes",int(table.voxelCount()));
    result.insert("raysPerProbe",int(opt.rays));
    result.insert("bounces",int(opt.bounces));
    result.insert("hours",int(table.frames));
    result.insert("floatCount",int(table.rgba.size()));
    result.insert("sceneHash",QString::number(qulonglong(table.sceneHash),16));
    result.insert("morningR",double(morning.x));result.insert("morningG",double(morning.y));
    result.insert("morningB",double(morning.z));result.insert("eveningR",double(evening.x));
    result.insert("eveningG",double(evening.y));result.insert("eveningB",double(evening.z));
    const QJsonDocument doc(result);
    if(bench){
        QFile f(report);
        if(!f.open(QFile::WriteOnly|QFile::Truncate)||f.write(doc.toJson())<0){
            qCritical()<<"Cannot save benchmark report"<<report;return 1;
        }
    }
    qInfo().noquote()<<QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    return 0;
}
