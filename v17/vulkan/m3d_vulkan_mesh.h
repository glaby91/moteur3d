#pragma once
// CSE WARP Vulkan: first raster port uses the exact V17 mesh tessellation rules.
// The source D3D11 renderer remains untouched, enabling image/geometry comparisons.
#include "m3d_engine.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace m3d { namespace vkmesh {

struct GpuVertex { float p[3], n[3], uv[2]; uint32_t material; };

struct TransparentTriangle { GpuVertex v[3]; V center; };

void put(GpuVertex& v,V p,V n,float u,float vv,uint32_t mat) {
    v={{p.x,p.y,p.z},{n.x,n.y,n.z},{u,vv},mat};
}
float uvScaleForMaterial(const Scene& scene,uint32_t mat) {
    if(mat<scene.mat.size()){
        const std::string& name=scene.mat[mat].name;
        if(name=="seatSteel")return .95f;
        if(name=="seatFabricClean"||name=="seatTrim"||name=="seatVinylBrown"||name=="seatSupportPaint")return .85f;
        if(name=="seatWood")return .65f;
        if(name=="mural")return .075f;
        // The old 0.16 scale stretched the 1k surface maps over 6.25 m.
        // Fine wear and normal detail then vanished at native output sizes.
        if(name=="concrete")return .34f;
        if(name=="stone"||name=="brick")return .28f;
        if(name=="glass"||name=="raptureGlass"||name=="raptureStone")return .16f;
    }
    return .65f;
}

class MeshBuilder {
public:
    std::vector<GpuVertex> vertices;
    std::vector<TransparentTriangle> transparent;
    bool transparentMaterial=false;
    void tri(V a,V b,V c,V na,V nb,V nc,float ua,float va,float ub,float vb,float uc,float vc,uint32_t mat) {
        if(vertices.size()+transparent.size()*3>12000000u)return;
        GpuVertex v[3]{};put(v[0],a,na,ua,va,mat);put(v[1],b,nb,ub,vb,mat);put(v[2],c,nc,uc,vc,mat);
        if(transparentMaterial){TransparentTriangle t{};std::copy(v,v+3,t.v);t.center=(a+b+c)/3.f;transparent.push_back(t);}
        else vertices.insert(vertices.end(),v,v+3);
    }
    static V boxUv(V p,V n,float scale) {
        if(std::abs(n.y)>=std::abs(n.x)&&std::abs(n.y)>=std::abs(n.z))return {p.x*scale,p.z*scale,0};
        if(std::abs(n.x)>=std::abs(n.z))return {p.z*scale,p.y*scale,0};
        return {p.x*scale,p.y*scale,0};
    }
    void face(V a,V b,V c,V d,V n,uint32_t mat,float uvScale) {
        V ta=boxUv(a,n,uvScale),tb=boxUv(b,n,uvScale),tc=boxUv(c,n,uvScale),td=boxUv(d,n,uvScale);
        tri(a,b,c,n,n,n,ta.x,ta.y,tb.x,tb.y,tc.x,tc.y,mat);tri(a,c,d,n,n,n,ta.x,ta.y,tc.x,tc.y,td.x,td.y,mat);
    }
    void box(V mn,V mx,uint32_t mat,float uvScale) {
        face({mn.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mn.y,mn.z},{0,0,-1},mat,uvScale);
        face({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},{0,0,1},mat,uvScale);
        face({mn.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},{-1,0,0},mat,uvScale);
        face({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},{mx.x,mn.y,mx.z},{1,0,0},mat,uvScale);
        face({mn.x,mn.y,mx.z},{mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},{0,-1,0},mat,uvScale);
        face({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mx.y,mn.z},{0,1,0},mat,uvScale);
    }
    void roundedBox(V mn,V mx,float radius,uint32_t mat,float uvScale) {
        V center=(mn+mx)*.5f,half=(mx-mn)*.5f;
        radius=std::max(0.f,std::min(radius,std::min({half.x,half.y,half.z})));
        V inner{std::max(0.f,half.x-radius),std::max(0.f,half.y-radius),std::max(0.f,half.z-radius)};
        auto coords=[](float h,float in){
            std::vector<float> out{-h,-in,0.f,in,h};
            std::sort(out.begin(),out.end());
            out.erase(std::unique(out.begin(),out.end(),[](float a,float b){return std::abs(a-b)<1e-6f;}),out.end());
            return out;
        };
        const auto xs=coords(half.x,inner.x),ys=coords(half.y,inner.y),zs=coords(half.z,inner.z);
        auto emitFace=[&](int axis,float sign,const std::vector<float>& u,const std::vector<float>& v){
            auto point=[&](float a,float b){V p=center;if(axis==0){p.x+=sign*half.x;p.y+=a;p.z+=b;}else if(axis==1){p.y+=sign*half.y;p.x+=a;p.z+=b;}else{p.z+=sign*half.z;p.x+=a;p.y+=b;}V q{clampf(p.x,center.x-inner.x,center.x+inner.x),clampf(p.y,center.y-inner.y,center.y+inner.y),clampf(p.z,center.z-inner.z,center.z+inner.z)};V n=norm(p-q);return std::pair<V,V>{q+n*radius,n};};
            for(size_t y=0;y+1<v.size();y++)for(size_t x=0;x+1<u.size();x++){
                auto p00=point(u[x],v[y]),p10=point(u[x+1],v[y]),p11=point(u[x+1],v[y+1]),p01=point(u[x],v[y+1]);
                V uv00=boxUv(p00.first,p00.second,uvScale),uv10=boxUv(p10.first,p10.second,uvScale),uv11=boxUv(p11.first,p11.second,uvScale),uv01=boxUv(p01.first,p01.second,uvScale);
                tri(p00.first,p10.first,p11.first,p00.second,p10.second,p11.second,uv00.x,uv00.y,uv10.x,uv10.y,uv11.x,uv11.y,mat);
                tri(p00.first,p11.first,p01.first,p00.second,p11.second,p01.second,uv00.x,uv00.y,uv11.x,uv11.y,uv01.x,uv01.y,mat);
            }
        };
        for(float s:{-1.f,1.f}){emitFace(0,s,ys,zs);emitFace(1,s,xs,zs);emitFace(2,s,xs,ys);}
    }
    void ellipsoid(V c,V r,uint32_t mat) {
        const float radius=std::max({std::abs(r.x),std::abs(r.y),std::abs(r.z)});
        const int rings=std::clamp(int(std::lround(8.f+radius*8.f)),8,32),slices=rings*2;
        auto point=[&](int y,int x){float phi=PI*float(y)/rings,theta=2*PI*float(x)/slices;return V{c.x+r.x*std::sin(phi)*std::cos(theta),c.y+r.y*std::cos(phi),c.z+r.z*std::sin(phi)*std::sin(theta)};};
        auto normal=[&](V p){return norm({(p.x-c.x)/std::max(1e-5f,r.x*r.x),(p.y-c.y)/std::max(1e-5f,r.y*r.y),(p.z-c.z)/std::max(1e-5f,r.z*r.z)});};
        for(int y=0;y<rings;y++)for(int x=0;x<slices;x++){
            V p00=point(y,x),p01=point(y,x+1),p10=point(y+1,x),p11=point(y+1,x+1);
            V n00=normal(p00),n01=normal(p01),n10=normal(p10),n11=normal(p11);
            float u0=2.5f*float(x)/slices,u1=2.5f*float(x+1)/slices,v0=2.5f*float(y)/rings,v1=2.5f*float(y+1)/rings;
            tri(p00,p10,p11,n00,n10,n11,u0,v0,u0,v1,u1,v1,mat);
            tri(p00,p11,p01,n00,n11,n01,u0,v0,u1,v1,u1,v0,mat);
        }
    }
    void frustum(V a,V axis,float len,float r0,float r1,uint32_t mat) {
        axis=norm(axis);V ref=std::abs(axis.y)<.9f?V{0,1,0}:V{1,0,0};V u=norm(cross(axis,ref)),v=cross(axis,u);constexpr int slices=12;V b=a+axis*len;
        for(int i=0;i<slices;i++){
            float t0=2*PI*i/slices,t1=2*PI*(i+1)/slices;V d0=u*std::cos(t0)+v*std::sin(t0),d1=u*std::cos(t1)+v*std::sin(t1);
            V p0=a+d0*r0,p1=a+d1*r0,p2=b+d1*r1,p3=b+d0*r1;V n0=norm(d0*len-axis*(r1-r0)),n1=norm(d1*len-axis*(r1-r0));
            tri(p0,p1,p2,n0,n1,n1,float(i)/slices,0,float(i+1)/slices,0,float(i+1)/slices,1,mat);
            tri(p0,p2,p3,n0,n1,n0,float(i)/slices,0,float(i+1)/slices,1,float(i)/slices,1,mat);
            tri(a,p1,p0,axis*-1,axis*-1,axis*-1,0,0,1,0,0,1,mat);tri(b,p3,p2,axis,axis,axis,0,0,0,1,1,1,mat);
        }
    }
    void capsule(V a,V b,float radius,uint32_t mat) {
        V axis=b-a;const float len=length(axis);if(len<1e-6f){ellipsoid(a,{radius,radius,radius},mat);return;}axis=axis/len;
        V ref=std::abs(axis.y)<.9f?V{0,1,0}:V{1,0,0};V u=norm(cross(axis,ref)),v=norm(cross(axis,u));
        constexpr int slices=12,capRings=4;V centers[2]={a,b};
        for(int end=0;end<2;end++){
            for(int ring=0;ring<capRings;ring++){
                float p0=(PI*.5f)*float(ring)/capRings,p1=(PI*.5f)*float(ring+1)/capRings;
                for(int s=0;s<slices;s++){
                    float t0=2*PI*float(s)/slices,t1=2*PI*float(s+1)/slices;
                    V d0=u*std::cos(t0)+v*std::sin(t0),d1=u*std::cos(t1)+v*std::sin(t1);
                    auto point=[&](float phi,V d){float axial=(end?1.f:-1.f)*std::cos(phi);V n=d*std::sin(phi)+axis*axial;return std::pair<V,V>{centers[end]+axis*(axial*radius)+d*(std::sin(phi)*radius),n};};
                    auto p00=point(p0,d0),p01=point(p0,d1),p10=point(p1,d0),p11=point(p1,d1);
                    float u0=float(s)/slices,u1=float(s+1)/slices;float vBase=end?len*.55f:-radius*.55f;float v0=vBase+radius*.55f*float(ring)/capRings,v1=vBase+radius*.55f*float(ring+1)/capRings;
                    tri(p00.first,p10.first,p11.first,p00.second,p10.second,p11.second,u0,v0,u0,v1,u1,v1,mat);
                    tri(p00.first,p11.first,p01.first,p00.second,p11.second,p01.second,u0,v0,u1,v1,u1,v0,mat);
                }
            }
        }
        for(int s=0;s<slices;s++){
            float t0=2*PI*float(s)/slices,t1=2*PI*float(s+1)/slices;V d0=u*std::cos(t0)+v*std::sin(t0),d1=u*std::cos(t1)+v*std::sin(t1);
            V p0=a+d0*radius,p1=a+d1*radius,p2=b+d1*radius,p3=b+d0*radius;
            tri(p0,p1,p2,d0,d1,d1,float(s)/slices,0,float(s+1)/slices,0,float(s+1)/slices,len*.55f,mat);tri(p0,p2,p3,d0,d1,d0,float(s)/slices,0,float(s+1)/slices,len*.55f,float(s)/slices,len*.55f,mat);
        }
    }
    void ground(const Scene& scene) {
        if (scene.obj.empty()) return;
        V mn=scene.obj.front().mn,mx=scene.obj.front().mx;
        for(const Obj& o:scene.obj){
            mn.x=std::min(mn.x,o.mn.x);mn.z=std::min(mn.z,o.mn.z);
            mx.x=std::max(mx.x,o.mx.x);mx.z=std::max(mx.z,o.mx.z);
        }
        const float step=.5f;
        const float x0=std::floor(mn.x-4.f),z0=std::floor(mn.z-4.f);
        const float x1=std::ceil(mx.x+4.f),z1=std::ceil(mx.z+4.f);
        for(float z=z0;z<z1;z+=step)for(float x=x0;x<x1;x+=step){
            if(vertices.size()>12000000u)return;
            const uint32_t mat=uint32_t(std::max(0,groundMaterial(scene,{x+step*.5f,0,z+step*.5f})));
            V a{x,0,z},b{x,0,z+step},c{x+step,0,z+step},d{x+step,0,z};
            tri(a,b,c,{0,1,0},{0,1,0},{0,1,0},x*.34f,z*.34f,x*.34f,(z+step)*.34f,(x+step)*.34f,(z+step)*.34f,mat);
            tri(a,c,d,{0,1,0},{0,1,0},{0,1,0},x*.34f,z*.34f,(x+step)*.34f,(z+step)*.34f,(x+step)*.34f,z*.34f,mat);
        }
    }
};

struct BoxInstance {
    float lo[3],uvScale;
    float hi[3];uint32_t material;
    float dimensions[3];uint32_t uvMode;
};
static_assert(sizeof(BoxInstance)==48,"Vulkan and DX11 box instance formats must match");
inline MeshBuilder build(const Scene& scene,std::vector<BoxInstance>* boxes=nullptr) {
    MeshBuilder mesh;
    std::vector<uint8_t> skip;
    if(boxes) {
        skip.assign(scene.obj.size(),0);
        for(const SourceBox& source:scene.sourceBoxes) {
            if(source.triangleCount!=12 || source.material>=scene.mat.size() ||
               scene.mat[source.material].transmissionPercent>0 ||
               size_t(source.firstTriangle)+12>scene.obj.size())continue;
            bool valid=true;
            for(size_t k=source.firstTriangle;k<size_t(source.firstTriangle)+12;k++) {
                if(skip[k] || scene.obj[k].kind!=Kind::Triangle ||
                   uint32_t(scene.obj[k].mat)!=source.material){valid=false;break;}
            }
            if(!valid)continue;
            BoxInstance inst{};
            inst.lo[0]=source.mn.x;inst.lo[1]=source.mn.y;inst.lo[2]=source.mn.z;
            inst.hi[0]=source.mx.x;inst.hi[1]=source.mx.y;inst.hi[2]=source.mx.z;
            inst.dimensions[0]=source.mx.x-source.mn.x;
            inst.dimensions[1]=source.mx.y-source.mn.y;
            inst.dimensions[2]=source.mx.z-source.mn.z;
            inst.material=source.material;
            const auto& name=scene.mat[source.material].name;
            inst.uvMode=(name=="cse_energetik_fresco"||
                         name=="cse_personnage_facade")?2u:1u;
            boxes->push_back(inst);
            for(size_t k=source.firstTriangle;k<size_t(source.firstTriangle)+12;k++)skip[k]=1;
        }
    }
    for(size_t objectIndex=0;objectIndex<scene.obj.size();objectIndex++) {
        if(!skip.empty()&&skip[objectIndex])continue;
        const Obj& o=scene.obj[objectIndex];
        const uint32_t m=o.mat;
        const float scale=uvScaleForMaterial(scene,m);
        mesh.transparentMaterial=m<scene.mat.size() && scene.mat[m].transmissionPercent>0;
        switch(o.kind) {
          case Kind::Triangle:
            mesh.tri(o.a,o.b,o.c,o.n0,o.n1,o.n2,
                     o.uv0.x,o.uv0.y,o.uv1.x,o.uv1.y,o.uv2.x,o.uv2.y,m);
            break;
          case Kind::Box:
            if(boxes&&!mesh.transparentMaterial){
                BoxInstance inst{};
                inst.lo[0]=o.mn.x;inst.lo[1]=o.mn.y;inst.lo[2]=o.mn.z;
                inst.hi[0]=o.mx.x;inst.hi[1]=o.mx.y;inst.hi[2]=o.mx.z;
                inst.dimensions[0]=o.mx.x-o.mn.x;
                inst.dimensions[1]=o.mx.y-o.mn.y;
                inst.dimensions[2]=o.mx.z-o.mn.z;
                inst.uvScale=scale;inst.material=m;inst.uvMode=0;
                boxes->push_back(inst);
            }else mesh.box(o.mn,o.mx,m,scale);
            break;
          case Kind::RoundBox: mesh.roundedBox(o.mn,o.mx,o.r,m,scale);break;
          case Kind::Capsule: mesh.capsule(o.a,o.b,o.r,m);break;
          case Kind::Cone: mesh.frustum(o.a,o.n0,o.len,o.r,o.r1,m);break;
          case Kind::Ellipse: mesh.ellipsoid(o.a,o.b,m);break;
        }
        mesh.transparentMaterial=false;
        if(mesh.vertices.size()+mesh.transparent.size()*3 > 12000000u)break;
    }
    mesh.ground(scene);
    return mesh;
}

}} // namespace m3d::vkmesh
