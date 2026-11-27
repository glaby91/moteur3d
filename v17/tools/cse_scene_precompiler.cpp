#include "src/m3d_v16_bridge.h"
#include "src/m3d_csp001_bvh.h"
#include "src/m3d_zreflect_catalog.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
namespace {
bool writeInventory(const QString& root,const m3d::v16::Runtime& rt,const m3d::v16::SceneDatabase& db,const QJsonObject& manifest){
    QSaveFile out(QDir(root).filePath("precomputed/zreflect-candidates.json"));
    const auto bytes=QJsonDocument(m3d::zreflect::catalogue(rt,db,manifest)).toJson(QJsonDocument::Compact);
    return out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size()&&out.commit();
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTextStream log(stderr);
    const auto args=app.arguments();
    if(args.size()!=2){log<<"Usage: cse_scene_precompiler <scene.json>\n";return 2;}
    QFile manifestFile(args[1]);
    if(!manifestFile.open(QIODevice::ReadOnly))return 3;
    const auto manifest=QJsonDocument::fromJson(manifestFile.readAll()).object();
    auto loaded=m3d::v16::loadScene(args[1]);
    if(!loaded){log<<loaded.error<<'\n';return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,2048);
    if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){
        log<<built.error<<' '<<built.runtime.lumaTraceBvhError<<'\n';return 4;
    }
    const auto root=loaded.scene.rootPath;
    if(!QDir(root).mkpath("precomputed")||
       !m3d::csp001::save(m3d::csp001::path(root),built.runtime.renderScene,built.runtime.lumaTraceBvh4)||
       !writeInventory(root,built.runtime,loaded.scene,manifest)){
        log<<"CSP001 write failed\n";return 5;
    }
    log<<"CSP001 sidecars written\n";return 0;
}
