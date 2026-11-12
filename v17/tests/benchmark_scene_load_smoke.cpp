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
       built.runtime.renderScene.mat.size()!=30 || built.runtime.objects.size()<1000){
        qCritical()<<"Invalid benchmark content: triangles="<<qulonglong(triangles)
                   <<"min="<<qulonglong(requested)
                   <<"cameras="<<built.runtime.cameras.size()
                   <<"materials="<<built.runtime.renderScene.mat.size()
                   <<"instances="<<built.runtime.objects.size();
        return 5;
    }
    // Eight scene-local materials preserve all 22 original Pripyat IDs.
    // BOTH the user's exterior portrait and original hall Energetik fresco
    // must load through the real scene PBR/JPEG path (not flat fallbacks).
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
    const int facadeIndex=built.runtime.materialIndex.value(
        QStringLiteral("cse_personnage_facade"),-1);
    if(facadeIndex<0){qCritical()<<"User facade artwork material missing";return 9;}
    const auto& facade=built.runtime.renderScene.mat[size_t(facadeIndex)].colorMip;
    if(facade.empty()||facade[0].p.size()<10000){
        qCritical()<<"User facade image failed to decode";return 10;
    }
    const std::set<uint32_t> facadeColours(facade[0].p.begin(),facade[0].p.end());
    if(facadeColours.size()<100){
        qCritical()<<"User portrait was replaced with a flat material";return 11;
    }
    if(facade[0].p==image[0].p){
        qCritical()<<"Outdoor portrait and hall fresco are the same image";return 12;
    }
    // A mural must map its entire image to a face, never tile it by meters.
    const auto countFullUvFaces=[&](int materialId){
        int full=0;
        for(const auto& triangle:built.runtime.renderScene.obj){
            if(int(triangle.mat)!=materialId)continue;
            const auto inRange=[](float v){return v>=-0.001f&&v<=1.001f;};
            if(!inRange(triangle.uv0.x)||!inRange(triangle.uv0.y)||
               !inRange(triangle.uv1.x)||!inRange(triangle.uv1.y)||
               !inRange(triangle.uv2.x)||!inRange(triangle.uv2.y))continue;
            if((triangle.uv1.x==1.f||triangle.uv2.x==1.f)&&
               (triangle.uv1.y==1.f||triangle.uv2.y==1.f))full++;
        }
        return full;
    };
    if(countFullUvFaces(artIndex)<2||countFullUvFaces(facadeIndex)<2){
        qCritical()<<"Art mural faces must use non-repeating 0..1 UVs";
        return 13;
    }
    qInfo()<<"V17.324 both mural UV layouts cover their full image:";
    qInfo()<<"V17.323 two distinct image assets decoded:"
        <<"Energetik"<<image[0].p.size()<<"texels"<<colours.size()<<"colors;"
        <<"facade"<<facade[0].p.size()<<"texels"<<facadeColours.size()<<"colors";
    qInfo()<<"CSE WARP V17 benchmark scene validated:"
           <<qulonglong(triangles)<<"triangles,"
           <<built.runtime.objects.size()<<"instances,"
           <<built.runtime.cameras.size()<<"cameras,"
           <<built.runtime.renderScene.mat.size()<<"PBR materials";
    return 0;
}
