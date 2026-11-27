#pragma once
// Restrictive offline catalogue. It is not yet consumed by the GPU renderer.
#include "m3d_v16_bridge.h"
#include "m3d_zreflect_geometry.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <set>
namespace m3d::zreflect {
inline QJsonArray vec(V v){return {v.x,v.y,v.z};}
inline Permission permission(const QJsonValue& value){
    if(value.isUndefined())return Permission::Unspecified;
    const auto s=value.toString();
    if(s=="enabled")return Permission::Enabled;
    if(s=="disabled")return Permission::Disabled;
    if(s=="auto")return Permission::Auto;
    return Permission::Invalid;
}
inline Permission resolvePermission(const QJsonObject& object,const QJsonObject& material,const QJsonObject& manifest){
    for(const auto& o:{object,material,manifest}){
        auto p=permission(o.value("planarReflection"));if(p!=Permission::Unspecified)return p;
    }
    return Permission::Unspecified;
}
inline QString permissionName(Permission p){
    switch(p){case Permission::Disabled:return "disabled";case Permission::Enabled:return "enabled";
        case Permission::Auto:return "auto";case Permission::Invalid:return "invalid";default:return "unspecified";}
}
inline QHash<QString,QJsonObject> authored(const v16::SceneDatabase& db,const QString& kind){
    QHash<QString,QJsonObject> out;
    for(const auto& sector:db.sectors)for(const auto& file:sector.resources.value(kind))
        for(const auto& value:file.data.value(kind).toArray()){
            auto item=value.toObject();out.insert(item.value("id").toString(),item);
        }
    return out;
}
struct Group{Patch patch;std::set<int> objects;bool explicitEnable=false;};
inline void mergePatch(std::map<PlaneKey,std::vector<Group>>& groups,const Scene& scene,
                       Patch p,int objectIndex,bool enabled){
    auto& bucket=groups[key(p.normal,p.offset)];Group* g=nullptr;
    for(auto& item:bucket){
        if(item.patch.side!=p.side||dot(item.patch.normal,p.normal)<.999999f)continue;
        bool compatible=true;
        for(auto id:p.triangles)if(!samePlane(item.patch,scene.obj[id],p.normal)){compatible=false;break;}
        if(compatible){g=&item;break;}
    }
    if(!g){bucket.emplace_back();g=&bucket.back();g->patch.normal=p.normal;g->patch.offset=p.offset;g->patch.side=p.side;}
    g->patch.bounds.add(p.bounds.lo);g->patch.bounds.add(p.bounds.hi);
    g->patch.area+=p.area;g->patch.triangles.insert(g->patch.triangles.end(),p.triangles.begin(),p.triangles.end());
    g->objects.insert(objectIndex);g->explicitEnable|=enabled;
}
inline QJsonObject catalogue(const v16::Runtime& rt,const v16::SceneDatabase& db,
                            const QJsonObject& manifest,const Policy& policy={}){
    const auto instances=authored(db,"instances"),materials=authored(db,"materials");
    const auto& scene=rt.renderScene;QJsonArray objects,planes,lights,diagnostics,cameras;
    std::map<PlaneKey,std::vector<Group>> groups;int optical=0,rejectedGeometry=0,refused=0;
    for(const auto& object:rt.objects){
        const auto src=instances.value(object.id),matSrc=materials.value(object.material);
        const auto perm=resolvePermission(src,matSrc,manifest);
        if(perm==Permission::Disabled||perm==Permission::Invalid){++refused;continue;}
        int first=object.firstTriangle,count=object.triangleCount;
        if(first<0||count<=0||size_t(first)>scene.obj.size()||size_t(count)>scene.obj.size()-size_t(first))continue;
        const auto id=scene.obj[size_t(first)].mat;if(id>=scene.mat.size())continue;
        const auto& mat=scene.mat[id];
        const bool water=mat.waterSurface||(!mat.surfaceAuthored&&mat.name=="cse_pool_water");
        // Nominal source roughness is not roughnessScale or sampled texture roughness.
        double rough=matSrc.value("roughness").toDouble(.72);
        const bool mirror=matSrc.value("opticalMirror").toBool(false);
        if(!opticalEligible(water,mirror,double(mat.transmissionPercent)/100.,mat.metallic,rough,perm,policy))continue;
        ++optical;
        auto patches=extractSlab(scene,first,count,water,perm==Permission::Enabled,policy);
        if(patches.empty()){
            ++rejectedGeometry;diagnostics.append(QJsonObject{{"objectId",object.id},{"reason","non-planar-or-small"}});
            continue;
        }
        Bounds bounds;for(const auto& patch:patches){bounds.add(patch.bounds.lo);bounds.add(patch.bounds.hi);}
        const int objectIndex=int(objects.size());
        // Unspecified objects remain hypothetical candidates only; production
        // authorization is not granted by this diagnostic.
        objects.append(QJsonObject{{"objectId",object.id},{"zoneHint",object.parent},{"material",object.material},
            {"permission",permissionName(perm)},{"authorized",perm==Permission::Auto||perm==Permission::Enabled||water},
            {"firstTriangle",first},{"triangleCount",count},{"min",vec(bounds.lo)},{"max",vec(bounds.hi)},
            {"ior",mat.refractiveIndex},{"dielectricF0",dielectricF0(mat.refractiveIndex)},
            {"reflectivityPercent",int(mat.reflectivityPercent)},{"transmissionPercent",int(mat.transmissionPercent)},
            {"metallic",mat.metallic},{"roughnessNominal",rough},{"roughnessScale",mat.roughnessScale},{"waterSurface",water}});
        for(auto& patch:patches)mergePatch(groups,scene,std::move(patch),objectIndex,perm==Permission::Enabled);
    }
    for(const auto& kv:groups)for(const auto& g:kv.second){
        const auto& p=g.patch;QJsonArray refs,triangles;for(int i:g.objects)refs.append(i);
        for(auto id:p.triangles)triangles.append(int(id));
        int slot=int(planes.size());planes.append(QJsonObject{{"cameraId",slot},{"bufferId",slot},{"state","DORMANT"},
            {"normal",vec(p.normal)},{"offset",p.offset},{"side",p.side},{"min",vec(p.bounds.lo)},{"max",vec(p.bounds.hi)},
            {"area",p.area},{"objects",refs},{"triangles",triangles},{"sun",true},{"sky",true},
            {"lightDependency","all-authored-conservative"}});
    }
    // Per authored view, all eligible planes are evaluated. No top-three truncation.
    for(const auto& preset:rt.cameras){
        const Camera c=v16::makeCamera(preset,1280,720);int visible=0,selected=0;
        for(const auto& kv:groups)for(const auto& g:kv.second){
            double area=projectedPatch(scene,g.patch,c);
            if(area>0){++visible;if(area>=policy.minProjected||g.explicitEnable)++selected;}
        }
        cameras.append(QJsonObject{{"id",preset.id},{"visible",visible},{"selectedUncapped",selected}});
    }
    for(const auto& l:scene.lights)lights.append(QJsonObject{{"position",vec(l.p)},{"radius",l.radius},
        {"intensity",l.intensity},{"color",vec(l.color)},{"exterior",l.exterior}});
    return {{"schema",1},{"catalogRevision",2},{"revision",db.revision},{"status","diagnostic-only"},
        {"objects",objects},{"planes",planes},{"lights",lights},{"rejected",diagnostics},{"cameraSimulation",cameras},
        {"policy",QJsonObject{{"maxNominalRoughness",policy.maxNominalRoughness},{"minArea",policy.minArea},
            {"minProjected",policy.minProjected},{"futureCameraLimit",int(policy.futureCameraLimit)},{"cameraLimitApplied",false}}},
        {"counts",QJsonObject{{"totalObjects",int(rt.objects.size())},{"opticalCandidates",optical},
            {"rejectedGeometry",rejectedGeometry},{"authorRefused",refused},{"eligibleObjects",int(objects.size())},{"planes",int(planes.size())}}}};
}
}
