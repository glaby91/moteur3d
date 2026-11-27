#pragma once
// Offline optical catalogue; the renderer does not consume these experimental
// planes yet. No object name dependency, except legacy water classification.
#include "m3d_v16_bridge.h"
#include "m3d_zreflect_visibility.h"
#include <QJsonArray>
#include <QJsonObject>
#include <map>
#include <array>
#include <set>
namespace m3d::zreflect {
struct Plane {
    V normal{};float offset=0;Bounds bounds;double area=0;
    std::set<int> objects;unsigned triangles=0;
};
using PlaneKey=std::array<int64_t,4>;
inline QJsonArray vec(V v){return {v.x,v.y,v.z};}
inline bool candidate(const Material& m){
    // Default dielectric reflectivity (4%) is assigned to nearly every object
    // by the loader. Do not turn all concrete triangles into mirror candidates.
    // This threshold is for diagnostic inventory only, NOT render rejection.
    return m.waterSurface||m.name=="cse_pool_water"||m.transmissionPercent>0||
        m.metallic>.05f||m.reflectivityPercent>5||
        m.roughnessScale<=.30f;
}
inline bool trianglePlane(const Obj& tri,V& n,float& d,double& area){
    if(tri.kind!=Kind::Triangle)return false;
    const V raw=cross(tri.b-tri.a,tri.c-tri.a);
    const float len=length(raw);
    if(!std::isfinite(len)||len<=1.e-7f)return false;
    n=raw/len;
    // Canonical sign: largest absolute component positive, so back/front
    // winding of the same geometrical plane can share the same camera.
    const float a[3]={std::abs(n.x),std::abs(n.y),std::abs(n.z)};
    const float k=(a[0]>=a[1]&&a[0]>=a[2])?n.x:(a[1]>=a[2]?n.y:n.z);
    if(k<0)n=n*-1.f;
    d=dot(n,tri.a);area=.5*len;
    return std::isfinite(d)&&std::abs(d)<1.e9f;
}
inline PlaneKey key(V n,float d){
    return {std::llround(n.x*10000.),std::llround(n.y*10000.),
        std::llround(n.z*10000.),std::llround(double(d)*1000.)};
}
inline bool belongs(const Plane& p,const Obj& tri,V n){
    if(dot(p.normal,n)<.999999f)return false;
    for(V v:{tri.a,tri.b,tri.c})
        if(std::abs(double(dot(p.normal,v))-p.offset)>.001)return false;
    return true;
}
inline void addTriangle(std::map<PlaneKey,std::vector<Plane>>& planes,const Obj& tri,int objectIndex){
    V n;float d;double area;
    if(!trianglePlane(tri,n,d,area))return;
    auto& bucket=planes[key(n,d)];
    Plane* selected=nullptr;
    for(auto& p:bucket)if(belongs(p,tri,n)){selected=&p;break;}
    if(!selected){bucket.emplace_back();selected=&bucket.back();selected->normal=n;selected->offset=d;}
    selected->bounds.add(tri.a);selected->bounds.add(tri.b);selected->bounds.add(tri.c);
    selected->area+=area;++selected->triangles;selected->objects.insert(objectIndex);
}
inline QJsonObject catalogue(const v16::Runtime& rt,const QString& revision){
    QJsonArray objects,planes,lights;
    std::map<PlaneKey,std::vector<Plane>> groups;
    const auto& scene=rt.renderScene;
    for(const auto& object:rt.objects){
        const int first=object.firstTriangle,count=object.triangleCount;
        if(first<0||count<=0||size_t(first)>scene.obj.size()||size_t(count)>scene.obj.size()-size_t(first))continue;
        const auto id=scene.obj[size_t(first)].mat;
        if(id>=scene.mat.size()||!candidate(scene.mat[id]))continue;
        const auto& mat=scene.mat[id];
        Bounds bounds;const int objectIndex=int(objects.size());
        for(int k=0;k<count;++k){
            const auto& tri=scene.obj[size_t(first+k)];
            bounds.add(tri.mn);bounds.add(tri.mx);
        }
        if(!bounds.valid||bounds.uncertain)continue;
        // Smooth curved geometry can yield one plane per triangle. Drop
        // singleton planes at the catalogue stage; no GPU activation implied.
        for(int k=0;k<count;++k){
            const auto& tri=scene.obj[size_t(first+k)];
            if(tri.mat==id)addTriangle(groups,tri,objectIndex);
        }
        objects.append(QJsonObject{
            {"objectId",object.id},{"zoneHint",object.parent},{"material",object.material},
            {"firstTriangle",first},{"triangleCount",count},{"min",vec(bounds.lo)},{"max",vec(bounds.hi)},
            {"ior",mat.refractiveIndex},{"dielectricF0",dielectricF0(mat.refractiveIndex)},
            {"reflectivityPercent",int(mat.reflectivityPercent)},{"transmissionPercent",int(mat.transmissionPercent)},
            {"metallic",mat.metallic},{"roughnessScale",mat.roughnessScale},{"waterSurface",mat.waterSurface}
        });
    }
    // Camera slots are logical, not actual D3D resources. Distinct coplanar
    // patches share a candidate slot; nearby quantization bins may split.
    for(const auto& entry:groups)for(const auto& plane:entry.second){
        if(plane.triangles<2||plane.area<.005||plane.bounds.uncertain)continue;
        QJsonArray refs;for(int i:plane.objects)refs.append(i);
        const int slot=int(planes.size());
        planes.append(QJsonObject{{"cameraId",slot},{"bufferId",slot},{"state","DORMANT"},
            {"normal",vec(plane.normal)},{"offset",plane.offset},{"min",vec(plane.bounds.lo)},
            {"max",vec(plane.bounds.hi)},{"area",plane.area},{"objects",refs},
            {"sun",true},{"sky",true},{"lightDependency","all-authored-conservative"}});
    }
    for(const auto& light:scene.lights)
        lights.append(QJsonObject{{"position",vec(light.p)},{"radius",light.radius},
            {"intensity",light.intensity},{"color",vec(light.color)},{"exterior",light.exterior}});
    return {{"schema",1},{"revision",revision},{"status","diagnostic-only"},
        {"objects",objects},{"planes",planes},{"lights",lights}};
}
} // namespace m3d::zreflect
