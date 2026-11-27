#pragma once
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>
#include <QSet>
#include <QVector>
#include <utility>
#include <cmath>
namespace m3d::zmaterial {
// Transactional selection: keep declaration order, never generate unused textures.
inline bool selectActive(QVector<QJsonObject>& declared,const QVector<QJsonObject>& instances,QString& error){
    QSet<QString> used,seen;
    for(const auto& instance:instances)used.insert(instance.value("material").toString());
    QVector<QJsonObject> active;
    for(const auto& material:declared){
        const QString id=material.value("id").toString();
        if(id.isEmpty()||seen.contains(id)){error="Duplicate or missing material: "+id;return false;}
        seen.insert(id);
        if(used.contains(id))active.append(material);
    }
    for(const auto& id:used)if(!seen.contains(id)){error="Unknown object material: "+id;return false;}
    if(active.isEmpty()||active.size()>256){error="Active material count outside 1..256";return false;}
    declared=std::move(active);
    return true;
}
// One compact external catalogue. Only explicitly referenced presets are expanded.
class Catalog {
    QJsonObject families_;
    bool loaded_=false;
public:
    bool resolve(QJsonObject& material,QString& error) {
        if(!material.contains("libraryMaterial")) return true;
        const QString id=material.value("libraryMaterial").toString();
        static const QRegularExpression rx(QStringLiteral("^([a-z_]+)_([0-9]{2})_([0-9]{2})$"));
        const auto match=rx.match(id);
        if(!match.hasMatch()){error="Invalid library material: "+id;return false;}
        if(!loaded_){
            QFile file(QDir(QCoreApplication::applicationDirPath()).filePath("materials/zmaterial8192.json"));
            if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024){error="Cannot open Z Material catalogue";return false;}
            QJsonParseError parse;
            const auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
            const auto obj=doc.object();
            families_=obj.value("families").toObject();
            if(parse.error!=QJsonParseError::NoError||obj.value("schema").toInt()!=1||families_.size()!=64){error="Invalid Z Material catalogue";return false;}
            loaded_=true;
        }
        const auto family=families_.value(match.captured(1)).toObject();
        const int c=match.captured(2).toInt()-1,r=match.captured(3).toInt()-1;
        const auto colors=family.value("colors").toArray(), rough=family.value("roughness").toArray();
        if(c<0||c>=8||r<0||r>=16||colors.size()!=8||rough.size()!=16){error="Unknown library material: "+id;return false;}
        const auto linear=colors.at(c).toArray();
        if(linear.size()!=3){error="Invalid catalogue color: "+id;return false;}
        QJsonArray srgb;
        for(const auto& x:linear){
            const double v=x.toDouble(-1);
            if(!x.isDouble()||v<0||v>1||!std::isfinite(v)){error="Invalid catalogue color: "+id;return false;}
            srgb.append(v<=.0031308?v*12.92:1.055*std::pow(v,1.0/2.4)-.055);
        }
        auto put=[&](const QString& key,const QJsonValue& value){if(!material.contains(key))material.insert(key,value);};
        put("baseColor",srgb);put("roughness",rough.at(r));put("metallic",family.value("metallic"));
        put("ior",family.value("ior"));put("transmission",family.value("transmission"));
        if(family.value("water").toBool())put("surfaceModel",QStringLiteral("water"));
        const double e=family.value("emission").toDouble();
        if(e>0){QJsonObject emission;emission.insert("color",linear);emission.insert("intensity",e);put("emission",emission);}
        return true;
    }
};
}
