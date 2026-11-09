#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if(argc!=2){qCritical()<<"Expected calibrated facade scene.json";return 2;}
 auto data=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
 if(!data){qCritical().noquote()<<data.error;return 1;}
 auto built=m3d::v16::buildRuntime(data.scene,64);
 if(!built){qCritical().noquote()<<built.error;return 1;}
 const auto& r=built.runtime;
 if(r.objects.size()!=16||r.cameras.size()!=3||r.renderScene.mat.size()!=3){
  qCritical()<<"Incorrect Photo2Scene scene structure:" <<r.objects.size()
             <<r.cameras.size()<<r.renderScene.mat.size();
  return 1;
 }
 if(!r.renderScene.finiteGround||r.renderScene.obj.empty()||
    r.renderScene.node.empty()||r.renderScene.root<0){
  qCritical()<<"Photo2Scene geometry/BVH invalid";return 1;
 }
 m3d::Stats st;
 m3d::Hit h;
 const m3d::V eye=r.cameras[0].eye,target{0.f,1.12664f,0.f};
 m3d::closest(r.renderScene,m3d::Ray{eye,m3d::norm(target-eye)},h,&st,false);
 if(h.id<0){
  qCritical()<<"Ray missed the diagnostic right-corner bay";return 1;
 }
 for(const auto& mat:r.renderScene.mat){
  if(mat.normalMip.empty()||mat.colorMip.empty()||mat.roughnessMip.empty()){
   qCritical()<<"PBR technical material missing texture chains";return 1;
  }
 }
 qInfo()<<"Photo2Scene WARP proof loaded through SAME V16 renderer:"
        <<r.objects.size()<<"diagnostic elements,"
        <<r.cameras.size()<<"image-calibrated cameras,"
        <<r.renderScene.obj.size()<<"triangles, BVH hit"<<h.id;
 return 0;
}
