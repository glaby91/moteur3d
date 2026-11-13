// Validate an arbitrary V17-compatible benchmark scene without Pavillon-01
// assumptions (the V16 scene_check fixture checks exactly 15 canopies).
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include "src/m3d_walk_controller.h"
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
       built.runtime.renderScene.mat.size()<44 || built.runtime.objects.size()<1000){
        qCritical()<<"Invalid benchmark content: triangles="<<qulonglong(triangles)
                   <<"min="<<qulonglong(requested)
                   <<"cameras="<<built.runtime.cameras.size()
                   <<"materials="<<built.runtime.renderScene.mat.size()
                   <<"instances="<<built.runtime.objects.size();
        return 5;
    }
    // Preserve the original 22 Pripyat materials plus scene-local materials; the scene catalog can grow.
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
    // Every artwork must be mapped exactly ONCE, not repeated as a normal
    // concrete or tile texture. Validate CPU/Compute UVs; D3D11 source-box
    // instancing mirrors these coordinates with dedicated uvMode=2.
    const auto fullImageOnce=[&](int material)->bool{
        size_t triangles=0;
        for(const m3d::Obj& o:built.runtime.renderScene.obj){
            if(o.mat!=material)continue;
            if(o.kind!=m3d::Kind::Triangle)return false;
            for(const m3d::V uv:{o.uv0,o.uv1,o.uv2}){
                if(uv.x<-.001f||uv.x>1.001f||
                   uv.y<-.001f||uv.y>1.001f)return false;
            }
            triangles++;
        }
        return triangles==12;
    };
    if(!fullImageOnce(artIndex)||!fullImageOnce(facadeIndex)){
        qCritical()<<"Artwork UV exceeds 0..1: image repeats over its wall";
        return 13;
    }
    // Real BVH-level first-person navigation, not only a 2D plan check.
    // Traverse the exterior main entrance, then climb the public hall
    // staircase to verify that the 4.5 m mezzanine has a real stairwell.
    m3d::walk::State outside;
    if(!m3d::walk::spawn(built.runtime.renderScene,{0.f,2.3f,28.f},outside)){
        qCritical()<<"V17 FPS cannot spawn on exterior front plaza";return 14;
    }
    m3d::walk::advance(built.runtime.renderScene,outside,{0.f,0.f,-40.f},.1f);
    if(outside.feet.z>-6.5f||!outside.grounded){
        qCritical()<<"V17 FPS cannot cross building entrance to interior"
                   <<outside.feet.z;return 15;
    }
    m3d::walk::State hallStair;
    if(!m3d::walk::spawn(built.runtime.renderScene,{-15.f,2.5f,-17.65f},hallStair)){
        qCritical()<<"V17 FPS cannot spawn at hall stair foot";return 16;
    }
    m3d::walk::advance(built.runtime.renderScene,hallStair,{0.f,0.f,8.2f},.1f);
    if(hallStair.feet.y<4.15f||hallStair.feet.z<-10.5f){
        qCritical()<<"V17 FPS cannot climb hall stairs to gallery:"
                   <<hallStair.feet.y<<hallStair.feet.z;return 17;
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
    qInfo()<<"V17 FPS outdoor entrance and staircase passed";
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
