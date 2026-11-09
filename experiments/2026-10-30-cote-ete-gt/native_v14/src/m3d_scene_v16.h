#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QHash>
#include "m3d_engine.h"

namespace m3d::v16 {
struct Resource {
    QString path;
    QJsonObject data;
};
struct Sector {
    QString id;
    QHash<QString, QVector<Resource>> resources;
};
struct SceneDatabase {
    QString name;
    QVector<Sector> sectors;
};
struct LoadResult {
    SceneDatabase scene;
    QString error;
    explicit operator bool() const { return error.isEmpty(); }
};
// Transactional: the caller replaces its active scene only after a successful load.
LoadResult loadScene(const QString& manifestPath);
// Build a triangle-only render scene from the validated modular resource database.
bool generateProceduralPbr(m3d::Material& material,const QString& recipe,int resolution,QString& error);
bool compileRenderScene(const SceneDatabase& database, m3d::Scene& out, QString& error);
}
