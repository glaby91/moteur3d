#include "src/m3d_v16_bridge.h"
#include "src/m3d_csp001_bvh.h"
#include "src/m3d_zreflect_catalog.h"
#include "src/m3d_zreflect_cache.h"
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
    const bool catalogOnly=args.size()>=3&&args[2]=="--catalog-only";
    const bool withPolicy=args.size()==5&&catalogOnly&&args[3]=="--policy";
    if(args.size()!=2&&!(args.size()==3&&catalogOnly)&&!withPolicy){log<<"Usage: cse_scene_precompiler <scene.json> [--catalog-only [--policy file.json]]\n";return 2;}
    QFile manifestFile(args[1]);
    if(!manifestFile.open(QIODevice::ReadOnly))return 3;
    auto manifest=QJsonDocument::fromJson(manifestFile.readAll()).object();
    if(withPolicy){
        QFile policyFile(args[4]);
        if(!policyFile.open(QIODevice::ReadOnly)){log<<"Cannot read author policy\n";return 3;}
        QJsonParseError err;const auto doc=QJsonDocument::fromJson(policyFile.readAll(),&err);
        const auto policy=doc.object();
        if(err.error!=QJsonParseError::NoError||!doc.isObject()||policy.value("schema").toInt()!=1||
           policy.value("sceneRevision").toString()!=manifest.value("revision").toString()||
           !policy.value("materialRules").isObject()||!policy.value("sharedProbes").isArray()){
            log<<"Invalid or incompatible author policy\n";return 3;
        }
        const auto rules=policy.value("materialRules").toObject();
        for(auto it=rules.begin();it!=rules.end();++it){
            const auto p=m3d::zreflect::permission(it.value().toObject().value("planarReflection"));
            if(p==m3d::zreflect::Permission::Invalid||p==m3d::zreflect::Permission::Unspecified){
                log<<"Invalid material permission: "<<it.key()<<'\n';return 3;
            }
        }
        manifest.insert("zReflectAuthorPolicy",policy);
    }
    auto loaded=m3d::v16::loadScene(args[1]);
    if(!loaded){log<<loaded.error<<'\n';return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,2048);
    if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){
        log<<built.error<<' '<<built.runtime.lumaTraceBvhError<<'\n';return 4;
    }
    const auto root=loaded.scene.rootPath;
    if(!QDir(root).mkpath("precomputed")||
       (!catalogOnly&&!m3d::csp001::save(m3d::csp001::path(root),built.runtime.renderScene,built.runtime.lumaTraceBvh4))||
       (!catalogOnly&&!m3d::zreflect::saveCache(m3d::zreflect::cachePath(root),built.runtime.renderScene,loaded.scene,manifest))||
       !writeInventory(root,built.runtime,loaded.scene,manifest)){
        log<<"CSP001 write failed\n";return 5;
    }
    if(!catalogOnly){
        auto& s=built.runtime.renderScene;
        const auto planeCount=s.opticalPlanes.size();
        s.opticalPlanes.clear();s.opticalIndexNodes.clear();s.opticalIndexOrder.clear();
        if(!m3d::zreflect::loadCache(m3d::zreflect::cachePath(root),s,loaded.scene,manifest)||s.opticalPlanes.size()!=planeCount){
            log<<"Z Reflect sidecar roundtrip failed\n";return 6;
        }
        if(!m3d::csp001::load(m3d::csp001::path(root),s,built.runtime.lumaTraceBvh4)){
            log<<"CSP001 sidecar roundtrip failed\n";return 7;
        }
        log<<"Precomputed BVH and optical index roundtrip OK; glass planes="<<planeCount<<'\n';
    }
    log<<(catalogOnly?"Z-Reflect diagnostic catalogue written\n":"CSP001 sidecars written\n");return 0;
}
