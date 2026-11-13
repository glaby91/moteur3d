#pragma once
// Portable, versioned camera records for architectural anomaly reproduction.
// No GPU dependency: JSON positions are expressed in scene world metres.
#include "src/m3d_v16_bridge.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <cmath>
#include <limits>

namespace cse::camera_diagnostic {
inline constexpr const char* formatId="cse-warp-camera-anomaly-v1";
inline QJsonObject vectorToJson(m3d::V v) {
    return {{"x",double(v.x)},{"y",double(v.y)},{"z",double(v.z)}};
}
inline bool vectorFromJson(const QJsonValue& json,m3d::V& out) {
    if(!json.isObject())return false;
    const QJsonObject obj=json.toObject();
    if(!obj.value("x").isDouble()||!obj.value("y").isDouble()||!obj.value("z").isDouble())return false;
    const double x=obj.value("x").toDouble();
    const double y=obj.value("y").toDouble();
    const double z=obj.value("z").toDouble();
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
       std::abs(x)>10000000||std::abs(y)>10000000||std::abs(z)>10000000)return false;
    out={float(x),float(y),float(z)};
    return true;
}
inline QJsonObject toJson(const m3d::v16::CameraPreset& camera) {
    const m3d::V v=camera.target-camera.eye;
    const float d=m3d::length(v);
    const m3d::V direction=d>0.000001f?v/d:m3d::V{0,0,-1};
    double yaw=std::atan2(double(direction.x),-double(direction.z))*180.0/m3d::PI;
    if(yaw<0)yaw+=360.0;
    const double pitch=std::asin(double(m3d::clampf(direction.y,-1.f,1.f)))*180.0/m3d::PI;
    return {{"id",camera.id},{"name",camera.name},
            {"eye",vectorToJson(camera.eye)},{"target",vectorToJson(camera.target)},
            {"fovDeg",double(camera.fov)},{"yawDeg",yaw},{"pitchDeg",pitch}};
}
inline bool fromJson(const QJsonObject& json,m3d::v16::CameraPreset& result,QString& error) {
    if(!json.value("eye").isObject()||!json.value("target").isObject()
       ||!json.value("fovDeg").isDouble()){
        error=QStringLiteral("Coordonnées ou FOV absents du rapport caméra.");return false;
    }
    m3d::V eye,target;
    if(!vectorFromJson(json.value("eye"),eye)||
       !vectorFromJson(json.value("target"),target)){
        error=QStringLiteral("Coordonnées caméra invalides.");return false;
    }
    const double fov=json.value("fovDeg").toDouble();
    if(!std::isfinite(fov)||fov<5||fov>160||m3d::length(target-eye)<0.0001f){
        error=QStringLiteral("Orientation ou FOV de caméra invalide.");return false;
    }
    result.id=json.value("id").toString();
    result.name=json.value("name").toString();
    result.eye=eye;
    result.target=target;
    result.fov=float(fov);
    error.clear();
    return true;
}
inline QStringList hudLines(const m3d::v16::CameraPreset& camera,const QString& label) {
    const auto info=toJson(camera);
    return {
       label,
       QStringLiteral("X %1   Y %2   Z %3 m")
            .arg(double(camera.eye.x),0,'f',3)
            .arg(double(camera.eye.y),0,'f',3)
            .arg(double(camera.eye.z),0,'f',3),
       QStringLiteral("Cap %1°   Incl. %2°   FOV %3°")
            .arg(info.value("yawDeg").toDouble(),0,'f',1)
            .arg(info.value("pitchDeg").toDouble(),0,'f',1)
            .arg(double(camera.fov),0,'f',1)
    };
}
} // namespace cse::camera_diagnostic
