#pragma once
#include "m3d_engine.h"
#include "m3d_scene_v16.h"
#include <QHash>
#include <QString>
#include <vector>
#include <functional>

namespace m3d::v16 {
struct CameraPreset {
    QString id, name;
    V eye{}, target{};
    float fov=55.f;
};
struct ObjectInfo {
    QString id, parent, material;
    int firstTriangle=0, triangleCount=0;
};
struct Runtime {
    Scene renderScene;
    QVector<CameraPreset> cameras;
    QVector<ObjectInfo> objects;
    QHash<QString,int> materialIndex;
    QString title;
    int textureResolution=0;
    float daylightHour=15.f;
};
struct BuildResult {
    Runtime runtime;
    QString error;
    explicit operator bool() const { return error.isEmpty(); }
};
// Optional, thread-safe consumer callback. Reports completed real stages/items
// in percent (not elapsed-time predictions) and their current description.
BuildResult buildRuntime(const SceneDatabase& source, int maxTextureResolution=2048,
    const std::function<void(int,const QString&)>& onProgress={});
Camera makeCamera(const CameraPreset& preset, int width, int height);
}
