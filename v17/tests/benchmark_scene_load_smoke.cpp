// Validate an arbitrary V17-compatible benchmark scene without Pavillon-01
// assumptions (the V16 scene_check fixture checks exactly 15 canopies).
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <set>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc<3){qCritical()<<"Expected scene path and minimum triangles";return 2;}
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<"Cannot load benchmark JSON:"<<loaded.error;return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);
    if(!built){qCritical()<<"Cannot build benchmark runtime:"<<built.error;return 4;}
    const size_t triangles=built.runtime.renderScene.obj.size();
    const size_t requested=QString::fromLocal8Bit(argv[2]).toULongLong();
    if(triangles<requested || built.runtime.cameras.size()<6 ||
       built.runtime.renderScene.mat.size()!=29 || built.runtime.objects.size()<1000){
        qCritical()<<"Invalid benchmark content: triangles="<<qulonglong(triangles)
                   <<"min="<<qulonglong(requested)
                   <<"cameras="<<built.runtime.cameras.size()
                   <<"materials="<<built.runtime.renderScene.mat.size()
                   <<"instances="<<built.runtime.objects.size();
        return 5;
    }
    // This scene adds seven local materials but preserves all 22 Pripyat
    // originals. Prove that the real JPEG mosaic was loaded, not replaced
    // with a uniform procedural fake.
    const int artIndex=built.runtime.materialIndex.value(
        QStringLiteral("cse_energetik_fresco"),-1);
    if(artIndex<0){qCritical()<<"No Energetik mosaic material";return 6;}
    const auto& image=built.runtime.renderScene.mat[size_t(artIndex)].colorMip;
    if(image.empty()||image[0].p.empty()){
        qCritical()<<"Energetik mosaic image has no texels";return 7;
    }
    std::set<uint32_t> colours(image[0].p.begin(),image[0].p.end());
    if(colours.size()<128){
        qCritical()<<"Energetik mosaic has no photographic color detail";return 8;
    }
    qInfo()<<"V17.317 authentic Energetik image verified:"
        <<image[0].p.size()<<"texels and"<<colours.size()<<"distinct colors";
    qInfo()<<"CSE WARP V17 benchmark scene validated:"
           <<qulonglong(triangles)<<"triangles,"
           <<built.runtime.objects.size()<<"instances,"
           <<built.runtime.cameras.size()<<"cameras,"
           <<built.runtime.renderScene.mat.size()<<"PBR materials";
    return 0;
}
