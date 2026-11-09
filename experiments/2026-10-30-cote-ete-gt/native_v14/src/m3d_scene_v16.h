#pragma once
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QHash>

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
}
