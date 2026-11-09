#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdio>
#include <QFileInfo>
static void ciLog(QtMsgType, const QMessageLogContext&, const QString& message) {
    const QByteArray utf8 = message.toUtf8();
    std::fprintf(stderr, "V16 TEST: %s\n", utf8.constData());
    std::fflush(stderr);
}
int main(int argc,char** argv){
    qInstallMessageHandler(ciLog);
    QCoreApplication app(argc,argv);
    if(argc!=2){qCritical()<<"Usage: v16_scene_check scene.json";return 2;}
    const QString path=QString::fromLocal8Bit(argv[1]);
    if(!QFileInfo::exists(path)){qCritical()<<"Missing scene:"<<path;return 2;}
    auto scene=m3d::v16::loadScene(path);
    if(!scene){qCritical().noquote()<<scene.error;return 1;}
    auto built=m3d::v16::buildRuntime(scene.scene,64);
    if(!built){qCritical().noquote()<<built.error;return 1;}
    const auto& r=built.runtime;
    const auto& s=r.renderScene;
    if(r.objects.size()<150 || s.mat.size()<8 || s.root<0 || s.node.empty() || s.order.size()!=s.obj.size()){
        qCritical()<<"Scene has incomplete resources or BVH";return 1;
    }
    for(const auto& m:s.mat){
        if(m.colorMip.empty()||m.normalMip.empty()||m.roughnessMip.empty()
          ||m.colorMip.front().size!=64||m.colorMip.back().size!=1
          ||m.normalMip.back().size!=1||m.roughnessMip.back().size!=1){
            qCritical()<<"Broken mip chain:"<<m.name.c_str();return 1;
        }
    }
    for(const auto& object:r.objects)if(object.triangleCount<=0){
        qCritical()<<"Empty object:"<<object.id;return 1;
    }
    m3d::Stats stats;
    m3d::Ray ray{{0.f,11.f,22.f},m3d::norm({0.f,-.3f,-1.f})};
    m3d::Hit hit;
    m3d::closest(s,ray,hit,&stats,false);
    if(hit.id<0){qCritical()<<"BVH raycast failed";return 1;}
    qInfo()<<"V16 scene OK — objects:"<<r.objects.size()
           <<"triangles:"<<s.obj.size()<<"BVH nodes:"<<s.node.size()
           <<"PBR materials:"<<s.mat.size()<<"camera presets:"<<r.cameras.size()
           <<"hit t:"<<hit.t;
    return 0;
}
