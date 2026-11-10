// Validate an arbitrary V17-compatible benchmark scene without Pavillon-01
// assumptions (the V16 scene_check fixture checks exactly 15 canopies).
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
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
       built.runtime.renderScene.mat.size()!=22 || built.runtime.objects.size()<1000){
        qCritical()<<"Invalid benchmark content: triangles="<<qulonglong(triangles)
                   <<"min="<<qulonglong(requested)
                   <<"cameras="<<built.runtime.cameras.size()
                   <<"materials="<<built.runtime.renderScene.mat.size()
                   <<"instances="<<built.runtime.objects.size();
        return 5;
    }
    qInfo()<<"CSE WARP V17 benchmark scene validated:"
           <<qulonglong(triangles)<<"triangles,"
           <<built.runtime.objects.size()<<"instances,"
           <<built.runtime.cameras.size()<<"cameras,"
           <<built.runtime.renderScene.mat.size()<<"PBR materials";
    return 0;
}
