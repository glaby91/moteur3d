// CSE WARP V17: exact procedural PBR equivalence and heterogeneous-mip Compute.
// Run under Windows Microsoft WARP with the bundled Compute HLSL shader.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QElapsedTimer>
#include <cstdlib>
#include <vector>

using namespace m3d;

static bool setSerial(bool enabled){
#ifdef _WIN32
    return _putenv_s("M3D_V17_PBR_SERIAL",enabled?"1":"")==0;
#else
    return enabled?setenv("M3D_V17_PBR_SERIAL","1",1)==0:
                   unsetenv("M3D_V17_PBR_SERIAL")==0;
#endif
}
static bool sameTextures(const v16::Runtime& a,const v16::Runtime& b){
    if(a.renderScene.mat.size()!=b.renderScene.mat.size() ||
       a.renderScene.obj.size()!=b.renderScene.obj.size() ||
       a.renderScene.node.size()!=b.renderScene.node.size())return false;
    for(size_t i=0;i<a.renderScene.mat.size();++i){
        const Material& x=a.renderScene.mat[i];
        const Material& y=b.renderScene.mat[i];
        if(x.name!=y.name||x.colorMip.size()!=y.colorMip.size()||
           x.normalMip.size()!=y.normalMip.size()||
           x.roughnessMip.size()!=y.roughnessMip.size())return false;
        for(size_t k=0;k<x.colorMip.size();++k)
            if(x.colorMip[k].size!=y.colorMip[k].size ||
               x.colorMip[k].p!=y.colorMip[k].p)return false;
        for(size_t k=0;k<x.normalMip.size();++k)
            if(x.normalMip[k].size!=y.normalMip[k].size ||
               x.normalMip[k].p!=y.normalMip[k].p)return false;
        for(size_t k=0;k<x.roughnessMip.size();++k)
            if(x.roughnessMip[k].size!=y.roughnessMip[k].size ||
               x.roughnessMip[k].p!=y.roughnessMip[k].p)return false;
    }
    return true;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc<2){qCritical()<<"Usage: v17_pbr_compute_smoke.exe scene.json";return 2;}
    const auto loaded=v16::loadScene(QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath());
    if(!loaded){qCritical()<<"Scene loading:"<<loaded.error;return 3;}
    if(!setSerial(true)){qCritical()<<"Cannot enable serial PBR path";return 4;}
    QElapsedTimer timer;timer.start();
    const auto original=v16::buildRuntime(loaded.scene,512);
    const qint64 originalMs=timer.elapsed();
    if(!setSerial(false)){qCritical()<<"Cannot disable serial PBR path";return 4;}
    timer.restart();
    const auto parallel=v16::buildRuntime(loaded.scene,512);
    const qint64 parallelMs=timer.elapsed();
    if(!original||!parallel){
        qCritical()<<"PBR preparation failed"<<original.error<<parallel.error;return 5;
    }
    if(!sameTextures(original.runtime,parallel.runtime)){
        qCritical()<<"PBR parallelization changed exact color, normal or roughness mip values";
        return 6;
    }
    qInfo()<<"PBR bit-identical at 512px, serial"<<originalMs
           <<"ms, parallel"<<parallelMs<<"ms, materials"
           <<parallel.runtime.renderScene.mat.size();
    // The broken Compute code required a 512px mip in every material and
    // therefore failed for valid 256px scenes. Deliberately stress that path.
    timer.restart();
    const auto mixed=v16::buildRuntime(loaded.scene,256);
    if(!mixed){qCritical()<<"Low-resolution PBR scene build failed"<<mixed.error;return 7;}
    if(mixed.runtime.cameras.isEmpty()){qCritical()<<"Missing camera";return 8;}
    updateLighting(mixed.runtime.daylightHour);
    const auto camera=v16::makeCamera(mixed.runtime.cameras.first(),96,54);
    std::vector<uint32_t> pixels;
    Stats stats;
    std::wstring error;
    const bool ok=renderCompute11Preview(mixed.runtime.renderScene,camera,96,54,
                                          pixels,stats,error);
    if(!ok||pixels.size()!=size_t(96*54)){
        qCritical()<<"Compute failed with valid 256px PBR mip chains:"
                   <<QString::fromStdWString(error)<<"pixel count"<<pixels.size();
        return 9;
    }
    qInfo()<<"Compute heterogeneous mip atlas OK, 96x54, elapsed"<<timer.elapsed()
           <<"ms, output pixels"<<pixels.size();
    return 0;
}
