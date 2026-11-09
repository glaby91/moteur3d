#include "src/m3d_scene_v16.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

int main(int argc,char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { qCritical() << "Expected path to Pavillon01 scene.json"; return 2; }
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if (!loaded) { qCritical() << loaded.error; return 3; }
    m3d::Scene scene;
    QString error;
    if (!m3d::v16::compileRenderScene(loaded.scene,scene,error)) {
        qCritical() << error; return 4;
    }
    if (scene.obj.size()!=84 || scene.root!=0 || scene.mat.size()!=5 ||
        scene.order.size()!=scene.obj.size() || scene.node.empty()) {
        qCritical() << "Pavillon01 scene geometry/BVH invariants failed";
        return 5;
    }
    for (const auto& material : scene.mat) {
        if (material.colorMip.size()!=12 || material.normalMip.size()!=12 ||
            material.roughnessMip.size()!=12 || material.colorMip.front().size!=2048 ||
            material.colorMip.back().size!=1) {
            qCritical() << \"PBR mip chain invalid for\" << material.name.c_str();
            return 10;
        }
    }
    for (const auto& object:scene.obj)
        if (object.kind!=m3d::Kind::Triangle || object.mat>=scene.mat.size())
            return 6;
    QTemporaryDir dir;
    if (!dir.isValid()) return 7;
    QJsonObject bad{{"format","m3d-scene-v16"}, {"sectors", QJsonArray{QJsonObject{
        {"id","invalid"},{"geometry",QJsonArray{"../outside.json"}}
    }}}};
    QFile file(dir.filePath("scene.json"));
    if (!file.open(QIODevice::WriteOnly)) return 8;
    file.write(QJsonDocument(bad).toJson());file.close();
    if (m3d::v16::loadScene(file.fileName())) {
        qCritical() << "Resource escape was not rejected"; return 9;
    }
    qInfo() << "PASS: Pavillon01 modular scene, 84 triangles, BVH, materials and path validation";
    return 0;
}
