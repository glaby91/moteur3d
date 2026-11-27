#pragma once
// Versioned, bounded, transactional cache for authorized optical patches and their BVH.
#include "m3d_zreflect_catalog.h"
#include "m3d_csp001_bvh.h"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDebug>
namespace m3d::zreflect {
inline QString cachePath(const QString& root){return QDir(root).filePath("precomputed/zreflect-index.json");}
inline QByteArray hashBytes(const QByteArray& b){return QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex();}
inline QByteArray cacheKey(const Scene& scene,const v16::SceneDatabase& db,const QJsonObject& manifest){
    const auto geometry=csp001::geometryKey(scene);if(geometry.isEmpty())return {};
    QCryptographicHash h(QCryptographicHash::Sha256);h.addData(geometry);
    auto add=[&](const QJsonObject& o){h.addData(hashBytes(QJsonDocument(o).toJson(QJsonDocument::Compact)));};
    add(manifest);
    for(const auto& s:db.sectors)for(const QString& kind:{QStringLiteral("instances"),QStringLiteral("materials"),QStringLiteral("pbr")})
        for(const auto& file:s.resources.value(kind))add(file.data);
    return h.result().toHex();
}
inline bool parseVec(const QJsonValue& j,V& v){
    const auto a=j.toArray();if(a.size()!=3)return false;
    for(const auto& x:a)if(!x.isDouble()||!std::isfinite(x.toDouble())||std::abs(x.toDouble())>1.e9)return false;
    v={float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};return true;
}
inline bool parseU(const QJsonValue& v,uint32_t& n){
    const double d=v.toDouble(-1);if(d<0||d>4294967295.||std::floor(d)!=d)return false;n=uint32_t(d);return true;
}
inline std::vector<IndexedPlane> descriptorsFor(const std::vector<OpticalPlane>& planes){
    std::vector<IndexedPlane> result;for(const auto& p:planes){Bounds b;b.add(p.lo);b.add(p.hi);result.push_back({uint32_t(result.size()),b});}return result;
}
inline bool saveCache(const QString& path,const Scene& scene,const v16::SceneDatabase& db,const QJsonObject& manifest){
    const auto key=cacheKey(scene,db,manifest);if(key.isEmpty())return false;
    QJsonArray planes,nodes,order;
    for(const auto& p:scene.opticalPlanes){
        QJsonArray ids;for(auto id:p.triangles)ids.append(double(id));
        planes.append(QJsonObject{{"normal",vec(p.normal)},{"lo",vec(p.lo)},{"hi",vec(p.hi)},
            {"offset",p.offset},{"area",p.area},{"side",p.side},{"forced",p.forced},{"triangles",ids}});
    }
    SpatialIndex index;index.build(descriptorsFor(scene.opticalPlanes));
    std::vector<OpticalIndexNode> nn;std::vector<uint32_t> oo;if(!index.snapshot(nn,oo))return false;
    for(auto i:oo)order.append(double(i));
    for(const auto& n:nn)nodes.append(QJsonObject{{"lo",vec(n.lo)},{"hi",vec(n.hi)},{"begin",double(n.begin)},
        {"end",double(n.end)},{"left",double(n.left)},{"right",double(n.right)},{"leaf",n.leaf}});
    QJsonObject payload{{"schema",2},{"key",QString::fromLatin1(key)},{"planes",planes},{"nodes",nodes},{"order",order}};
    const auto encoded=QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const auto bytes=QJsonDocument(QJsonObject{{"payload",payload},{"sha256",QString::fromLatin1(hashBytes(encoded))}}).toJson(QJsonDocument::Compact);
    QSaveFile out(path);return out.open(QIODevice::WriteOnly)&&out.write(bytes)==bytes.size()&&out.commit();
}
inline bool loadCache(const QString& path,Scene& scene,const v16::SceneDatabase& db,const QJsonObject& manifest){
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>32ll*1024*1024||f.size()<=0)return false;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(f.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    const auto root=doc.object(),p=root.value("payload").toObject();
    if(p.value("schema").toInt()!=2||root.value("sha256").toString().toLatin1()!=hashBytes(QJsonDocument(p).toJson(QJsonDocument::Compact)))return false;
    const auto key=cacheKey(scene,db,manifest);
    if(key.isEmpty()||p.value("key").toString().toLatin1()!=key)return false;
    if(!p.value("planes").isArray()||!p.value("nodes").isArray()||!p.value("order").isArray())return false;
    const auto pp=p.value("planes").toArray(),nn=p.value("nodes").toArray(),oo=p.value("order").toArray();
    if(pp.size()>100000||nn.size()>2*pp.size()||oo.size()!=pp.size())return false;
    std::vector<OpticalPlane> planes;std::vector<OpticalIndexNode> nodes;std::vector<uint32_t> order;
    std::set<uint32_t> used;
    for(const auto& e:pp){
        const auto q=e.toObject();OpticalPlane a;
        if(!parseVec(q.value("normal"),a.normal)||!parseVec(q.value("lo"),a.lo)||!parseVec(q.value("hi"),a.hi)||
           !csp001::validBox(a.lo,a.hi)||std::abs(length(a.normal)-1.f)>1.e-4f||
           !q.value("offset").isDouble()||!q.value("area").isDouble()||!q.value("forced").isBool()||!q.value("triangles").isArray())return false;
        a.offset=q.value("offset").toDouble();a.area=q.value("area").toDouble();a.side=q.value("side").toInt(99);a.forced=q.value("forced").toBool();
        if(!std::isfinite(a.offset)||!std::isfinite(a.area)||a.area<=0||a.side < -1||a.side>1)return false;
        Patch patch;patch.normal=a.normal;patch.offset=a.offset;
        Bounds bounds;
        for(const auto& value:q.value("triangles").toArray()){
            uint32_t id;if(!parseU(value,id)||id>=scene.obj.size()||!used.insert(id).second)return false;
            const auto& t=scene.obj[id];if(t.kind!=Kind::Triangle||t.mat>=scene.mat.size()||!samePlane(patch,t,a.normal))return false;
            const auto& m=scene.mat[t.mat];
            const bool water=m.waterSurface||(!m.surfaceAuthored&&m.name=="cse_pool_water");
            if(!water&&!m.transmissionPercent)return false;
            for(V v:{t.a,t.b,t.c})bounds.add(v);a.triangles.push_back(id);
        }
        if(a.triangles.empty()||!bounds.valid||!csp001::contains(a.lo,a.hi,bounds.lo,bounds.hi))return false;
        planes.push_back(std::move(a));
    }
    for(const auto& e:nn){const auto q=e.toObject();OpticalIndexNode n;
        if(!parseVec(q.value("lo"),n.lo)||!parseVec(q.value("hi"),n.hi)||!parseU(q.value("begin"),n.begin)||
           !parseU(q.value("end"),n.end)||!parseU(q.value("left"),n.left)||!parseU(q.value("right"),n.right)||!q.value("leaf").isBool())return false;
        n.leaf=q.value("leaf").toBool();nodes.push_back(n);}
    for(const auto& e:oo){uint32_t id;if(!parseU(e,id))return false;order.push_back(id);}
    SpatialIndex index;if(!index.restore(descriptorsFor(planes),nodes,order))return false;
    scene.opticalPlanes=std::move(planes);scene.opticalIndexNodes=std::move(nodes);scene.opticalIndexOrder=std::move(order);
    return true;
}
}
