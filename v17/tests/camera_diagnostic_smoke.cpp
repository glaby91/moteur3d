#include "../camera_diagnostic.h"
#include <QJsonDocument>
#include <QCoreApplication>
#include <cmath>
#include <cstdio>

int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    using cse::camera_diagnostic::toJson;
    using cse::camera_diagnostic::fromJson;
    m3d::v16::CameraPreset original;
    original.id=QStringLiteral("v8_glazing_raccords");
    original.name=QStringLiteral("Façade, raccords des vitrages");
    original.eye={32.45f,4.70f,-18.32f};
    original.target={33.0f,4.45f,-20.0f};
    original.fov=75.f;
    const QJsonObject exported=toJson(original);
    const QByteArray bytes=QJsonDocument(exported).toJson(QJsonDocument::Compact);
    m3d::v16::CameraPreset restored;
    QString reason;
    if(!fromJson(QJsonDocument::fromJson(bytes).object(),restored,reason)){
        std::fprintf(stderr,"V17.434 CAMERA JSON ROUNDTRIP FAIL: %s\n",reason.toUtf8().constData());return 1;
    }
    const auto close=[](float a,float b){return std::abs(double(a)-double(b))<0.00001;};
    if(!close(original.eye.x,restored.eye.x)||!close(original.eye.y,restored.eye.y)||
       !close(original.eye.z,restored.eye.z)||!close(original.target.x,restored.target.x)||
       !close(original.target.y,restored.target.y)||!close(original.target.z,restored.target.z)||
       !close(original.fov,restored.fov)||original.id!=restored.id){
        std::fprintf(stderr,"V17.434 CAMERA ROUNDTRIP COORDINATE MISMATCH\n");return 2;
    }
    QJsonObject bad=exported;
    bad.remove("target");
    if(fromJson(bad,restored,reason)){std::fprintf(stderr,"V17.434 accepted missing target\n");return 3;}
    bad=exported;bad["fovDeg"]=230;
    if(fromJson(bad,restored,reason)){std::fprintf(stderr,"V17.434 accepted invalid FOV\n");return 4;}
    bad=exported;bad["target"]=bad.value("eye");
    if(fromJson(bad,restored,reason)){std::fprintf(stderr,"V17.434 accepted zero length sightline\n");return 5;}
    const auto lines=cse::camera_diagnostic::hudLines(original,QStringLiteral("CAMERA"));
    if(lines.size()!=3||!lines[1].contains(QStringLiteral("32.450"))||
       !lines[2].contains(QStringLiteral("FOV 75.0"))){
        std::fprintf(stderr,"V17.434 camera HUD values missing\n");return 6;
    }
    std::printf("PASS V17.434 camera HUD and exact JSON pose: 4 validations\n");
    return 0;
}
