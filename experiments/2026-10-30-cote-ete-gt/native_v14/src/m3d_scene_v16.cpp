#include "m3d_scene_v16.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace m3d::v16 {
namespace {
bool readObject(const QString& path, QJsonObject& object, QString& error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot open %1: %2").arg(path, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QStringLiteral("Invalid JSON object %1: %2").arg(path, parseError.errorString());
        return false;
    }
    object = doc.object();
    return true;
}
}
LoadResult loadScene(const QString& manifestPath) {
    LoadResult result;
    const QFileInfo manifestInfo(manifestPath);
    const QString root = QDir::cleanPath(manifestInfo.absolutePath());
    result.scene.rootPath = root;
    QJsonObject manifest;
    if (!readObject(manifestInfo.absoluteFilePath(), manifest, result.error)) return result;
    if (manifest.value("format").toString() != QStringLiteral("m3d-scene-v16")) {
        result.error = QStringLiteral("Expected m3d-scene-v16 manifest");
        return result;
    }
    if (!manifest.value("sectors").isArray()) {
        result.error = QStringLiteral("Missing sectors array");
        return result;
    }
    result.scene.name = manifest.value("name").toString();
    static const QStringList kinds = {"geometry", "instances", "materials", "pbr",
                                      "lighting", "cameras", "environment"};
    QSet<QString> sectorIds;
    const QDir rootDir(root);
    for (const auto& sectorValue : manifest.value("sectors").toArray()) {
        if (!sectorValue.isObject()) {
            result.error = QStringLiteral("Sector must be an object"); return result;
        }
        const auto sectorObject = sectorValue.toObject();
        Sector sector;
        sector.id = sectorObject.value("id").toString();
        if (sector.id.isEmpty() || sectorIds.contains(sector.id)) {
            result.error = QStringLiteral("Missing or duplicate sector id: %1").arg(sector.id);
            return result;
        }
        sectorIds.insert(sector.id);
        for (const auto& kind : kinds) {
            const auto value = sectorObject.value(kind);
            if (!value.isUndefined() && !value.isArray()) {
                result.error = QStringLiteral("%1.%2 must be an array").arg(sector.id, kind);
                return result;
            }
            for (const auto& reference : value.toArray()) {
                if (!reference.isString() || reference.toString().isEmpty()) {
                    result.error = QStringLiteral("Invalid reference in %1.%2").arg(sector.id, kind);
                    return result;
                }
                const QString ref = reference.toString();
                const QString absolute = QDir::cleanPath(rootDir.absoluteFilePath(ref));
                const QString relative = rootDir.relativeFilePath(absolute);
                if (QDir::isAbsolutePath(ref) || relative == ".." || relative.startsWith("../")) {
                    result.error = QStringLiteral("Resource escapes scene root: %1").arg(ref);
                    return result;
                }
                // Canonical paths also prevent escaping through symlinks.
                const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
                const QString canonicalFile = QFileInfo(absolute).canonicalFilePath();
                if (canonicalFile.isEmpty() || canonicalRoot.isEmpty() ||
                    !(canonicalFile.startsWith(canonicalRoot + "/") ||
                      canonicalFile.startsWith(canonicalRoot + "\\"))) {
                    result.error = QStringLiteral("Missing or external resource: %1").arg(ref);
                    return result;
                }
                Resource resource;
                resource.path = ref;
                if (!readObject(absolute, resource.data, result.error)) return result;
                sector.resources[kind].push_back(std::move(resource));
            }
        }
        result.scene.sectors.push_back(std::move(sector));
    }
    return result;
}
}
