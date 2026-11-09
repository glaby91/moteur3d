#include "m3d_scene_v16.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <functional>
#include <limits>

namespace m3d::v16 {
namespace {
bool vec3(const QJsonValue& value, V& v) {
    const auto a=value.toArray();
    if(a.size()!=3) return false;
    for(const auto& q:a) if(!q.isDouble() || !std::isfinite(q.toDouble())) return false;
    v={float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};
    return true;
}
bool finite01(const QJsonValue& val, float& result, float fallback) {
    if(val.isUndefined()) {result=fallback;return true;}
    if(!val.isDouble() || !std::isfinite(val.toDouble()) || val.toDouble()<0 || val.toDouble()>1) return false;
    result=float(val.toDouble());return true;
}
uint32_t rgb(V c) {
    auto u=[](float x)->uint32_t {return uint32_t(std::lround(clampf(x,0.f,1.f)*255.f));};
    return (u(c.x)<<16)|(u(c.y)<<8)|u(c.z);
}
std::vector<ColorLevel> constantColorMips(V c) {
    ColorLevel m;m.size=1;m.p={rgb(c)};
    return {std::move(m)};
}
std::vector<Level> constantGrayMips(float value) {
    const auto b=uint16_t(std::lround(clampf(value,0.f,1.f)*31.f));
    Level m;m.size=1;m.p={uint16_t((b<<11)|(uint16_t(std::lround(value*63.f))<<5)|b)};
    return {std::move(m)};
}
void addTriangle(Scene& scene, V a,V b,V c,uint8_t mat) {
    Obj o;o.kind=Kind::Triangle;o.mat=mat;o.a=a;o.b=b;o.c=c;
    o.mn={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    o.mx={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    const V n=norm(cross(b-a,c-a));o.n0=o.n1=o.n2=n;
    // World-distance UV scaling is a stable first-pass mapping.
    o.uv0={0,0,0};o.uv1={length(b-a)*.25f,0,0};o.uv2={0,length(c-a)*.25f,0};
    o.center=b-a;o.core=c-a;
    o.uv0.z=dot(o.center,o.center);o.uv1.z=dot(o.center,o.core);
    o.uv2.z=dot(o.core,o.core);
    const float det=o.uv0.z*o.uv2.z-o.uv1.z*o.uv1.z;
    o.len2=std::abs(det)>1e-20f?1.f/det:0.f;
    scene.obj.push_back(o);
}
void addQuad(Scene& scene,V a,V b,V c,V d,uint8_t mat) {
    addTriangle(scene,a,b,c,mat);
    addTriangle(scene,a,c,d,mat);
}
void addBox(Scene& scene,V center,V size,uint8_t mat) {
    const V a=center-size*.5f,b=center+size*.5f;
    const V p000{a.x,a.y,a.z},p001{a.x,a.y,b.z},p010{a.x,b.y,a.z},p011{a.x,b.y,b.z};
    const V p100{b.x,a.y,a.z},p101{b.x,a.y,b.z},p110{b.x,b.y,a.z},p111{b.x,b.y,b.z};
    addQuad(scene,p001,p101,p111,p011,mat);
    addQuad(scene,p100,p000,p010,p110,mat);
    addQuad(scene,p000,p001,p011,p010,mat);
    addQuad(scene,p101,p100,p110,p111,mat);
    addQuad(scene,p010,p011,p111,p110,mat);
    addQuad(scene,p000,p100,p101,p001,mat);
}
V centroid(const Obj& o){return(o.mn+o.mx)*.5f;}
void buildBvh(Scene& s) {
    s.order.resize(s.obj.size());
    std::iota(s.order.begin(),s.order.end(),uint32_t(0));
    std::function<int(int,int)> recurse=[&](int start,int end)->int {
        const int index=int(s.node.size());s.node.push_back(Node{});
        V mn{INF,INF,INF},mx{-INF,-INF,-INF};
        for(int j=start;j<end;++j){
            const Obj& o=s.obj[s.order[j]];
            mn={std::min(mn.x,o.mn.x),std::min(mn.y,o.mn.y),std::min(mn.z,o.mn.z)};
            mx={std::max(mx.x,o.mx.x),std::max(mx.y,o.mx.y),std::max(mx.z,o.mx.z)};
        }
        s.node[index].mn=mn;s.node[index].mx=mx;
        if(end-start<=8){s.node[index].start=start;s.node[index].count=end-start;return index;}
        const V d=mx-mn;const int axis=(d.x>=d.y&&d.x>=d.z)?0:(d.y>=d.z?1:2);
        const int mid=(start+end)/2;
        std::nth_element(s.order.begin()+start,s.order.begin()+mid,s.order.begin()+end,
            [&](uint32_t a,uint32_t b){
                V x=centroid(s.obj[a]),y=centroid(s.obj[b]);
                return axis==0?x.x<y.x:axis==1?x.y<y.y:x.z<y.z;
            });
        const int left=recurse(start,mid),right=recurse(mid,end);
        s.node[index].left=left;s.node[index].right=right;
        return index;
    };
    s.root=s.obj.empty()?-1:recurse(0,int(s.obj.size()));
}
}
bool compileRenderScene(const SceneDatabase& database, Scene& out,QString& error) {
    Scene result;
    QHash<QString,uint8_t> materials;
    QHash<QString,V> primitiveSizes;
    QHash<QString,uint8_t> primitiveMaterials;
    if(database.sectors.isEmpty()){error=QStringLiteral("No sectors");return false;}
    // Resolve materials independently from scene objects.
    for(const auto& sector:database.sectors) {
        for(const auto& resource:sector.resources.value(QStringLiteral("materials"))) {
            const auto list=resource.data.value(QStringLiteral("materials")).toArray();
            for(const auto& value:list) {
                if(!value.isObject()){error=QStringLiteral("Invalid material entry");return false;}
                const auto obj=value.toObject();const QString id=obj.value(QStringLiteral("id")).toString();
                V color;float roughness=0,metallic=0;
                if(id.isEmpty()||materials.contains(id)||!vec3(obj.value(QStringLiteral("baseColor")),color)||
                   !finite01(obj.value(QStringLiteral("roughness")),roughness,.7f)||
                   !finite01(obj.value(QStringLiteral("metallic")),metallic,0.f)){
                    error=QStringLiteral("Invalid/duplicate PBR material: %1").arg(id);return false;
                }
                if(result.mat.size()>=256){error=QStringLiteral("Maximum 256 materials");return false;}
                Material m;m.name=id.toStdString();m.metallic=metallic;m.roughnessScale=1.f;
                m.colorMip=constantColorMips(color);m.roughnessMip=constantGrayMips(roughness);
                // Later: texture image sets and tangent-space normal mipmaps.
                float transmission=0;
                if(!finite01(obj.value(QStringLiteral("transmission")),transmission,0.f)){
                    error=QStringLiteral("Invalid transmission for %1").arg(id);return false;
                }
                m.transmissionPercent=uint8_t(std::lround(transmission*100.f));
                const double ior=obj.value(QStringLiteral("ior")).toDouble(1.5);
                if(!std::isfinite(ior)||ior<1||ior>3){error=QStringLiteral("Invalid IOR for %1").arg(id);return false;}
                m.refractiveIndex=float(ior);
                m.reflectivityPercent=uint8_t(std::lround((1-roughness)*35.f));
                materials.insert(id,uint8_t(result.mat.size()));result.mat.push_back(std::move(m));
            }
        }
    }
    if(result.mat.empty()){error=QStringLiteral("No materials");return false;}
    for(const auto& sector:database.sectors) {
        primitiveSizes.clear();primitiveMaterials.clear();
        for(const auto& resource:sector.resources.value(QStringLiteral("geometry"))) {
            const auto list=resource.data.value(QStringLiteral("primitives")).toArray();
            for(const auto& value:list){
                const auto obj=value.toObject();const auto id=obj.value(QStringLiteral("id")).toString();
                const auto matId=obj.value(QStringLiteral("material")).toString();
                V size;
                if(id.isEmpty()||primitiveSizes.contains(id)||obj.value(QStringLiteral("type")).toString()!="box"||
                   !vec3(obj.value(QStringLiteral("size")),size)||size.x<=0||size.y<=0||size.z<=0||
                   !materials.contains(matId)){
                    error=QStringLiteral("Invalid box primitive/material: %1").arg(id);return false;
                }
                primitiveSizes.insert(id,size);primitiveMaterials.insert(id,materials[matId]);
            }
        }
        for(const auto& resource:sector.resources.value(QStringLiteral("instances"))) {
            for(const auto& value:resource.data.value(QStringLiteral("instances")).toArray()){
                const auto obj=value.toObject();
                const QString geom=obj.value(QStringLiteral("geometry")).toString();
                V position; 
                if(!primitiveSizes.contains(geom)||!vec3(obj.value(QStringLiteral("position")),position)){
                    error=QStringLiteral("Unknown primitive or invalid position: %1").arg(geom);return false;
                }
                if(result.obj.size()>200000){error=QStringLiteral("Scene exceeds triangle limit");return false;}
                addBox(result,position,primitiveSizes[geom],primitiveMaterials[geom]);
            }
        }
    }
    // Material ID -> independent PBR recipe, with mip pyramid generated once per resource.
    QSet<QString> appliedPbr;
    for(const auto& sector:database.sectors) {
        for(const auto& resource:sector.resources.value(QStringLiteral("pbr"))) {
            for(const auto& value:resource.data.value(QStringLiteral("textures")).toArray()) {
                const auto entry=value.toObject();
                const QString materialId=entry.value(QStringLiteral("id")).toString();
                const QString recipe=entry.value(QStringLiteral("recipe")).toString();
                const int resolution=entry.value(QStringLiteral("resolution")).toInt(2048);
                if(!materials.contains(materialId) || recipe.isEmpty() || appliedPbr.contains(materialId)) {
                    error=QStringLiteral("Invalid or duplicate PBR texture set: %1").arg(materialId);
                    return false;
                }
                if(!generateProceduralPbr(result.mat[materials[materialId]],recipe,resolution,error))
                    return false;
                appliedPbr.insert(materialId);
            }
        }
    }
    if(result.obj.empty()){error=QStringLiteral("No render geometry");return false;}
    buildBvh(result);
    error.clear();out=std::move(result);return true;
}
}
