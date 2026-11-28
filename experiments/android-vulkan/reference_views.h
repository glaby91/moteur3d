#pragma once
#include "m3d_engine.h"
#include "src/m3d_v16_bridge.h"
#include "../../v17/anomaly_catalog.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace cse::androidqa {
using namespace m3d;
inline void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
inline QJsonArray vec(V p){return {double(p.x),double(p.y),double(p.z)};}
inline QVector<v16::CameraPreset> views(const v16::Runtime& rt){
    check(!rt.cameras.isEmpty(),"missing authored camera");
    QVector<v16::CameraPreset> out{rt.cameras.first()};
    for(const char* id:{"ANO-011","ANO-005","ANO-007","ANO-013"}){
        bool found=false;
        for(const auto& r:cse::anomalies::records)if(r.cameraKnown&&std::strcmp(r.id,id)==0){
            const float yaw=r.yaw*PI/180.f,pitch=r.pitch*PI/180.f;
            V eye{r.x,r.y,r.z},dir{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
            out.push_back({QString::fromLatin1(r.id),QString::fromLatin1(r.title),eye,eye+dir,r.fov});
            found=true;break;
        }
        check(found,"missing measured anomaly view");
    }
    return out;
}
inline void renderViews(const v16::Runtime& rt,const ShadowMap& sm,const QString& output){
    QDir dir(output);check(dir.mkpath("."),"create reference output");
    QJsonArray records;
    auto cameras=views(rt);
    for(int i=0;i<cameras.size();++i){
        auto preset=cameras[i];auto camera=v16::makeCamera(preset,640,360);
        std::vector<uint32_t> pixels;Stats st{};std::wstring reason;
        if(!renderVulkanPreview(rt.renderScene,sm,camera,640,360,pixels,st,reason))
            throw std::runtime_error(QString::fromStdWString(reason).toStdString());
        check(st.gpu&&st.gpuDrawCalls>0&&pixels.size()==640u*360u,"invalid reference frame");
        check(std::any_of(pixels.begin(),pixels.end(),[&](uint32_t p){return p!=pixels.front();}),"flat reference frame");
        QImage image(640,360,QImage::Format_ARGB32);
        std::memcpy(image.bits(),pixels.data(),pixels.size()*4);
        const QString name=QString("view-%1.png").arg(i);
        check(image.save(dir.filePath(name)),"reference PNG export");
        records.append(QJsonObject{{"file",name},{"id",preset.id},{"name",preset.name},
            {"eye",vec(preset.eye)},{"target",vec(preset.target)},{"fov",double(preset.fov)}});
        qInfo().noquote()<<"CSE_ANDROID PASS VIEW"<<i<<preset.id<<"ms"<<st.ms;
    }
    QFile manifest(dir.filePath("views.json"));
    check(manifest.open(QIODevice::WriteOnly),"write reference metadata");
    const auto bytes=QJsonDocument(records).toJson();
    check(manifest.write(bytes)==bytes.size(),"save reference metadata");
}
}
