#include "src/m3d_v16_bridge.h"
#include "src/m3d_csp001_bvh.h"
#include "src/m3d_zreflect_visibility.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
namespace {
using namespace m3d;
QJsonArray vec(V v){return {v.x,v.y,v.z};}
// Inventory only: candidates are not assumed planar and never activate a camera.
// Record object bounds and real material fields; roughnessScale is NOT roughness.
QJsonArray inventory(const v16::Runtime& rt){
    QJsonArray out;
    for(const auto& object:rt.objects){
        const auto first=object.firstTriangle,count=object.triangleCount;
        if(first<0||count<=0||size_t(first)>rt.renderScene.obj.size()||
           size_t(count)>rt.renderScene.obj.size()-size_t(first))continue;
        const auto id=rt.renderScene.obj[size_t(first)].mat;
        if(id>=rt.renderScene.mat.size())continue;
        const auto& mat=rt.renderScene.mat[id];
        if(!mat.waterSurface&&!mat.transmissionPercent&&!mat.reflectivityPercent&&mat.metallic<=0.f)continue;
        zreflect::Bounds bounds;
        for(int k=0;k<count;++k){
            const auto& tri=rt.renderScene.obj[size_t(first+k)];
            bounds.add(tri.mn);bounds.add(tri.mx);
        }
        if(!bounds.valid||bounds.uncertain)continue;
        out.append(QJsonObject{
            {"objectId",object.id},{"parent",object.parent},{"material",object.material},
            {"firstTriangle",first},{"triangleCount",count},{"min",vec(bounds.lo)},{"max",vec(bounds.hi)},
            {"ior",mat.refractiveIndex},{"dielectricF0",zreflect::dielectricF0(mat.refractiveIndex)},
            {"reflectivityPercent",int(mat.reflectivityPercent)},{"transmissionPercent",int(mat.transmissionPercent)},
            {"metallic",mat.metallic},{"roughnessScale",mat.roughnessScale},
            {"waterSurface",mat.waterSurface},{"cameraId",-1},{"bufferId",-1}
        });
    }
    return out;
}
bool writeInventory(const QString& root,const v16::Runtime& rt,const QString& revision){
    QSaveFile out(QDir(root).filePath("precomputed/zreflect-candidates.json"));
    const auto bytes=QJsonDocument(QJsonObject{{"schema",1},{"revision",revision},
        {"status","inventory-only"},{"objects",inventory(rt)}}).toJson(QJsonDocument::Compact);
    return out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size()&&out.commit();
}
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);QTextStream log(stderr);
    const auto args=app.arguments();
    if(args.size()!=2){log<<"Usage: cse_scene_precompiler <scene.json>\n";return 2;}
    auto loaded=m3d::v16::loadScene(args[1]);
    if(!loaded){log<<loaded.error<<'\n';return 3;}
    auto built=m3d::v16::buildRuntime(loaded.scene,2048);
    if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){
        log<<built.error<<' '<<built.runtime.lumaTraceBvhError<<'\n';return 4;
    }
    const auto root=loaded.scene.rootPath;
    if(!QDir(root).mkpath("precomputed")||
       !m3d::csp001::save(m3d::csp001::path(root),built.runtime.renderScene,built.runtime.lumaTraceBvh4)||
       !writeInventory(root,built.runtime,loaded.scene.revision)){
        log<<"CSP001 write failed\n";return 5;
    }
    log<<"CSP001 sidecars written\n";return 0;
}
