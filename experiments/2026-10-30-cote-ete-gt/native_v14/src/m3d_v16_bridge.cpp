#include "m3d_v16_bridge.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numeric>
#include <random>

namespace m3d::v16 {
namespace {
float f(const QJsonValue& v, float fallback=0.f) { return v.isDouble()?float(v.toDouble()):fallback; }
V v3(const QJsonValue& v,V fallback={}) {
    if (!v.isArray()) return fallback;
    const auto a=v.toArray();
    if (a.size()!=3) return fallback;
    return {f(a[0]),f(a[1]),f(a[2])};
}
float axis(V v,int i){return i==0?v.x:i==1?v.y:v.z;}
V vmin(V a,V b){return{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
V vmax(V a,V b){return{std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
int byte(float v){return int(std::lround(clampf(v,0.f,1.f)*255));}
uint32_t rgb(V v){return uint32_t((byte(v.x)<<16)|(byte(v.y)<<8)|byte(v.z));}
uint16_t rgb565(V c){return uint16_t((byte(c.x)*31/255)<<11|(byte(c.y)*63/255)<<5|(byte(c.z)*31/255));}
V unpack(uint32_t c){return{float(c>>16&255)/255,float(c>>8&255)/255,float(c&255)/255};}
V unpack565(uint16_t c){return{float(c>>11)*1.f/31,float(c>>5&63)/63,float(c&31)/31};}
float srgbToLinear(float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
float linearToSrgb(float c){return c<=.0031308f?c*12.92f:1.055f*std::pow(c,1.f/2.4f)-.055f;}
V srgbToLinear(V c){return{srgbToLinear(c.x),srgbToLinear(c.y),srgbToLinear(c.z)};}
V linearToSrgb(V c){return{linearToSrgb(c.x),linearToSrgb(c.y),linearToSrgb(c.z)};}
uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
float noise(int x,int y,uint32_t seed){return float(hash32(uint32_t(x)*73856093u ^ uint32_t(y)*19349663u ^ seed)&0xffffu)/65535.f;}
V scale(V a,V b){return {a.x*b.x,a.y*b.y,a.z*b.z};}

void triangle(Scene& scene,V a,V b,V c,V uv0,V uv1,V uv2,int material,V normal={}) {
    const V e1=b-a,e2=c-a;
    const V n=length(normal)>0?norm(normal):norm(cross(e1,e2));
    if (length(cross(e1,e2))<1e-8f) return;
    Obj o;
    o.kind=Kind::Triangle;o.mat=uint8_t(material);
    o.a=a;o.b=b;o.c=c;o.n0=n;o.n1=n;o.n2=n;
    o.uv0=uv0;o.uv1=uv1;o.uv2=uv2;
    o.mn=vmin(a,vmin(b,c));o.mx=vmax(a,vmax(b,c));
    o.center=e1;o.core=e2;
    o.uv0.z=dot(e1,e1);o.uv1.z=dot(e1,e2);o.uv2.z=dot(e2,e2);
    const float d=o.uv0.z*o.uv2.z-o.uv1.z*o.uv1.z;
    o.len2=std::abs(d)>1e-20f?1.f/d:0.f;
    scene.obj.push_back(o);
}
void box(Scene& scene,V position,V size,float rotationY,int mat) {
    const float rad=rotationY*PI/180.f,cs=std::cos(rad),sn=std::sin(rad);
    auto world=[&](float x,float y,float z)->V{
        return {position.x+x*cs+z*sn,position.y+y,position.z-x*sn+z*cs};
    };
    const float x=size.x*.5f,y=size.y*.5f,z=size.z*.5f;
    const std::array<std::array<V,4>,6> faces{{
        {{{-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z}}},
        {{{x,-y,-z},{-x,-y,-z},{-x,y,-z},{x,y,-z}}},
        {{{x,-y,z},{x,-y,-z},{x,y,-z},{x,y,z}}},
        {{{-x,-y,-z},{-x,-y,z},{-x,y,z},{-x,y,-z}}},
        {{{-x,y,z},{x,y,z},{x,y,-z},{-x,y,-z}}},
        {{{-x,-y,-z},{x,-y,-z},{x,-y,z},{-x,-y,z}}}
    }};
    for (int k=0;k<6;k++){
        const auto& v=faces[k];
        V a=world(v[0].x,v[0].y,v[0].z),b=world(v[1].x,v[1].y,v[1].z),
          c=world(v[2].x,v[2].y,v[2].z),d=world(v[3].x,v[3].y,v[3].z);
        const float us=(k<2?size.x:k<4?size.z:size.x)*.5f;
        const float vs=(k<4?size.y:size.z)*.5f;
        triangle(scene,a,b,c,{0,0,0},{us,0,0},{us,vs,0},mat);
        triangle(scene,a,c,d,{0,0,0},{us,vs,0},{0,vs,0},mat);
    }
}
void sphere(Scene& scene,V position,V size,int mat) {
    constexpr int S=12,R=8;
    auto pt=[&](int i,int j)->V {
        const float a=float(i)*PI/R,b=float(j)*2*PI/S;
        return {position.x+size.x*.5f*std::sin(a)*std::cos(b),
                position.y+size.y*.5f*std::cos(a),
                position.z+size.z*.5f*std::sin(a)*std::sin(b)};
    };
    for(int i=0;i<R;i++)for(int j=0;j<S;j++){
        V a=pt(i,j),b=pt(i+1,j),c=pt(i+1,j+1),d=pt(i,j+1);
        const auto add=[&](V p,V q,V r,V u,V v,V w){
            V n=norm(cross(q-p,r-p));
            if(dot(n,p-position)<0.f){std::swap(q,r);std::swap(v,w);}
            triangle(scene,p,q,r,u,v,w,mat);
        };
        if(i>0) add(a,b,d,{float(j)/S,float(i)/R,0},{float(j)/S,float(i+1)/R,0},{float(j+1)/S,float(i)/R,0});
        if(i<R-1)add(b,c,d,{float(j)/S,float(i+1)/R,0},{float(j+1)/S,float(i+1)/R,0},{float(j+1)/S,float(i)/R,0});
    }
}
int buildBvh(Scene& s,int first,int count){
    Node node;node.mn={INF,INF,INF};node.mx={-INF,-INF,-INF};
    V cmin{INF,INF,INF},cmax{-INF,-INF,-INF};
    for(int i=first;i<first+count;i++){
        const auto& o=s.obj[s.order[size_t(i)]];
        node.mn=vmin(node.mn,o.mn);node.mx=vmax(node.mx,o.mx);
        V centroid=(o.mn+o.mx)*.5f;
        cmin=vmin(cmin,centroid);cmax=vmax(cmax,centroid);
    }
    const int index=int(s.node.size());
    s.node.push_back(node);
    if(count<=6){s.node[size_t(index)].start=first;s.node[size_t(index)].count=count;return index;}
    V d=cmax-cmin;
    const int axisId=(d.x>=d.y&&d.x>=d.z)?0:((d.y>=d.z)?1:2);
    const int midpoint=first+count/2;
    std::nth_element(s.order.begin()+first,s.order.begin()+midpoint,s.order.begin()+first+count,[&](uint32_t a,uint32_t b){
        const Obj& x=s.obj[a];const Obj& y=s.obj[b];
        return axis((x.mn+x.mx)*.5f,axisId)<axis((y.mn+y.mx)*.5f,axisId);
    });
    const int left=buildBvh(s,first,midpoint-first);
    const int right=buildBvh(s,midpoint,first+count-midpoint);
    s.node[size_t(index)].left=left;s.node[size_t(index)].right=right;
    return index;
}
void generatePbr(Material& mat,V base,float roughness,const QString& pattern,int resolution) {
    ColorLevel first;first.size=uint32_t(resolution);first.p.resize(size_t(resolution)*resolution);
    Level nr;nr.size=uint32_t(resolution);nr.p.resize(first.p.size());
    Level rr;rr.size=uint32_t(resolution);rr.p.resize(first.p.size());
    const uint32_t seed=hash32(uint32_t(qHash(pattern)));
    for(int y=0;y<resolution;y++)for(int x=0;x<resolution;x++){
        const float u=float(x)/resolution,v=float(y)/resolution;
        const float n=noise(x/5,y/5,seed),fine=noise(x,y,seed^0x9e3779b9u);
        float factor=1.f;
        if(pattern=="wood")factor=.76f+.18f*std::sin(u*105.f+std::sin(v*17)*2)+.10f*n;
        else if(pattern=="concrete"||pattern=="plaster")factor=.89f+.20f*n+.04f*fine;
        else if(pattern=="stone")factor=.75f+.23f*std::sin(u*23+v*15)+.15f*n;
        else if(pattern=="fabric")factor=.83f+.15f*std::sin(u*700)*std::sin(v*700)+.07f*n;
        else if(pattern=="lawn"||pattern=="foliage")factor=.73f+.4f*n+.1f*fine;
        else if(pattern=="metal")factor=.87f+.12f*n;
        const V color=scale(base,{factor,factor,factor});
        const size_t i=size_t(y)*resolution+x;
        first.p[i]=rgb({clampf(color.x,0,1),clampf(color.y,0,1),clampf(color.z,0,1)});
        const float d=(n-.5f)*.11f;
        V normal=norm({d,d*.55f,1.f});
        nr.p[i]=rgb565({(normal.x+1)*.5f,(normal.y+1)*.5f,(normal.z+1)*.5f});
        const float r=clampf(roughness+(n-.5f)*.09f,.03f,.98f);
        rr.p[i]=rgb565({r,r,r});
    }
    mat.colorMip.push_back(std::move(first));
    mat.normalMip.push_back(std::move(nr));
    mat.roughnessMip.push_back(std::move(rr));
    while(mat.colorMip.back().size>1) {
        const auto& source=mat.colorMip.back();
        const auto& normals=mat.normalMip.back();
        const auto& rough=mat.roughnessMip.back();
        const int prev=int(source.size),next=prev/2;
        ColorLevel target;target.size=uint32_t(next);target.p.resize(size_t(next)*next);
        Level nt;nt.size=uint32_t(next);nt.p.resize(target.p.size());
        Level rt;rt.size=uint32_t(next);rt.p.resize(target.p.size());
        for(int y=0;y<next;y++)for(int x=0;x<next;x++){
            V sum{},normal{};float squared=0;
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
                const size_t at=size_t(y*2+dy)*prev+x*2+dx;
                sum=sum+srgbToLinear(unpack(source.p[at]));
                const V n=unpack565(normals.p[at])*2.f-V{1,1,1};normal=normal+n;
                const float r=unpack565(rough.p[at]).x;squared+=r*r;
            }
            sum=linearToSrgb(sum*.25f);normal=norm(normal);
            const float roughAvg=std::sqrt(squared*.25f);
            const size_t out=size_t(y)*next+x;
            target.p[out]=rgb(sum);
            nt.p[out]=rgb565((normal+V{1,1,1})*.5f);
            rt.p[out]=rgb565({roughAvg,roughAvg,roughAvg});
        }
        mat.colorMip.push_back(std::move(target));
        mat.normalMip.push_back(std::move(nt));
        mat.roughnessMip.push_back(std::move(rt));
    }
}
bool finite3(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
} // namespace

BuildResult buildRuntime(const SceneDatabase& source,int maxTextureResolution) {
    BuildResult result;
    result.runtime.title=source.name;
    maxTextureResolution=std::clamp(maxTextureResolution,64,2048);
    if(source.sectors.empty()){result.error="No sectors";return result;}
    auto& render=result.runtime.renderScene;
    QHash<QString,QJsonObject> meshes;
    QHash<QString,QJsonObject> sets;
    QVector<QJsonObject> instanceDocs;
    QVector<QJsonObject> materialDocs;
    for(const auto& sector:source.sectors){
        auto accumulate=[&](const QString& kind,const QString& key,QHash<QString,QJsonObject>& target){
            for(const auto& file:sector.resources.value(kind)){
                for(const auto& entry:file.data.value(key).toArray()){
                    const auto item=entry.toObject();
                    const QString id=item.value("id").toString();
                    if(!id.isEmpty()) target.insert(id,item);
                }
            }
        };
        accumulate("geometry","meshes",meshes);
        accumulate("pbr","sets",sets);
        for(const auto& file:sector.resources.value("materials"))
            for(const auto& item:file.data.value("materials").toArray())materialDocs.append(item.toObject());
        for(const auto& file:sector.resources.value("instances"))
            for(const auto& item:file.data.value("instances").toArray())instanceDocs.append(item.toObject());
        for(const auto& file:sector.resources.value("cameras"))
            for(const auto& item:file.data.value("cameras").toArray()){
                const auto j=item.toObject();
                CameraPreset c;c.id=j.value("id").toString();c.name=j.value("name").toString(c.id);
                c.eye=v3(j.value("position"));c.target=v3(j.value("target"));
                c.fov=f(j.value("fovDegrees"),55.f);
                result.runtime.cameras.push_back(c);
            }
        for(const auto& file:sector.resources.value("lighting")) {
            const float requestedHour=f(file.data.value("hour"),15.f);
            if(std::isfinite(requestedHour) && requestedHour>=6.f && requestedHour<=19.f)
                result.runtime.daylightHour=requestedHour;
            for(const auto& item:file.data.value("lights").toArray()){
                const auto j=item.toObject();Light l;
                l.p=v3(j.value("position"));l.radius=f(j.value("radius"),8.f);
                l.intensity=f(j.value("intensity"),1.f);
                l.color=v3(j.value("color"),{255,220,190});
                render.lights.push_back(l);
            }
        }
    }
    if(materialDocs.isEmpty()||meshes.isEmpty()||instanceDocs.isEmpty()){
        result.error="Missing meshes, instances or materials";return result;
    }
    if(materialDocs.size()>256){result.error="Material count exceeds 256";return result;}
    for(const auto& j:materialDocs){
        const QString id=j.value("id").toString();
        if(id.isEmpty()||result.runtime.materialIndex.contains(id)){
            result.error="Duplicate or missing material: "+id;return result;
        }
        const auto base=v3(j.value("baseColor"),{.7f,.7f,.7f});
        const float rough=f(j.value("roughness"),.72f);
        const float metal=f(j.value("metallic"),0.f);
        const float transmission=f(j.value("transmission"),0.f);
        if(!finite3(base)||rough<0||rough>1||metal<0||metal>1||transmission<0||transmission>1){
            result.error="Invalid PBR properties: "+id;return result;
        }
        Material material;material.name=id.toStdString();
        material.reflectivityPercent=uint8_t(std::lround(clampf(.04f+metal*.80f,0,1)*100));
        material.transmissionPercent=uint8_t(std::lround(transmission*100));
        material.refractiveIndex=f(j.value("ior"),1.5f);
        material.metallic=metal;
        material.roughnessScale=clampf(f(j.value("roughnessScale"),1.f),.01f,2.f);
        const auto texId=j.value("textureSet").toString();
        if(!sets.contains(texId)){result.error="Missing texture set: "+texId;return result;}
        const auto set=sets.value(texId);
        const int requested=int(set.value("resolution").toInt(1024));
        int size=std::min(maxTextureResolution,requested);
        if(size<64||size>2048||(size&(size-1))){result.error="Invalid mip resolution";return result;}
        generatePbr(material,base,rough,set.value("pattern").toString("plaster"),size);
        result.runtime.textureResolution=std::max(result.runtime.textureResolution,size);
        result.runtime.materialIndex.insert(id,int(render.mat.size()));
        render.mat.push_back(std::move(material));
    }
    QSet<QString> instanceIds;
    for(const auto& j:instanceDocs){
        ObjectInfo object;
        object.id=j.value("id").toString();object.parent=j.value("parent").toString();
        object.material=j.value("material").toString();
        const QString meshId=j.value("geometry").toString();
        if(object.id.isEmpty()||instanceIds.contains(object.id)||!meshes.contains(meshId)
            ||!result.runtime.materialIndex.contains(object.material)){
            result.error="Invalid or duplicate object: "+object.id;return result;
        }
        instanceIds.insert(object.id);
        const auto size=v3(j.value("size"),{1,1,1});
        const auto pos=v3(j.value("position"));
        if(!finite3(size)||!finite3(pos)||size.x<=0||size.y<=0||size.z<=0){
            result.error="Invalid transform: "+object.id;return result;
        }
        const int material=result.runtime.materialIndex.value(object.material);
        object.firstTriangle=int(render.obj.size());
        const auto type=meshes.value(meshId).value("type").toString();
        if(type=="box")box(render,pos,size,f(j.value("rotationY")),material);
        else if(type=="uv_sphere")sphere(render,pos,size,material);
        else {result.error="Unsupported V16 geometry: "+meshId;return result;}
        object.triangleCount=int(render.obj.size())-object.firstTriangle;
        result.runtime.objects.push_back(object);
    }
    if(render.obj.empty()){result.error="No triangles generated";return result;}
    render.order.resize(render.obj.size());
    std::iota(render.order.begin(),render.order.end(),0u);
    render.root=buildBvh(render,0,int(render.order.size()));
    return result;
}
Camera makeCamera(const CameraPreset& preset,int width,int height) {
    Camera c;
    c.eye=preset.eye;c.f=norm(preset.target-preset.eye);
    c.r=norm(cross(c.f,{0,1,0}));
    c.u=norm(cross(c.r,c.f));
    c.tanHalf=std::tan(clampf(preset.fov,20.f,100.f)*PI/360.f);
    c.aspect=float(width)/std::max(height,1);
    c.pixelAngle=2*c.tanHalf/std::max(width,1);
    return c;
}
}
