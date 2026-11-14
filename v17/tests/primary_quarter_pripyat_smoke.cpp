// Live BVH A/B test on the actual V17 generated civic/pool scene at 19:00.
// Runs CPU and D3D11 Compute (WARP in CI) using 100% vs 25% primary rays.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace m3d;

static bool quarter(bool enabled){
#ifdef _WIN32
    return _putenv_s("M3D_PRIMARY_RAYS_25",enabled?"1":"")==0;
#else
    return enabled?setenv("M3D_PRIMARY_RAYS_25","1",1)==0:
                   unsetenv("M3D_PRIMARY_RAYS_25")==0;
#endif
}
static bool exactDonors(const std::vector<uint32_t>& reference,
                        const std::vector<uint32_t>& reduced,int w,int h){
    if(reference.size()!=size_t(w)*h||reduced.size()!=reference.size())return false;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const size_t i=size_t(y)*w+x;
        const size_t donor=size_t(y&~1)*w+(x&~1);
        if(reduced[i]!=reduced[donor])return false;
        if((x&1)==0&&(y&1)==0&&reduced[i]!=reference[i])return false;
    }
    return true;
}
static bool saveImage(const QString& filename,const std::vector<uint32_t>& rgb,int w,int h){
    if(rgb.size()!=size_t(w)*h)return false;
    QImage out(w,h,QImage::Format_ARGB32);
    for(int y=0;y<h;++y)
        std::memcpy(out.scanLine(y),rgb.data()+size_t(y)*w,size_t(w)*4);
    return out.save(filename,"PNG");
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Usage: primary_quarter_pripyat_smoke scene.json output_directory";return 2;}
    const QString scenePath=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    const QString outPath=QString::fromLocal8Bit(argv[2]);
    if(!QDir().mkpath(outPath)){qCritical()<<"Cannot create output directory";return 3;}
    auto loaded=v16::loadScene(scenePath);
    if(!loaded){qCritical()<<"Cannot load Pripyat benchmark"<<loaded.error;return 4;}
    auto built=v16::buildRuntime(loaded.scene,128);
    if(!built||built.runtime.cameras.isEmpty()){
        qCritical()<<"Cannot prepare Pripyat BVH, camera or PBR"<<built.error;return 5;
    }
    constexpr int w=96,h=54;
    const auto& runtime=built.runtime;
    // Camera #8 in the authored civic scene: the 19h side view of the pool.
    const int cameraIndex=runtime.cameras.size()>7?7:0;
    const Camera camera=v16::makeCamera(runtime.cameras[cameraIndex],w,h);
    updateLighting(19.f);
    Stats shadowStats;
    const ShadowMap shadow=buildShadow(runtime.renderScene,shadowStats);
    std::atomic<uint64_t> generation{1u};
    std::vector<uint32_t> cpuReference,cpuReduced;
    if(!quarter(false))return 6;
    const Stats cpuFull=render(runtime.renderScene,shadow,camera,w,h,cpuReference,
                               2,false,generation,1,1,false,false,false,3);
    if(!quarter(true))return 7;
    const Stats cpu25=render(runtime.renderScene,shadow,camera,w,h,cpuReduced,
                             2,false,generation,1,1,false,false,false,3);
    quarter(false);
    if(cpuFull.primarySamples!=uint64_t(w*h)||
       cpu25.primarySamples!=uint64_t((w/2)*(h/2))||
       !exactDonors(cpuReference,cpuReduced,w,h)||
       !(cpu25.rays<cpuFull.rays)){
        qCritical()<<"Real scene CPU quarter primary rays are not correct"
                   <<cpuFull.primarySamples<<cpu25.primarySamples;
        return 8;
    }

    std::vector<uint32_t> gpuReference,gpuReduced;
    Stats warm, gpuFull, gpu25;
    std::wstring error;
    if(!renderCompute11Preview(runtime.renderScene,camera,w,h,gpuReference,warm,error)){
        qCritical()<<"Compute shader warmup failed"<<QString::fromStdWString(error);return 9;
    }
    error.clear();
    if(!renderCompute11Preview(runtime.renderScene,camera,w,h,gpuReference,gpuFull,error)){
        qCritical()<<"Compute reference failed"<<QString::fromStdWString(error);return 10;
    }
    if(!quarter(true))return 11;
    const bool reducedOk=renderCompute11Preview(runtime.renderScene,camera,w,h,
                                                gpuReduced,gpu25,error);
    quarter(false);
    if(!reducedOk||gpuFull.primarySamples!=uint64_t(w*h)||
       gpu25.primarySamples!=uint64_t((w/2)*(h/2))||
       !exactDonors(gpuReference,gpuReduced,w,h)){
        qCritical()<<"Real scene Compute quarter primary rays are not correct"
                   <<QString::fromStdWString(error)
                   <<gpuFull.primarySamples<<gpu25.primarySamples;
        return 12;
    }

    const QStringList filenames={
        "pripyat-pool-19h-cpu-reference.png",
        "pripyat-pool-19h-cpu-primary25.png",
        "pripyat-pool-19h-compute-reference.png",
        "pripyat-pool-19h-compute-primary25.png"
    };
    const std::vector<std::vector<uint32_t>> pictures={
        cpuReference,cpuReduced,gpuReference,gpuReduced
    };
    for(int i=0;i<filenames.size();++i)
        if(!saveImage(QDir(outPath).filePath(filenames[i]),pictures[size_t(i)],w,h))
            return 13;
    QFile csv(QDir(outPath).filePath("primary-ray-ab.csv"));
    if(!csv.open(QIODevice::WriteOnly|QIODevice::Text))return 14;
    QTextStream stream(&csv);
    stream<<"backend,mode,width,height,primarySamples,tracedRaysIncludingSecondary,elapsedMs\n";
    const auto row=[&](const char* backend,const char* mode,const Stats& stats){
        stream<<backend<<","<<mode<<","<<w<<","<<h<<","
              <<stats.primarySamples<<","<<stats.rays<<","<<stats.ms<<"\n";
    };
    row("CPU","full",cpuFull);row("CPU","quarter",cpu25);
    row("D3D11_Compute_WARP","full",gpuFull);
    row("D3D11_Compute_WARP","quarter",gpu25);
    qInfo()<<"Pripyat 19h native primary rays 100% -> 25% validated on CPU and Compute;"
           <<"CPU"<<cpuFull.ms<<"->"<<cpu25.ms<<"ms;"
           <<"Compute WARP"<<gpuFull.ms<<"->"<<gpu25.ms<<"ms";
    return 0;
}
