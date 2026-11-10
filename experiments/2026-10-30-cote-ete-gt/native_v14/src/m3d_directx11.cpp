#include "m3d_engine.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <cmath>
#include <limits>

using Microsoft::WRL::ComPtr;

namespace m3d {
namespace {
// Single atomic configuration word: host and GUI may update quality while
// the dedicated GPU worker is rendering. Each frame gets one full snapshot.
// Bits: 0..7 MSAA, 8..15 anisotropy, 16..31 LOD quality percent,
// 32 culling, 33 LOD.
constexpr uint64_t defaultQualityBits=4ull|(16ull<<8)|(100ull<<16)|(1ull<<32)|(1ull<<33);
std::atomic<uint64_t> gpuQualityBits{defaultQualityBits};
uint64_t packQuality(const RasterQualitySettings& q){
    return uint64_t(q.msaaSamples)|(uint64_t(q.anisotropy)<<8)
        |(uint64_t(q.lodQualityPercent)<<16)
        |(uint64_t(q.frustumCulling?1u:0u)<<32)
        |(uint64_t(q.dynamicLod?1u:0u)<<33);
}
RasterQualitySettings unpackQuality(uint64_t bits){
    RasterQualitySettings q;
    q.msaaSamples=uint32_t(bits&255u);
    q.anisotropy=uint32_t((bits>>8)&255u);
    q.lodQualityPercent=uint32_t((bits>>16)&65535u);
    q.frustumCulling=((bits>>32)&1u)!=0;
    q.dynamicLod=((bits>>33)&1u)!=0;
    return q;
}
struct GpuVertex { float p[3], n[3], uv[2]; uint32_t material; };
// Box geometry is immutable and shared. Per-object bounds/materials occupy
// 32 bytes instead of duplicating 36 vertex records (~1.3 kB) per box.
struct GpuBoxInstance {
    float lo[3], uvScale;
    float hi[3]; uint32_t material;
    float size[3]; uint32_t uvMode;
};
static_assert(sizeof(GpuBoxInstance)==48,"D3D11 per-instance layout must be 48 bytes");
// Ellipsoids use a single dynamic instance buffer per frame. Three immutable
// unit-sphere meshes provide GPU LOD without regenerating scene geometry.
struct GpuEllipseInstance {
    float center[3];uint32_t material;
    float radii[3];float padding;
};
static_assert(sizeof(GpuEllipseInstance)==32,"D3D11 ellipsoid instance size");
struct TransparentTriangle { GpuVertex v[3]; V center; };
struct FrameConstants {
    float eye[3], tanHalf;
    float forward[3], aspect;
    float right[3], nearPlane;
    float up[3], farPlane;
    float light[3], ambient;
    float sky[3]; uint32_t materialCount;
    float shadowBase[3], shadowSpan;
    float shadowRight[3], shadowDist;
    float shadowUp[3], shadowSize;
    float materialGlow[256][4];
    float materialSpecular[256][4];
    float materialOpacity[256][4];
    float materialDiffuse[256][4];
    float materialEmission[256][4];
    float lightPositionRadius[256][4];
    float lightColorIntensity[256][4];
    uint32_t lightCount; float padding[3];
};

constexpr char kShader[] = R"(
cbuffer Frame : register(b0) {
    float3 eye; float tanHalf;
    float3 forward; float aspect;
    float3 right; float nearPlane;
    float3 up; float farPlane;
    float3 lightDir; float ambient;
    float3 skyColor; uint materialCount;
    float3 shadowBase; float shadowSpan;
    float3 shadowRight; float shadowDist;
    float3 shadowUp; float shadowSize;
    float4 materialGlow[256];
    float4 materialSpecular[256];
    float4 materialOpacity[256];
    float4 materialDiffuse[256];
    float4 materialEmission[256];
    float4 lightPositionRadius[256];
    float4 lightColorIntensity[256];
    uint lightCount; float3 padding;
};
Texture2DArray<float4> albedo : register(t0);
Texture2D<float> shadowDepth : register(t1);
Texture2DArray<float> roughnessMap : register(t2);
Texture2DArray<float4> normalMap : register(t3);
SamplerState wrapSampler : register(s0);
struct VSIn { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint mat:MATERIAL; };
struct VSOut { float4 p:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; float3 world:TEXCOORD1; nointerpolation uint mat:MATERIAL; };
VSOut VSMain(VSIn i) {
    VSOut o;
    float3 q = i.p - eye;
    float z = dot(q, forward);
    float safeZ = max(z, nearPlane);
    float nx = dot(q, right) / (safeZ * tanHalf);
    float ny = dot(q, up) * aspect / (safeZ * tanHalf);
    float depth = (farPlane / (farPlane - nearPlane)) * z - (farPlane * nearPlane) / (farPlane - nearPlane);
    // Keep the real camera-space z in w and clip space. Using safeZ here would
    // pull geometry behind the near plane onto the near plane instead of clipping it.
    o.p = float4(nx * safeZ, ny * safeZ, depth, z);
    o.n = i.n; o.uv = i.uv; o.world=i.p; o.mat = min(i.mat, materialCount - 1);
    return o;
}
struct VSBoxIn {
    float3 p:POSITION; float3 n:NORMAL; float2 unusedUv:TEXCOORD0;
    uint unusedMaterial:MATERIAL;
    float3 boxLo:BOXMIN; float uvScale:BOXSCALE;
    float3 boxHi:BOXMAX; uint boxMat:BOXMAT;
    float3 boxSize:BOXSIZE;uint uvMode:BOXUVMODE;
};
VSOut VSBoxMain(VSBoxIn i) {
    // Select the original box corners directly rather than recomputing a
    // matrix: world coordinates and texture mapping match the legacy mesh.
    float3 p=float3(i.p.x<0.5?i.boxLo.x:i.boxHi.x,
                    i.p.y<0.5?i.boxLo.y:i.boxHi.y,
                    i.p.z<0.5?i.boxLo.z:i.boxHi.z);
    float3 n=i.n;
    float2 uv;
    if(i.uvMode==1){
        // Source V16 JSON boxes store per-face local UVs, while native
        // Kind::Box maps the world position. Preserve both conventions.
        float2 localUV=i.unusedUv;
        if(n.z < -0.5 || n.x > 0.5)localUV.x=1.0-localUV.x;
        if(n.y > 0.5)localUV.y=1.0-localUV.y;
        float uLength=(abs(n.x)>0.5?i.boxSize.z:i.boxSize.x)*0.5;
        float vLength=(abs(n.y)>0.5?i.boxSize.z:i.boxSize.y)*0.5;
        uv=localUV*float2(uLength,vLength);
    }else{
        uv=(abs(n.y)>=abs(n.x)&&abs(n.y)>=abs(n.z))
            ?p.xz*i.uvScale
            :(abs(n.x)>=abs(n.z)?p.zy*i.uvScale:p.xy*i.uvScale);
    }
    VSIn raw;
    raw.p=p;raw.n=n;raw.uv=uv;raw.mat=i.boxMat;
    return VSMain(raw);
}
struct VSEllipseIn {
    float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0;
    uint unusedMaterial:MATERIAL;
    float3 center:ELLIPSECENTER;uint ellipseMat:ELLIPSEMAT;
    float3 radii:ELLIPSERADII;float pad:ELLIPSEPAD;
};
VSOut VSEllipseMain(VSEllipseIn i) {
    VSIn raw;
    raw.p=i.center+i.p*i.radii;
    raw.n=normalize(i.n/max(i.radii,float3(.00001,.00001,.00001)));
    raw.uv=i.uv;
    raw.mat=i.ellipseMat;
    return VSMain(raw);
}
float4 PSMain(VSOut i):SV_TARGET {
    // WRAP sampler already handles tiling. frac(uv) here broke automatic
    // derivatives at tile boundaries and forced inappropriate mip levels.
    float3 base = albedo.Sample(wrapSampler, float3(i.uv, i.mat)).rgb;
    float3 normal = normalize(i.n);
    float3 dpdx = ddx(i.world), dpdy = ddy(i.world);
    float2 duvdx = ddx(i.uv), duvdy = ddy(i.uv);
    float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    if (materialOpacity[i.mat].y > 0.5) {
        float3 detailNormal = normalize(normalMap.Sample(wrapSampler, float3(i.uv, i.mat)).xyz * 2.0 - 1.0);
        if (abs(determinant) > 0.000001) {
            float3 tangent = (dpdx * duvdy.y - dpdy * duvdx.y) / determinant;
            float3 rawBitangent = (-dpdx * duvdy.x + dpdy * duvdx.x) / determinant;
            tangent = normalize(tangent - normal * dot(normal, tangent));
            float3 bitangent = normalize(cross(normal, tangent)) * (dot(cross(normal, tangent), rawBitangent) < 0.0 ? -1.0 : 1.0);
            normal = normalize(tangent * detailNormal.x + bitangent * detailNormal.y + normal * detailNormal.z);
        }
    }
    float diffuse = saturate(dot(normal, normalize(lightDir)));
    float3 shadowOffset = i.world - shadowBase;
    float shadowFactor = 1.0;
    float shadowScale = shadowSize / (2.0 * shadowSpan);
    float shadowCenter = shadowSize * 0.5 - 0.5;
    int2 shadowPixel = int2(round(float2(dot(shadowOffset, shadowRight), dot(shadowOffset, shadowUp)) * shadowScale + shadowCenter));
    float shadowDistance = shadowDist - dot(shadowOffset, lightDir);
    if (shadowPixel.x >= 2 && shadowPixel.y >= 2 && shadowPixel.x < int(shadowSize) - 2 && shadowPixel.y < int(shadowSize) - 2) {
        float visible = 0.0;
        [unroll] for (int sy = -1; sy <= 1; ++sy) [unroll] for (int sx = -1; sx <= 1; ++sx) {
            float storedDepth = shadowDepth.Load(int3(shadowPixel + int2(sx, sy), 0));
            visible += shadowDistance <= storedDepth + 0.095 ? 1.0 : 0.0;
        }
        shadowFactor = 0.16 + 0.84 * visible / 9.0;
    }
    float4 diffuseTerms = materialDiffuse[i.mat];
    // Cheap hemispherical sky fill: upward surfaces receive slightly more
    // ambient daylight than vertical walls and undersides. Direct lighting
    // and the existing shadow map remain separate for stronger depth.
    float hemisphere=saturate(normal.y*0.5+0.5);
    float skyFill=lerp(0.86,1.10,hemisphere);
    float light=(diffuseTerms.x + diffuseTerms.y*diffuseTerms.w)*skyFill
                 + diffuseTerms.z*diffuse*shadowFactor*diffuseTerms.w;
    float3 color = base * light + base * materialGlow[i.mat].rgb * materialGlow[i.mat].a;
    color += materialEmission[i.mat].rgb * materialEmission[i.mat].a;
    color += float3(28.0, 13.0, 4.0) / 255.0 * (1.0 - diffuseTerms.w);
    float3 viewDir = normalize(eye - i.world);
    float3 reflectedLight = reflect(-normalize(lightDir), normal);
    float roughness = roughnessMap.Sample(wrapSampler, float3(i.uv, i.mat));
    float specularPower = materialSpecular[i.mat].y;
    if (materialSpecular[i.mat].z > 0.5) {
        specularPower = clamp(2.0 / max(roughness * roughness, 0.0001) - 2.0, 8.0, 80.0);
    }
    float specular = pow(saturate(dot(viewDir, reflectedLight)), specularPower)
                   * materialSpecular[i.mat].x * shadowFactor * diffuseTerms.w;
    // Use PBR metalness for the highlight tint and a cheap 5th-power
    // Schlick grazing response. This avoids another expensive pow() call.
    float metallic=saturate(materialSpecular[i.mat].w);
    float grazing=1.0-saturate(dot(viewDir,normal));
    float f2=grazing*grazing;
    float fresnel=f2*f2*grazing;
    float3 specTint=lerp(float3(178.0,202.0,214.0)/255.0,
                         saturate(base*1.2),metallic);
    color += specTint * specular * (0.88+0.22*fresnel);
    [loop] for (uint l = 0; l < lightCount; ++l) {
        float3 delta = lightPositionRadius[l].xyz - i.world;
        float distanceToLight = length(delta);
        float radius = lightPositionRadius[l].w;
        if (distanceToLight < radius && distanceToLight > 0.0001) {
            float edge = 1.0 - distanceToLight / radius;
            float lambert = saturate(dot(normal, delta / distanceToLight));
            color += lightColorIntensity[l].rgb * (edge * edge * lightColorIntensity[l].a * lambert * 0.27);
        }
    }
    return float4(color, materialOpacity[i.mat].x);
}
)";

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

// A reusable canopy with the same deterministic ripple/leaf construction
// as the V16 bridge. High LOD matches its 18x9 tessellation and 36 leaf
// details; lower LODs only remove geometric detail at a distance.
static std::vector<GpuVertex> makeFoliageTemplate(int segments,int rings,int leaves){
    MeshBuilder mesh;
    const auto point=[&](int ring,int segment){
        const float latitude=float(ring)*PI/rings;
        const float azimuth=float(segment)*2.f*PI/segments;
        const float ripple=1.f+.075f*std::sin(5.f*azimuth+2.f*latitude)
                             +.055f*std::cos(9.f*azimuth-3.f*latitude)
                             +.04f*std::sin(7.f*latitude+4.f*azimuth);
        const float widest=std::sin(latitude)*ripple;
        return V{widest*std::cos(azimuth),std::cos(latitude),
                 widest*std::sin(azimuth)};
    };
    auto add=[&](V p,V q,V r,V uv0,V uv1,V uv2){
        V n=norm(cross(q-p,r-p));
        if(dot(n,p)<0.f){std::swap(q,r);std::swap(uv1,uv2);n=norm(cross(q-p,r-p));}
        mesh.tri(p,q,r,n,n,n,uv0.x,uv0.y,uv1.x,uv1.y,uv2.x,uv2.y,0);
    };
    for(int ring=0;ring<rings;++ring)for(int segment=0;segment<segments;++segment){
        const V a=point(ring,segment),b=point(ring+1,segment);
        const V c=point(ring+1,segment+1),d=point(ring,segment+1);
        if(ring>0)add(a,b,d,
            {float(segment)/segments,float(ring)/rings},
            {float(segment)/segments,float(ring+1)/rings},
            {float(segment+1)/segments,float(ring)/rings});
        if(ring<rings-1)add(b,c,d,
            {float(segment)/segments,float(ring+1)/rings},
            {float(segment+1)/segments,float(ring+1)/rings},
            {float(segment+1)/segments,float(ring)/rings});
    }
    for(int i=0;i<leaves;++i){
        const float azimuth=float(i)*2.39996323f;
        const float elevation=.35f*std::sin(i*1.731f);
        const float cy=std::sin(elevation),radius=std::cos(elevation);
        const V n=norm({radius*std::cos(azimuth),cy,radius*std::sin(azimuth)});
        const V center=n*.93f;
        const V helper=std::abs(n.y)<.8f?V{0,1,0}:V{1,0,0};
        const V tangent=norm(cross(n,helper));
        const float width=(.12f+.035f*float(i%3))*2.f;
        const float height=width*(1.15f+.20f*float(i%4));
        const V base=center-tangent*(width*.45f);
        const V end=center+tangent*(width*.4f)+V{0,height*.24f,0}+n*(height*.24f);
        const V sides=center+V{0,height*.4f,0};
        mesh.tri(base,sides,end,n,n,n,0,0,.5f,1.f,1,0,0);
    }
    return std::move(mesh.vertices);
}
// Consecutive triangle batches are built once from the immutable mesh.
 // Their bounding spheres conservatively reject fully off-screen batches.
 // No geometry is edited or deleted: switch off for exact A/B comparisons.
struct DrawChunk {
    UINT first=0, count=0;
    V mn{}, mx{};
};
static bool visibleChunk(const DrawChunk& chunk,const Camera& camera) {
    const V center=(chunk.mn+chunk.mx)*.5f;
    const float radius=length(chunk.mx-chunk.mn)*.5f + .01f;
    const V q=center-camera.eye;
    const float z=dot(q,camera.f);
    if(z+radius<.08f||z-radius>10000.f)return false;
    const float x=dot(q,camera.r),y=dot(q,camera.u);
    const float h=std::max(.001f,camera.tanHalf);
    const float v=h/std::max(.001f,camera.aspect);
    // Outward-facing frustum plane: sphere support radius is
    // radius * sqrt(1+tanHalf^2). This is conservative on near-plane
    // crossings and cannot cut partially visible geometry.
    if(std::abs(x)>z*h+radius*std::sqrt(1.f+h*h))return false;
    if(std::abs(y)>z*v+radius*std::sqrt(1.f+v*v))return false;
    return true;
}
class DirectX11Preview {
    using Clock=std::chrono::steady_clock;
    struct ReadbackSlot {
        ComPtr<ID3D11Texture2D> staging;
        ComPtr<ID3D11Query> disjoint,start,afterDraw,afterCopy;
        bool queryReady=false;
        int width=0,height=0;
        Clock::time_point cpuStarted{},copyQueued{};
        Stats submitted{};
    };
public:
    bool render(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,
                std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error) {
        // Retain the original exact-image synchronous API. The pipelined API
        // deliberately lives alongside this path and is opt-in in the bench.
        return enqueue(scene,shadows,cam,w,h,1,error)&&receive(pixels,stats,error);
    }
    bool enqueue(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,
                 unsigned depth,std::wstring& error) {
        const auto started=Clock::now();
        if(w<=0||h<=0){error=L"Résolution GPU invalide.";return false;}
        if(depth<1||depth>3){error=L"Profondeur des tampons GPU attendue : 1 à 3.";return false;}
        if(pending_.size()>=depth || (!pending_.empty()&&depth!=readbackDepth_)){
            error=L"Lecture GPU en attente : vider la file avant de changer la profondeur.";return false;
        }
        if(!pending_.empty() && (w!=width_||h!=height_)){
            error=L"Changement de résolution avec des images GPU en attente.";return false;
        }
        const RasterQualitySettings quality=unpackQuality(gpuQualityBits.load(std::memory_order_acquire));
        if(!ensureDevice(error)||!ensureScene(scene,error)||!ensureShadow(shadows,error)||
           !ensureTargets(w,h,quality.msaaSamples,error)||
           !ensureAnisotropy(quality.anisotropy,error)||
           !ensureReadback(depth,w,h,error))return false;
        const unsigned slotIndex=nextReadbackSlot_;
        ReadbackSlot& slot=readbackSlots_[slotIndex];
        slot.cpuStarted=started;
        FrameConstants cb{};
        cb.eye[0]=cam.eye.x;cb.eye[1]=cam.eye.y;cb.eye[2]=cam.eye.z;cb.tanHalf=cam.tanHalf;
        cb.forward[0]=cam.f.x;cb.forward[1]=cam.f.y;cb.forward[2]=cam.f.z;cb.aspect=cam.aspect;
        cb.right[0]=cam.r.x;cb.right[1]=cam.r.y;cb.right[2]=cam.r.z;cb.nearPlane=.08f;
        cb.up[0]=cam.u.x;cb.up[1]=cam.u.y;cb.up[2]=cam.u.z;cb.farPlane=10000.f;
        cb.light[0]=sun.x;cb.light[1]=sun.y;cb.light[2]=sun.z;cb.ambient=.66f;
        V bg=scene.underwaterEnvironment?V{7,27,39}:skyHorizon*.65f+skyZenith*.35f;
        cb.sky[0]=bg.x/255.f;cb.sky[1]=bg.y/255.f;cb.sky[2]=bg.z/255.f;cb.materialCount=uint32_t(std::max<size_t>(1,scene.mat.size()));
        cb.shadowBase[0]=shadows.base.x;cb.shadowBase[1]=shadows.base.y;cb.shadowBase[2]=shadows.base.z;cb.shadowSpan=shadows.span;
        cb.shadowRight[0]=shadows.right.x;cb.shadowRight[1]=shadows.right.y;cb.shadowRight[2]=shadows.right.z;cb.shadowDist=shadows.dist;
        cb.shadowUp[0]=shadows.up.x;cb.shadowUp[1]=shadows.up.y;cb.shadowUp[2]=shadows.up.z;cb.shadowSize=float(shadows.size);
        for(size_t m=0;m<scene.mat.size()&&m<256;m++){
            const std::string& name=scene.mat[m].name;
            const bool carPaint=name.rfind("sports",0)==0||name=="carSilver"||name=="carBlue"||name=="carGreen"||name=="classicBody";
            const bool raptureSurface=name=="raptureStone"||name=="raptureBronze"||name=="raptureGlass";
            float r=1.f,g=1.f,b=1.f,strength=0.f;
            float spec=0.f,power=34.f,sr=178.f/255.f,sg=202.f/255.f;
            float emissionR=0.f,emissionG=0.f,emissionB=0.f,emissionStrength=0.f;
            // Lift the general environment fill without flattening direct sun.
            // The previous .21/.34/.50 mix left shaded concrete too dark.
            float diffuseBase=.30f,diffuseDay=.44f,diffuseDirect=.56f;
            if(name=="foam"){diffuseBase=.60f;diffuseDay=.30f;diffuseDirect=.24f;}
            else if(name=="orange"){diffuseBase=.34f;diffuseDay=.43f;diffuseDirect=.26f;}
            else if(carPaint){diffuseBase=.32f;diffuseDay=.42f;diffuseDirect=.32f;}
            else if(raptureSurface){diffuseBase=.42f;diffuseDay=.28f;diffuseDirect=.50f;}
            if(name=="lamp")strength=.45f+1.05f*lampFactor;
            else if(name=="led")strength=1.35f*lampFactor;
            else if(name=="taillight")strength=1.05f*lampFactor;
            else if(name=="neon")strength=1.1f;
            else if(name=="windowlit")strength=.88f;
            if(name=="lamp"){emissionR=1.f;emissionG=166.f/255.f;emissionB=91.f/255.f;emissionStrength=.75f+1.9f*lampFactor;}
            else if(name=="led"){emissionR=1.f;emissionG=217.f/255.f;emissionB=165.f/255.f;emissionStrength=.48f+.85f*lampFactor;}
            else if(name=="taillight"){emissionR=1.f;emissionG=28.f/255.f;emissionB=12.f/255.f;emissionStrength=1.25f*lampFactor;}
            if(name=="sea"){spec=.46f;power=25.f;}
            else if(name=="puddle"){spec=.64f;power=18.f;}
            else if(name=="glass"||name=="raptureGlass"){spec=.27f;power=34.f;}
            else if(name=="raptureBronze"){spec=.20f;power=34.f;}
            else if(name=="metal"||name=="seatSteel"){spec=.24f;power=34.f;}
            else if(name=="wheelMetal"){spec=.34f;power=42.f;}
            else if(name=="carSilver"){spec=.94f;power=52.f;}
            else if(carPaint){spec=.68f;power=30.f;}
            else if(name=="carGlass"){spec=.52f;power=38.f;}
            else if(name=="taxi"){spec=.20f;power=34.f;}
            else if(name=="mutant"){spec=.23f;power=29.f;}
            else if(name=="scar"){spec=.28f;power=35.f;}
            else if(name=="wound"){spec=.19f;power=20.f;}
            else if(name=="asphalt"){spec=.16f;power=13.f;}
            else if(name=="orange"){spec=.13f;power=42.f;}
            // The Pripyat material IDs carry generic PBR parameters rather
            // than the old hardcoded names (metal, puddle, glass). Honor
            // roughness/metalness/transmission even for unseen material IDs.
            const float metal=clampf(scene.mat[m].metallic,0.f,1.f);
            const float surfaceRough=clampf(scene.mat[m].roughnessScale,.08f,1.f);
            const float transmission=float(scene.mat[m].transmissionPercent)/100.f;
            spec=std::max(spec,.025f+.29f*metal+.095f*(1.f-surfaceRough)
                          +.14f*transmission);
            if(scene.mat[m].roughnessMip.empty())
                power=clampf(2.f/(surfaceRough*surfaceRough)-2.f,8.f,80.f);
            cb.materialGlow[m][0]=r;cb.materialGlow[m][1]=g;cb.materialGlow[m][2]=b;cb.materialGlow[m][3]=strength;
            cb.materialSpecular[m][0]=spec;cb.materialSpecular[m][1]=power;cb.materialSpecular[m][2]=scene.mat[m].roughnessMip.empty()?0.f:1.f;cb.materialSpecular[m][3]=metal;
            cb.materialOpacity[m][0]=1.f-float(scene.mat[m].transmissionPercent)/100.f;
            cb.materialOpacity[m][1]=scene.mat[m].normalMip.empty()?0.f:1.f;
            cb.materialDiffuse[m][0]=diffuseBase;cb.materialDiffuse[m][1]=diffuseDay;cb.materialDiffuse[m][2]=diffuseDirect;cb.materialDiffuse[m][3]=daylight;
            cb.materialEmission[m][0]=emissionR;cb.materialEmission[m][1]=emissionG;cb.materialEmission[m][2]=emissionB;cb.materialEmission[m][3]=emissionStrength;
        }
        const float sceneLightScale=.42f+.58f*(1.f-daylight);
        auto appendLight=[&](const Light& l,float scale){
            if(cb.lightCount>=256)return;
            const uint32_t i=cb.lightCount++;
            cb.lightPositionRadius[i][0]=l.p.x;cb.lightPositionRadius[i][1]=l.p.y;cb.lightPositionRadius[i][2]=l.p.z;cb.lightPositionRadius[i][3]=l.radius;
            cb.lightColorIntensity[i][0]=l.color.x/255.f;cb.lightColorIntensity[i][1]=l.color.y/255.f;cb.lightColorIntensity[i][2]=l.color.z/255.f;cb.lightColorIntensity[i][3]=l.intensity*scale;
        };
        for(const Light& l:pointLights)appendLight(l,1.f);
        for(const Light& l:scene.lights)appendLight(l,sceneLightScale);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr=context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        if(FAILED(hr)){error=L"Impossible de transmettre les paramètres à DirectX 11.";return false;}
        std::memcpy(mapped.pData,&cb,sizeof(cb));context_->Unmap(constants_.Get(),0);
        const float clear[4]={cb.sky[0],cb.sky[1],cb.sky[2],1.f};
        if(slot.queryReady){
            context_->Begin(slot.disjoint.Get());
            context_->End(slot.start.Get());
        }
        context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),dsv_.Get());context_->ClearRenderTargetView(rtv_.Get(),clear);context_->ClearDepthStencilView(dsv_.Get(),D3D11_CLEAR_DEPTH,1.f,0);
        D3D11_VIEWPORT vp{};vp.Width=float(w);vp.Height=float(h);vp.MinDepth=0;vp.MaxDepth=1;context_->RSSetViewports(1,&vp);
        UINT stride=sizeof(GpuVertex),offset=0;ID3D11Buffer* vb=vertices_.Get();context_->IASetInputLayout(layout_.Get());context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->RSSetState(raster_.Get());context_->OMSetDepthStencilState(depth_.Get(),0);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
        ID3D11Buffer* cbuff=constants_.Get();context_->VSSetConstantBuffers(0,1,&cbuff);context_->PSSetConstantBuffers(0,1,&cbuff);
        ID3D11ShaderResourceView* srvs[]={albedo_.Get(),shadowSrv_.Get(),roughness_.Get(),normal_.Get()};context_->PSSetShaderResources(0,4,srvs);ID3D11SamplerState* sampler=sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
        UINT submittedVertices=0;
        uint64_t drawCalls=0;
        const char* noCull=std::getenv("M3D_V17_DISABLE_FRUSTUM_CULLING");
        const bool cullEnabled=quality.frustumCulling
            &&!(noCull && std::strcmp(noCull,"1")==0);
        if(vertexCount_){
            if(!cullEnabled){
                context_->Draw(vertexCount_,0);
                ++drawCalls;
                submittedVertices=vertexCount_;
            }else{
                // Merge adjacent visible batches to avoid thousands of draw
                // calls. A scene with every batch visible stays one Draw().
                UINT start=0,count=0;
                auto flush=[&]{
                    if(!count)return;
                    context_->Draw(count,start);
                    ++drawCalls;
                    submittedVertices+=count;
                    count=0;
                };
                for(const DrawChunk& chunk:chunks_){
                    if(!visibleChunk(chunk,cam)){flush();continue;}
                    if(count && start+count!=chunk.first)flush();
                    if(!count)start=chunk.first;
                    count+=chunk.count;
                }
                flush();
            }
        }
        UINT submittedBoxInstances=0;
        if(boxInstanceCount_){
            ID3D11Buffer* buffers[]={boxVertices_.Get(),boxInstances_.Get()};
            UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuBoxInstance))};
            UINT offsets[]={0,0};
            context_->IASetInputLayout(boxLayout_.Get());
            context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
            context_->VSSetShader(boxVs_.Get(),nullptr,0);
            auto drawBoxes=[&](UINT first,UINT count){
                if(!count)return;
                context_->DrawInstanced(36,count,0,first);
                ++drawCalls;
                submittedBoxInstances+=count;
            };
            if(!cullEnabled){
                drawBoxes(0,boxInstanceCount_);
            }else{
                UINT first=0,count=0;
                auto flush=[&]{drawBoxes(first,count);count=0;};
                for(const DrawChunk& chunk:boxChunks_){
                    if(!visibleChunk(chunk,cam)){flush();continue;}
                    if(count && first+count!=chunk.first)flush();
                    if(!count)first=chunk.first;
                    count+=chunk.count;
                }
                flush();
            }
            context_->IASetInputLayout(layout_.Get());
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        uint64_t submittedEllipseTriangles=0;
        uint64_t submittedEllipseInstances=0;
        if(ellipseInstanceCount_){
            std::array<std::vector<GpuEllipseInstance>,3> level;
            for(auto& bucket:level)bucket.reserve(ellipseInstanceCount_/3+1);
            const char* disableLod=std::getenv("M3D_V17_DISABLE_LOD");
            const bool lodEnabled=quality.dynamicLod
                &&!(disableLod && std::strcmp(disableLod,"1")==0);
            for(const auto& sphere:ellipses_){
                const V center{sphere.center[0],sphere.center[1],sphere.center[2]};
                const float radius=std::max({sphere.radii[0],sphere.radii[1],sphere.radii[2]});
                const DrawChunk bounds{0,0,{center.x-radius,center.y-radius,center.z-radius},
                                           {center.x+radius,center.y+radius,center.z+radius}};
                if(cullEnabled&&!visibleChunk(bounds,cam))continue;
                const float z=std::max(.10f,dot(center-cam.eye,cam.f));
                const float projectedRadius=radius*float(w)/(2.f*std::max(.01f,cam.tanHalf)*z);
                const float lodScale=100.f/float(quality.lodQualityPercent);
                const int lod=(!lodEnabled||projectedRadius>=48.f*lodScale)?2
                    :(projectedRadius>=14.f*lodScale?1:0);
                level[lod].push_back(sphere);
            }
            // One small streaming allocation/update for all visible
            // ellipsoids, not a per-object Draw() or a CPU rebuild.
            D3D11_MAPPED_SUBRESOURCE ellipseMap{};
            hr=context_->Map(ellipseInstances_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&ellipseMap);
            if(FAILED(hr)){error=L"Mise à jour des LOD ellipsoïdes impossible.";return false;}
            GpuEllipseInstance* dest=static_cast<GpuEllipseInstance*>(ellipseMap.pData);
            for(const auto& bucket:level){
                if(bucket.empty())continue;
                std::memcpy(dest,bucket.data(),bucket.size()*sizeof(GpuEllipseInstance));
                dest+=bucket.size();
            }
            context_->Unmap(ellipseInstances_.Get(),0);
            context_->IASetInputLayout(ellipseLayout_.Get());
            context_->VSSetShader(ellipseVs_.Get(),nullptr,0);
            UINT first=0;
            for(size_t lod=0;lod<level.size();++lod){
                const UINT count=UINT(level[lod].size());
                if(!count)continue;
                ID3D11Buffer* buffers[]={ellipseVertices_[lod].Get(),ellipseInstances_.Get()};
                UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuEllipseInstance))};
                UINT offsets[]={0,0};
                context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
                context_->DrawInstanced(ellipseVertexCounts_[lod],count,0,first);
                ++drawCalls;
                submittedEllipseInstances+=count;
                submittedEllipseTriangles+=uint64_t(ellipseVertexCounts_[lod]/3)*count;
                first+=count;
            }
            context_->IASetInputLayout(layout_.Get());
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        uint64_t submittedFoliageTriangles=0;
        uint64_t submittedFoliageInstances=0;
        if(foliageInstanceCount_){
            std::array<std::vector<GpuEllipseInstance>,3> buckets;
            for(auto& bucket:buckets)bucket.reserve(foliageInstanceCount_/3+1);
            const char* disabled=std::getenv("M3D_V17_DISABLE_LOD");
            const bool lodEnabled=quality.dynamicLod
                &&!(disabled&&std::strcmp(disabled,"1")==0);
            for(const auto& plant:foliage_){
                const V center{plant.center[0],plant.center[1],plant.center[2]};
                const float radius=std::max({plant.radii[0],plant.radii[1],plant.radii[2]});
                const DrawChunk bounds{0,0,{center.x-radius,center.y-radius,center.z-radius},
                    {center.x+radius,center.y+radius,center.z+radius}};
                if(cullEnabled&&!visibleChunk(bounds,cam))continue;
                const float z=std::max(.10f,dot(center-cam.eye,cam.f));
                const float projectedRadius=radius*float(w)/(2.f*std::max(.01f,cam.tanHalf)*z);
                const float lodScale=100.f/float(quality.lodQualityPercent);
                const int lod=(!lodEnabled||projectedRadius>=40.f*lodScale)?2
                    :(projectedRadius>=12.f*lodScale?1:0);
                buckets[lod].push_back(plant);
            }
            D3D11_MAPPED_SUBRESOURCE foliageMap{};
            hr=context_->Map(foliageInstances_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&foliageMap);
            if(FAILED(hr)){error=L"Mise à jour des LOD du feuillage impossible.";return false;}
            GpuEllipseInstance* dest=static_cast<GpuEllipseInstance*>(foliageMap.pData);
            for(const auto& bucket:buckets){
                if(bucket.empty())continue;
                std::memcpy(dest,bucket.data(),bucket.size()*sizeof(GpuEllipseInstance));
                dest+=bucket.size();
            }
            context_->Unmap(foliageInstances_.Get(),0);
            context_->IASetInputLayout(ellipseLayout_.Get());
            context_->VSSetShader(ellipseVs_.Get(),nullptr,0);
            UINT first=0;
            for(size_t lod=0;lod<buckets.size();++lod){
                const UINT count=UINT(buckets[lod].size());
                if(!count)continue;
                ID3D11Buffer* buffers[]={foliageVertices_[lod].Get(),foliageInstances_.Get()};
                UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuEllipseInstance))};
                UINT offsets[]={0,0};
                context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
                context_->DrawInstanced(foliageVertexCounts_[lod],count,0,first);
                ++drawCalls;
                submittedFoliageTriangles+=uint64_t(foliageVertexCounts_[lod]/3)*count;
                submittedFoliageInstances+=count;
                first+=count;
            }
            context_->IASetInputLayout(layout_.Get());
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        if(!transparent_.empty()){
            std::sort(transparent_.begin(),transparent_.end(),[&](const TransparentTriangle& a,const TransparentTriangle& b){return dot(a.center-cam.eye,cam.f)>dot(b.center-cam.eye,cam.f);});
            D3D11_MAPPED_SUBRESOURCE transparentMap{};hr=context_->Map(transparentVertices_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&transparentMap);
            if(FAILED(hr)){error=L"Impossible de préparer les surfaces transparentes DirectX 11.";return false;}
            auto* out=static_cast<GpuVertex*>(transparentMap.pData);for(const auto& t:transparent_)for(const auto& v:t.v)*out++=v;context_->Unmap(transparentVertices_.Get(),0);
            UINT transparentStride=sizeof(GpuVertex);ID3D11Buffer* transparentBuffer=transparentVertices_.Get();context_->IASetVertexBuffers(0,1,&transparentBuffer,&transparentStride,&offset);
            context_->OMSetDepthStencilState(transparentDepth_.Get(),0);context_->OMSetBlendState(blend_.Get(),nullptr,0xffffffffu);context_->Draw(UINT(transparent_.size()*3),0);++drawCalls;
            context_->OMSetBlendState(nullptr,nullptr,0xffffffffu);context_->OMSetDepthStencilState(depth_.Get(),0);context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        // GPU timestamps surround the actual draw commands and the queued
        // GPU copy. They do not include any CPU-side GetData/Map blocking.
        if(slot.queryReady)context_->End(slot.afterDraw.Get());
        if(sampleCount_>1)context_->ResolveSubresource(resolve_.Get(),0,target_.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
        else context_->CopyResource(resolve_.Get(),target_.Get());
        context_->CopyResource(slot.staging.Get(),resolve_.Get());
        if(slot.queryReady){
            context_->End(slot.afterCopy.Get());
            context_->End(slot.disjoint.Get());
        }
        slot.submitted={};
        slot.submitted.gpu=true;slot.submitted.threads=0;
        slot.submitted.readbackBufferCount=depth;
        slot.submitted.gpuDrawCalls=drawCalls;
        slot.submitted.gpuBoxInstances=submittedBoxInstances;
        slot.submitted.gpuEllipseInstances=submittedEllipseInstances;
        slot.submitted.gpuFoliageInstances=submittedFoliageInstances;
        slot.submitted.tests=submittedVertices/3+uint64_t(submittedBoxInstances)*12
            +submittedEllipseTriangles+submittedFoliageTriangles+transparent_.size();
        slot.copyQueued=Clock::now();
        slot.submitted.cpuSubmitMs=std::chrono::duration<double,std::milli>(
            slot.copyQueued-started).count();
        pending_.push_back(slotIndex);
        nextReadbackSlot_=(slotIndex+1u)%readbackDepth_;
        return true;
    }
    bool receive(std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
        if(pending_.empty()){error=L"Aucune image GPU en attente.";return false;}
        ReadbackSlot& slot=readbackSlots_[pending_.front()];
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto mapStarted=Clock::now();
        const HRESULT hr=context_->Map(slot.staging.Get(),0,D3D11_MAP_READ,0,&mapped);
        if(FAILED(hr)){error=L"Lecture différée DirectX 11 impossible.";return false;}
        const auto mapDone=Clock::now();
        pixels.resize(size_t(slot.width)*size_t(slot.height));
        for(int y=0;y<slot.height;++y){
            const uint8_t* row=static_cast<const uint8_t*>(mapped.pData)
                +size_t(y)*mapped.RowPitch;
            for(int x=0;x<slot.width;++x){
                const uint8_t* p=row+size_t(x)*4;
                pixels[size_t(y)*slot.width+x]=0xff000000u
                    |(uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|uint32_t(p[2]);
            }
        }
        context_->Unmap(slot.staging.Get(),0);
        const auto finished=Clock::now();
        stats=slot.submitted;
        stats.ms=std::chrono::duration<double,std::milli>(finished-slot.cpuStarted).count();
        stats.postMs=std::chrono::duration<double,std::milli>(finished-slot.copyQueued).count();
        stats.cpuMapWaitMs=std::chrono::duration<double,std::milli>(mapDone-mapStarted).count();
        stats.cpuPixelConvertMs=std::chrono::duration<double,std::milli>(finished-mapDone).count();
        if(slot.queryReady){
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info{};
            UINT64 begin=0,draw=0,copy=0;
            const bool valid=context_->GetData(slot.disjoint.Get(),&info,sizeof(info),0)==S_OK
                &&context_->GetData(slot.start.Get(),&begin,sizeof(begin),0)==S_OK
                &&context_->GetData(slot.afterDraw.Get(),&draw,sizeof(draw),0)==S_OK
                &&context_->GetData(slot.afterCopy.Get(),&copy,sizeof(copy),0)==S_OK;
            if(valid&&!info.Disjoint&&info.Frequency>0&&begin<=draw&&draw<=copy){
                const double ticksToMs=1000.0/double(info.Frequency);
                stats.gpuDrawMs=double(draw-begin)*ticksToMs;
                stats.gpuTransferMs=double(copy-draw)*ticksToMs;
                stats.gpuTotalMs=double(copy-begin)*ticksToMs;
                stats.gpuTimingValid=true;
            }
        }
        pending_.pop_front();
        return true;
    }
private:
    bool ensureShadow(const ShadowMap& shadows,std::wstring& error) {
        if(shadows.size<=0||shadows.d.size()<size_t(shadows.size)*shadows.size){error=L"Carte d’ombres invalide pour DirectX 11.";return false;}
        if(!shadowTexture_||shadowSize_!=shadows.size){
            shadowTexture_.Reset();shadowSrv_.Reset();
            D3D11_TEXTURE2D_DESC td{};td.Width=UINT(shadows.size);td.Height=UINT(shadows.size);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R32_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            if(FAILED(device_->CreateTexture2D(&td,nullptr,&shadowTexture_))||FAILED(device_->CreateShaderResourceView(shadowTexture_.Get(),nullptr,&shadowSrv_))){error=L"Création de la carte d’ombres DirectX 11 impossible.";return false;}
            shadowSize_=shadows.size;
        }
        context_->UpdateSubresource(shadowTexture_.Get(),0,nullptr,shadows.d.data(),UINT(shadows.size*sizeof(float)),0);return true;
    }
    bool ensureDevice(std::wstring& error) {
        if(device_)return true;
        D3D_FEATURE_LEVEL feature{};
        // Hardware rendering remains the default. CI can explicitly use the
        // Microsoft WARP software renderer to test this raster backend.
        const char* warp=std::getenv("M3D_V16_FORCE_WARP");
        const D3D_DRIVER_TYPE driver=(warp&&std::strcmp(warp,"1")==0)
            ?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
        HRESULT hr=D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,&feature,&context_);
        if(FAILED(hr)){error=L"Aucun périphérique Direct3D 11 matériel n’est disponible.";return false;}
        ComPtr<ID3DBlob> vsCode,boxVsCode,ellipseVsCode,psCode,errors;
        auto shaderError=[&](const wchar_t* prefix){
            std::wstring result(prefix);
            if(errors&&errors->GetBufferPointer()){
                result+=L": ";
                const char* p=static_cast<const char*>(errors->GetBufferPointer());
                const size_t n=std::min<size_t>(errors->GetBufferSize(),1024);
                for(size_t i=0;i<n;++i)
                    result.push_back(p[i]>=32&&p[i]<127?wchar_t(p[i]):L' ');
            }
            return result;
        };
        hr=D3DCompile(kShader,sizeof(kShader)-1,"m3d_directx11_preview",nullptr,nullptr,"VSMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du vertex shader DirectX 11 impossible");return false;}
        hr=D3DCompile(kShader,sizeof(kShader)-1,"m3d_directx11_preview",nullptr,nullptr,"VSBoxMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&boxVsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du shader d'instancing V17 impossible");return false;}
        hr=D3DCompile(kShader,sizeof(kShader)-1,"m3d_directx11_preview",nullptr,nullptr,
                      "VSEllipseMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ellipseVsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du shader LOD ellipsoïdes impossible");return false;}
        hr=D3DCompile(kShader,sizeof(kShader)-1,"m3d_directx11_preview",nullptr,nullptr,"PSMain","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du pixel shader DirectX 11 impossible");return false;}
        if(FAILED(device_->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,&vs_))||FAILED(device_->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps_))){error=L"Création des shaders DirectX 11 impossible.";return false;}
        const D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertex,p),D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertex,n),D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(GpuVertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},{"MATERIAL",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertex,material),D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(device_->CreateInputLayout(input,4,vsCode->GetBufferPointer(),vsCode->GetBufferSize(),&layout_))){error=L"Création du format de sommet DirectX 11 impossible.";return false;}
        if(FAILED(device_->CreateVertexShader(boxVsCode->GetBufferPointer(),boxVsCode->GetBufferSize(),nullptr,&boxVs_))){
            error=L"Création du vertex shader d'instancing V17 impossible.";return false;
        }
        const D3D11_INPUT_ELEMENT_DESC boxInput[]={
            input[0],input[1],input[2],input[3],
            {"BOXMIN",0,DXGI_FORMAT_R32G32B32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXSCALE",0,DXGI_FORMAT_R32_FLOAT,1,12,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXMAX",0,DXGI_FORMAT_R32G32B32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXMAT",0,DXGI_FORMAT_R32_UINT,1,28,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXSIZE",0,DXGI_FORMAT_R32G32B32_FLOAT,1,32,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXUVMODE",0,DXGI_FORMAT_R32_UINT,1,44,D3D11_INPUT_PER_INSTANCE_DATA,1}
        };
        const HRESULT boxLayoutResult=device_->CreateInputLayout(
            boxInput,10,boxVsCode->GetBufferPointer(),boxVsCode->GetBufferSize(),&boxLayout_);
        if(FAILED(boxLayoutResult)){
            error=L"Création du format des instances GPU impossible. HRESULT "
                  +std::to_wstring(uint32_t(boxLayoutResult));return false;
        }
        if(FAILED(device_->CreateVertexShader(ellipseVsCode->GetBufferPointer(),
            ellipseVsCode->GetBufferSize(),nullptr,&ellipseVs_))){
            error=L"Création du shader GPU LOD impossible.";return false;
        }
        const D3D11_INPUT_ELEMENT_DESC ellipseInput[]={
            input[0],input[1],input[2],input[3],
            {"ELLIPSECENTER",0,DXGI_FORMAT_R32G32B32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"ELLIPSEMAT",0,DXGI_FORMAT_R32_UINT,1,12,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"ELLIPSERADII",0,DXGI_FORMAT_R32G32B32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"ELLIPSEPAD",0,DXGI_FORMAT_R32_FLOAT,1,28,D3D11_INPUT_PER_INSTANCE_DATA,1}
        };
        const HRESULT ellipseLayoutResult=device_->CreateInputLayout(
            ellipseInput,8,ellipseVsCode->GetBufferPointer(),
            ellipseVsCode->GetBufferSize(),&ellipseLayout_);
        if(FAILED(ellipseLayoutResult)){
            error=L"Format des instances ellipsoïdes incorrect. HRESULT "
                  +std::to_wstring(uint32_t(ellipseLayoutResult));return false;
        }
        D3D11_BUFFER_DESC cbd{};cbd.ByteWidth=sizeof(FrameConstants);cbd.Usage=D3D11_USAGE_DYNAMIC;cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cbd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(device_->CreateBuffer(&cbd,nullptr,&constants_))){error=L"Création du buffer de constantes DirectX 11 impossible.";return false;}
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.MultisampleEnable=TRUE;
        if(FAILED(device_->CreateRasterizerState(&rd,&raster_))){error=L"Création de l’état raster DirectX 11 impossible.";return false;}
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        if(FAILED(device_->CreateDepthStencilState(&dd,&depth_))){error=L"Création du depth buffer DirectX 11 impossible.";return false;}
        dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
        if(FAILED(device_->CreateDepthStencilState(&dd,&transparentDepth_))){error=L"Création de l’état de profondeur transparent DirectX 11 impossible.";return false;}
        D3D11_BLEND_DESC blendDesc{};blendDesc.RenderTarget[0].BlendEnable=TRUE;blendDesc.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;blendDesc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;blendDesc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;blendDesc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;blendDesc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;blendDesc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;blendDesc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        if(FAILED(device_->CreateBlendState(&blendDesc,&blend_))){error=L"Création de l’état de transparence DirectX 11 impossible.";return false;}
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_ANISOTROPIC;sd.MaxAnisotropy=16;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device_->CreateSamplerState(&sd,&sampler_))){error=L"Création du sampler DirectX 11 impossible.";return false;}return true;
    }
    bool ensureScene(const Scene& scene,std::wstring& error) {
        const char* disableInstancing=std::getenv("M3D_V17_DISABLE_GPU_INSTANCING");
        const bool useInstancing=!(disableInstancing && std::strcmp(disableInstancing,"1")==0);
        const char* disableBoxes=std::getenv("M3D_V17_DISABLE_BOX_INSTANCING");
        const char* disableEllipses=std::getenv("M3D_V17_DISABLE_ELLIPSE_INSTANCING");
        const bool useBoxInstancing=useInstancing &&
            !(disableBoxes && std::strcmp(disableBoxes,"1")==0);
        const bool useEllipseInstancing=useInstancing &&
            !(disableEllipses && std::strcmp(disableEllipses,"1")==0);
        const char* disableFoliage=std::getenv("M3D_V17_DISABLE_FOLIAGE_INSTANCING");
        const bool useFoliageInstancing=useInstancing &&
            !(disableFoliage && std::strcmp(disableFoliage,"1")==0);
        if(sceneKey_==scene.obj.data()&&objectCount_==scene.obj.size()&&
           (vertices_||boxInstances_||ellipseInstances_) &&
           boxInstancingEnabled_==useBoxInstancing &&
           ellipseInstancingEnabled_==useEllipseInstancing &&
           foliageInstancingEnabled_==useFoliageInstancing &&
           sceneIdentity_==scene.gpuCacheIdentity &&
           sceneRevision_==scene.gpuCacheRevision)return true;
        // An edited scene may retain the same std::vector address: revision
        // tracking is required to refresh its GPU PBR/mesh resources.
        vertices_.Reset();transparentVertices_.Reset();albedo_.Reset();
        boxVertices_.Reset();boxInstances_.Reset();boxInstanceCount_=0;
        roughness_.Reset();normal_.Reset();transparent_.clear();vertexCount_=0;
        chunks_.clear();boxChunks_.clear();
        ellipseInstances_.Reset();
        for(auto& buffer:ellipseVertices_)buffer.Reset();
        ellipseVertexCounts_.fill(0);ellipses_.clear();ellipseInstanceCount_=0;
        foliageInstances_.Reset();
        for(auto& buffer:foliageVertices_)buffer.Reset();
        foliageVertexCounts_.fill(0);foliage_.clear();foliageInstanceCount_=0;
        MeshBuilder mesh;
        std::vector<GpuBoxInstance> boxData;
        // V16 JSON geometry is triangulated for CPU/Compute, but the source
        // builder now retains the exact 12-triangle ranges of unrotated
        // boxes. Skip those triangles only in the raster vertex buffer and
        // submit their identical faces through GPU box instancing instead.
        std::vector<uint8_t> skipSource;
        if((useBoxInstancing && !scene.sourceBoxes.empty()) ||
           (useFoliageInstancing && !scene.sourceFoliage.empty()))
            skipSource.assign(scene.obj.size(),0);
        if(useBoxInstancing && !scene.sourceBoxes.empty()){
            for(const SourceBox& source:scene.sourceBoxes){
                if(source.triangleCount!=12||source.material>=scene.mat.size()||
                   scene.mat[source.material].transmissionPercent>0||
                   size_t(source.firstTriangle)+12>scene.obj.size())continue;
                bool valid=true;
                for(size_t k=source.firstTriangle;k<size_t(source.firstTriangle)+12;++k){
                    const Obj& o=scene.obj[k];
                    if(o.kind!=Kind::Triangle||o.mat!=source.material||skipSource[k]){
                        valid=false;break;
                    }
                }
                if(!valid)continue;
                GpuBoxInstance box{};
                box.lo[0]=source.mn.x;box.lo[1]=source.mn.y;box.lo[2]=source.mn.z;
                box.hi[0]=source.mx.x;box.hi[1]=source.mx.y;box.hi[2]=source.mx.z;
                box.size[0]=source.mx.x-source.mn.x;
                box.size[1]=source.mx.y-source.mn.y;
                box.size[2]=source.mx.z-source.mn.z;
                box.material=source.material;
                box.uvMode=1;
                boxData.push_back(box);
                for(size_t k=source.firstTriangle;k<size_t(source.firstTriangle)+12;++k)
                    skipSource[k]=1;
            }
        }
        if(useFoliageInstancing && !scene.sourceFoliage.empty()){
            for(const SourceFoliage& source:scene.sourceFoliage){
                const size_t begin=source.firstTriangle,end=begin+source.triangleCount;
                if(!source.triangleCount||end>scene.obj.size()||
                    source.material>=scene.mat.size()||
                    scene.mat[source.material].transmissionPercent>0||
                    source.radii.x<=0||source.radii.y<=0||source.radii.z<=0)continue;
                bool valid=true;
                for(size_t k=begin;k<end;++k){
                    if(skipSource[k]||scene.obj[k].kind!=Kind::Triangle||
                       scene.obj[k].mat!=source.material){valid=false;break;}
                }
                if(!valid)continue;
                GpuEllipseInstance canopy{};
                canopy.center[0]=source.center.x;
                canopy.center[1]=source.center.y;
                canopy.center[2]=source.center.z;
                canopy.radii[0]=source.radii.x;
                canopy.radii[1]=source.radii.y;
                canopy.radii[2]=source.radii.z;
                canopy.material=source.material;
                foliage_.push_back(canopy);
                for(size_t k=begin;k<end;++k)skipSource[k]=1;
            }
        }
        for(size_t objectIndex=0;objectIndex<scene.obj.size();++objectIndex){
            if(!skipSource.empty() && skipSource[objectIndex])continue;
            const Obj& o=scene.obj[objectIndex];
            const uint32_t m=o.mat;
            const float uvScale=uvScaleForMaterial(scene,m);
            mesh.transparentMaterial=m<scene.mat.size()&&scene.mat[m].transmissionPercent>0;
            if(o.kind==Kind::Triangle)mesh.tri(o.a,o.b,o.c,o.n0,o.n1,o.n2,o.uv0.x,o.uv0.y,o.uv1.x,o.uv1.y,o.uv2.x,o.uv2.y,m);
            else if(o.kind==Kind::Box){
                if(useBoxInstancing && !mesh.transparentMaterial){
                    GpuBoxInstance instance{};
                    instance.lo[0]=o.mn.x;instance.lo[1]=o.mn.y;instance.lo[2]=o.mn.z;
                    instance.hi[0]=o.mx.x;instance.hi[1]=o.mx.y;instance.hi[2]=o.mx.z;
                    instance.uvScale=uvScale;instance.material=m;
                    instance.size[0]=o.mx.x-o.mn.x;
                    instance.size[1]=o.mx.y-o.mn.y;
                    instance.size[2]=o.mx.z-o.mn.z;
                    instance.uvMode=0;
                    boxData.push_back(instance);
                }else mesh.box(o.mn,o.mx,m,uvScale);
            }
            else if(o.kind==Kind::RoundBox)mesh.roundedBox(o.mn,o.mx,o.r,m,uvScale);
            else if(o.kind==Kind::Ellipse){
                if(useEllipseInstancing && !mesh.transparentMaterial
                   && o.b.x>0.f && o.b.y>0.f && o.b.z>0.f){
                    GpuEllipseInstance instance{};
                    instance.center[0]=o.a.x;instance.center[1]=o.a.y;instance.center[2]=o.a.z;
                    instance.radii[0]=o.b.x;instance.radii[1]=o.b.y;instance.radii[2]=o.b.z;
                    instance.material=m;
                    ellipses_.push_back(instance);
                }else mesh.ellipsoid(o.a,o.b,m);
            }
            else if(o.kind==Kind::Capsule)mesh.capsule(o.a,o.b,o.r,m);
            else if(o.kind==Kind::Cone)mesh.frustum(o.a,o.n0,o.len,o.r,o.r1,m);
            mesh.transparentMaterial=false;
            if(mesh.vertices.size()+mesh.transparent.size()*3>12000000u){error=L"Scène trop grande pour l’aperçu DirectX 11 (limite de sécurité mémoire).";return false;}
        }
        mesh.ground(scene);
        if(mesh.vertices.size()+mesh.transparent.size()*3>12000000u){error=L"Scène trop grande pour l’aperçu DirectX 11 (limite de sécurité mémoire).";return false;}
        if(mesh.vertices.empty()&&mesh.transparent.empty()&&boxData.empty()&&ellipses_.empty()&&foliage_.empty()){
            error=L"La scène ne contient aucune géométrie affichable par DirectX 11.";return false;
        }
        if(!boxData.empty()){
            // Reuse the exact 12-triangle cube winding and face normals of
            // the existing MeshBuilder. One template is shared by all boxes.
            MeshBuilder templateMesh;
            templateMesh.box({0,0,0},{1,1,1},0,1.f);
            if(templateMesh.vertices.size()!=36){
                error=L"Maillage cube d'instancing incorrect.";return false;
            }
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth=UINT(templateMesh.vertices.size()*sizeof(GpuVertex));
            bd.Usage=D3D11_USAGE_IMMUTABLE;
            bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA init{};init.pSysMem=templateMesh.vertices.data();
            if(FAILED(device_->CreateBuffer(&bd,&init,&boxVertices_))){
                error=L"Allocation de la géométrie cube partagée impossible.";return false;
            }
            bd.ByteWidth=UINT(boxData.size()*sizeof(GpuBoxInstance));
            init.pSysMem=boxData.data();
            if(FAILED(device_->CreateBuffer(&bd,&init,&boxInstances_))){
                error=L"Allocation des instances GPU impossible.";return false;
            }
            boxInstanceCount_=UINT(boxData.size());
            constexpr size_t kBoxBatch=128;
            for(size_t first=0;first<boxData.size();first+=kBoxBatch){
                const size_t end=std::min(boxData.size(),first+kBoxBatch);
                DrawChunk batch;
                batch.first=UINT(first);batch.count=UINT(end-first);
                batch.mn={boxData[first].lo[0],boxData[first].lo[1],boxData[first].lo[2]};
                batch.mx={boxData[first].hi[0],boxData[first].hi[1],boxData[first].hi[2]};
                for(size_t i=first+1;i<end;++i){
                    const auto& box=boxData[i];
                    for(int axis=0;axis<3;++axis){
                        float *lo=axis==0?&batch.mn.x:axis==1?&batch.mn.y:&batch.mn.z;
                        float *hi=axis==0?&batch.mx.x:axis==1?&batch.mx.y:&batch.mx.z;
                        *lo=std::min(*lo,box.lo[axis]);
                        *hi=std::max(*hi,box.hi[axis]);
                    }
                }
                boxChunks_.push_back(batch);
            }
        }
        if(!ellipses_.empty()){
            // Low / medium / high: 8×16, 16×32 and 32×64. Shared meshes are
            // authored once per scene; their UVs match legacy ellipsoids.
            const int ringsLevels[]={8,16,32};
            for(int lod=0;lod<3;++lod){
                const int rings=ringsLevels[lod],slices=rings*2;
                MeshBuilder sphere;
                auto point=[&](int y,int x){
                    const float phi=PI*float(y)/rings,theta=2.f*PI*float(x)/slices;
                    return V{std::sin(phi)*std::cos(theta),std::cos(phi),
                             std::sin(phi)*std::sin(theta)};
                };
                for(int y=0;y<rings;++y)for(int x=0;x<slices;++x){
                    const V p00=point(y,x),p01=point(y,x+1);
                    const V p10=point(y+1,x),p11=point(y+1,x+1);
                    const float u0=2.5f*x/slices,u1=2.5f*(x+1)/slices;
                    const float v0=float(y)/rings,v1=float(y+1)/rings;
                    sphere.tri(p00,p10,p11,p00,p10,p11,u0,v0,u0,v1,u1,v1,0);
                    sphere.tri(p00,p11,p01,p00,p11,p01,u0,v0,u1,v1,u1,v0,0);
                }
                D3D11_BUFFER_DESC bd{};
                bd.ByteWidth=UINT(sphere.vertices.size()*sizeof(GpuVertex));
                bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA init{};init.pSysMem=sphere.vertices.data();
                if(FAILED(device_->CreateBuffer(&bd,&init,&ellipseVertices_[lod]))){
                    error=L"Allocation des niveaux de détail ellipsoïdes impossible.";return false;
                }
                ellipseVertexCounts_[lod]=UINT(sphere.vertices.size());
            }
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth=UINT(ellipses_.size()*sizeof(GpuEllipseInstance));
            bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device_->CreateBuffer(&bd,nullptr,&ellipseInstances_))){
                error=L"Allocation du buffer dynamique des ellipsoïdes impossible.";return false;
            }
            ellipseInstanceCount_=UINT(ellipses_.size());
        }
        if(!foliage_.empty()){
            const int segmentLevels[]={8,12,18};
            const int ringLevels[]={4,6,9};
            const int leafLevels[]={0,12,36};
            for(int lod=0;lod<3;++lod){
                const auto unit=makeFoliageTemplate(
                    segmentLevels[lod],ringLevels[lod],leafLevels[lod]);
                if(unit.empty()){error=L"Le maillage LOD du feuillage est vide.";return false;}
                D3D11_BUFFER_DESC bd{};
                bd.ByteWidth=UINT(unit.size()*sizeof(GpuVertex));
                bd.Usage=D3D11_USAGE_IMMUTABLE;
                bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA init{};init.pSysMem=unit.data();
                if(FAILED(device_->CreateBuffer(&bd,&init,&foliageVertices_[lod]))){
                    error=L"Allocation des maillages LOD du feuillage impossible.";return false;
                }
                foliageVertexCounts_[lod]=UINT(unit.size());
            }
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth=UINT(foliage_.size()*sizeof(GpuEllipseInstance));
            bd.Usage=D3D11_USAGE_DYNAMIC;
            bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device_->CreateBuffer(&bd,nullptr,&foliageInstances_))){
                error=L"Allocation des instances de feuillage impossible.";return false;
            }
            foliageInstanceCount_=UINT(foliage_.size());
        }
        if(!mesh.vertices.empty()){
            // 4096 triangles per batch. Bounding boxes are derived from the
            // real emitted vertices: this also works for complicated shapes,
            // foliage and the appended world ground without OBJ assumptions.
            constexpr size_t kChunkVertices=3u*4096u;
            for(size_t start=0;start<mesh.vertices.size();start+=kChunkVertices){
                const size_t end=std::min(start+kChunkVertices,mesh.vertices.size());
                DrawChunk chunk;
                chunk.first=UINT(start);
                chunk.count=UINT(end-start);
                const auto& first=mesh.vertices[start];
                chunk.mn={first.p[0],first.p[1],first.p[2]};
                chunk.mx=chunk.mn;
                for(size_t i=start+1;i<end;++i){
                    const auto& v=mesh.vertices[i];
                    chunk.mn.x=std::min(chunk.mn.x,v.p[0]);
                    chunk.mn.y=std::min(chunk.mn.y,v.p[1]);
                    chunk.mn.z=std::min(chunk.mn.z,v.p[2]);
                    chunk.mx.x=std::max(chunk.mx.x,v.p[0]);
                    chunk.mx.y=std::max(chunk.mx.y,v.p[1]);
                    chunk.mx.z=std::max(chunk.mx.z,v.p[2]);
                }
                chunks_.push_back(chunk);
            }
            D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(mesh.vertices.size()*sizeof(GpuVertex));bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=mesh.vertices.data();if(FAILED(device_->CreateBuffer(&bd,&init,&vertices_))){error=L"Allocation de la géométrie dans la mémoire GPU impossible.";return false;}vertexCount_=UINT(mesh.vertices.size());}
        if(!mesh.transparent.empty()){D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(mesh.transparent.size()*3*sizeof(GpuVertex));bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&transparentVertices_))){error=L"Allocation du tampon des surfaces transparentes impossible.";return false;}}
        transparent_=std::move(mesh.transparent);
        UINT texSize=512;for(const Material& material:scene.mat){if(!material.colorMip.empty())texSize=std::max(texSize,material.colorMip.front().size);else if(!material.mip.empty())texSize=std::max(texSize,material.mip.front().size);}const UINT matCount=UINT(std::max<size_t>(1,scene.mat.size()));
        UINT mipLevels=1;for(UINT size=texSize;size>1;size>>=1)++mipLevels;
        std::vector<std::vector<uint32_t>> mipRgba(size_t(matCount)*mipLevels);
        std::vector<std::vector<uint32_t>> mipNormal(size_t(matCount)*mipLevels);
        std::vector<std::vector<uint8_t>> mipRoughness(size_t(matCount)*mipLevels);
        std::vector<D3D11_SUBRESOURCE_DATA> sub(size_t(matCount)*mipLevels),normalSub(size_t(matCount)*mipLevels),roughSub(size_t(matCount)*mipLevels);
        for(UINT m=0;m<matCount;m++)for(UINT level=0;level<mipLevels;level++){
            const UINT targetSize=std::max(1u,texSize>>level);const size_t subIndex=size_t(m)*mipLevels+level;
            auto& color=mipRgba[subIndex];color.resize(size_t(targetSize)*targetSize);
            auto& normal=mipNormal[subIndex];normal.resize(size_t(targetSize)*targetSize,0xffff8080u);
            auto& rough=mipRoughness[subIndex];rough.resize(size_t(targetSize)*targetSize,184);
            const Material& material=scene.mat[m];
            if(!material.colorMip.empty()){
                const ColorLevel& l=material.colorMip[std::min<size_t>(level,material.colorMip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    const uint32_t c=l.p[size_t(sy)*l.size+sx];const uint32_t r=(c>>16)&255,g=(c>>8)&255,b=c&255;
                    color[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                }
            }else if(!material.mip.empty()){
                const Level& l=material.mip[std::min<size_t>(level,material.mip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    const uint16_t c=l.p[size_t(sy)*l.size+sx];const uint32_t r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
                    color[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                }
            }else std::fill(color.begin(),color.end(),0xff808080u);
            if(!material.normalMip.empty()){
                const Level& l=material.normalMip[std::min<size_t>(level,material.normalMip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    const uint16_t c=l.p[size_t(sy)*l.size+sx];const uint32_t r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
                    normal[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                }
            }
            if(!material.roughnessMip.empty()){
                const Level& l=material.roughnessMip[std::min<size_t>(level,material.roughnessMip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    rough[size_t(y)*targetSize+x]=uint8_t(((l.p[size_t(sy)*l.size+sx]>>11)&31)*255/31);
                }
            }
            sub[subIndex].pSysMem=color.data();sub[subIndex].SysMemPitch=targetSize*4;sub[subIndex].SysMemSlicePitch=targetSize*targetSize*4;
            normalSub[subIndex].pSysMem=normal.data();normalSub[subIndex].SysMemPitch=targetSize*4;normalSub[subIndex].SysMemSlicePitch=targetSize*targetSize*4;
            roughSub[subIndex].pSysMem=rough.data();roughSub[subIndex].SysMemPitch=targetSize;roughSub[subIndex].SysMemSlicePitch=targetSize*targetSize;
        }
        D3D11_TEXTURE2D_DESC td{};td.Width=texSize;td.Height=texSize;td.MipLevels=mipLevels;td.ArraySize=matCount;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;ComPtr<ID3D11Texture2D> texture;
        if(FAILED(device_->CreateTexture2D(&td,sub.data(),&texture))){error=L"Chargement des textures dans DirectX 11 impossible.";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;sv.Texture2DArray.MipLevels=mipLevels;sv.Texture2DArray.ArraySize=matCount;
        if(FAILED(device_->CreateShaderResourceView(texture.Get(),&sv,&albedo_))){error=L"Création de la table de textures DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        // SRV format must match the typed normal-map resource. Keeping the
        // previous albedo SRGB view description causes E_INVALIDARG on D3D11.
        sv.Format=td.Format;
        ComPtr<ID3D11Texture2D> normalTexture;
        if(FAILED(device_->CreateTexture2D(&td,normalSub.data(),&normalTexture))){error=L"Chargement des normales dans DirectX 11 impossible.";return false;}
        if(FAILED(device_->CreateShaderResourceView(normalTexture.Get(),&sv,&normal_))){error=L"Création de la table de normales DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_R8_UNORM;ComPtr<ID3D11Texture2D> roughTexture;
        if(FAILED(device_->CreateTexture2D(&td,roughSub.data(),&roughTexture))){error=L"Chargement des rugosités dans DirectX 11 impossible.";return false;}
        sv.Format=td.Format;
        if(FAILED(device_->CreateShaderResourceView(roughTexture.Get(),&sv,&roughness_))){error=L"Création de la table de rugosités DirectX 11 impossible.";return false;}
        sceneKey_=scene.obj.data();objectCount_=scene.obj.size();
        sceneIdentity_=scene.gpuCacheIdentity;sceneRevision_=scene.gpuCacheRevision;
        boxInstancingEnabled_=useBoxInstancing;
        ellipseInstancingEnabled_=useEllipseInstancing;
        foliageInstancingEnabled_=useFoliageInstancing;
        return true;
    }
    bool ensureTargets(int w,int h,unsigned msaa,std::wstring& error) {
        if(w==width_&&h==height_&&target_&&msaaPreference_==msaa)return true;
        if(!pending_.empty()){
            error=L"Vider les images GPU différées avant de changer de résolution.";
            return false;
        }
        for(auto& slot:readbackSlots_)slot=ReadbackSlot{};
        readbackDepth_=0;nextReadbackSlot_=0;
        width_=w;height_=h;msaaPreference_=msaa;
        target_.Reset();resolve_.Reset();rtv_.Reset();depthTexture_.Reset();dsv_.Reset();
        sampleCount_=1;sampleQuality_=0;
        for(UINT candidate:{4u,2u}){
            if(candidate>msaa)continue;
            UINT levels=0,depthLevels=0;
            if(SUCCEEDED(device_->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,candidate,&levels))&&levels>0&&
               SUCCEEDED(device_->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT,candidate,&depthLevels))&&depthLevels>0){
                sampleCount_=candidate;sampleQuality_=std::min(levels,depthLevels)-1;break;
            }
        }
        D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;td.SampleDesc.Count=sampleCount_;td.SampleDesc.Quality=sampleQuality_;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&target_))){error=L"Création de la cible image DirectX 11 impossible.";return false;}
        // The render target must be TYPELESS for an sRGB RTV on D3D11/WARP.
        // A typed UNORM texture with an sRGB view fails E_INVALIDARG.
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};rtvDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;rtvDesc.ViewDimension=sampleCount_>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
        if(sampleCount_==1)rtvDesc.Texture2D.MipSlice=0;
        if(FAILED(device_->CreateRenderTargetView(target_.Get(),&rtvDesc,&rtv_))){error=L"Création de la cible sRGB DirectX 11 impossible.";return false;}
        td.SampleDesc.Count=1;td.SampleDesc.Quality=0;td.BindFlags=0;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&resolve_))){error=L"Création de la cible de résolution DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;td.SampleDesc.Count=sampleCount_;td.SampleDesc.Quality=sampleQuality_;td.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&depthTexture_))||FAILED(device_->CreateDepthStencilView(depthTexture_.Get(),nullptr,&dsv_))){error=L"Création du tampon de profondeur DirectX 11 impossible.";return false;}
        return true;
    }
    bool ensureAnisotropy(unsigned aniso,std::wstring& error){
        if(sampler_&&samplerAnisotropy_==aniso)return true;
        D3D11_SAMPLER_DESC sd{};
        sd.Filter=aniso>1?D3D11_FILTER_ANISOTROPIC:D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.MaxAnisotropy=aniso;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxLOD=D3D11_FLOAT32_MAX;
        ComPtr<ID3D11SamplerState> selected;
        if(FAILED(device_->CreateSamplerState(&sd,&selected))){
            error=L"Réglage anisotropique DirectX 11 non disponible.";return false;
        }
        sampler_=std::move(selected);
        samplerAnisotropy_=aniso;
        return true;
    }
    bool ensureReadback(unsigned depth,int w,int h,std::wstring& error){
        if(depth==readbackDepth_ && readbackSlots_[0].staging)return true;
        if(!pending_.empty()){
            error=L"La profondeur de lecture GPU ne peut changer tant que des images sont en attente.";
            return false;
        }
        for(auto& slot:readbackSlots_)slot=ReadbackSlot{};
        nextReadbackSlot_=0;readbackDepth_=0;
        D3D11_TEXTURE2D_DESC td{};
        td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count=1;td.SampleDesc.Quality=0;
        td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(unsigned i=0;i<depth;++i){
            auto& slot=readbackSlots_[i];
            if(FAILED(device_->CreateTexture2D(&td,nullptr,&slot.staging))){
                error=L"Impossible de créer les tampons de lecture GPU différée.";
                return false;
            }
            slot.width=w;slot.height=h;
            // Timestamp/disjoint queries are optional on older drivers;
            // rendering and byte-identical readback still work without them.
            D3D11_QUERY_DESC qd{};
            qd.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;
            const bool disjointOk=SUCCEEDED(device_->CreateQuery(&qd,&slot.disjoint));
            qd.Query=D3D11_QUERY_TIMESTAMP;
            const bool timestampsOk=disjointOk &&
                SUCCEEDED(device_->CreateQuery(&qd,&slot.start)) &&
                SUCCEEDED(device_->CreateQuery(&qd,&slot.afterDraw)) &&
                SUCCEEDED(device_->CreateQuery(&qd,&slot.afterCopy));
            slot.queryReady=disjointOk&&timestampsOk;
        }
        readbackDepth_=depth;
        return true;
    }
    ComPtr<ID3D11Device> device_;ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11VertexShader> vs_,boxVs_,ellipseVs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11InputLayout> layout_,boxLayout_,ellipseLayout_;
    ComPtr<ID3D11Buffer> constants_,vertices_,transparentVertices_,boxVertices_,boxInstances_,ellipseInstances_,foliageInstances_;
    std::array<ComPtr<ID3D11Buffer>,3> ellipseVertices_;
    std::array<UINT,3> ellipseVertexCounts_{{0,0,0}};
    std::vector<GpuEllipseInstance> ellipses_;
    UINT ellipseInstanceCount_=0;
    std::array<ComPtr<ID3D11Buffer>,3> foliageVertices_;
    std::array<UINT,3> foliageVertexCounts_{{0,0,0}};
    std::vector<GpuEllipseInstance> foliage_;
    UINT foliageInstanceCount_=0;
    ComPtr<ID3D11RasterizerState> raster_;ComPtr<ID3D11DepthStencilState> depth_,transparentDepth_;ComPtr<ID3D11BlendState> blend_;ComPtr<ID3D11SamplerState> sampler_;ComPtr<ID3D11ShaderResourceView> albedo_,roughness_,normal_,shadowSrv_;ComPtr<ID3D11Texture2D> shadowTexture_;int shadowSize_=0;std::vector<TransparentTriangle> transparent_;
    ComPtr<ID3D11Texture2D> target_,resolve_,depthTexture_;
    std::array<ReadbackSlot,3> readbackSlots_;
    std::deque<unsigned> pending_;
    unsigned readbackDepth_=0,nextReadbackSlot_=0;
    ComPtr<ID3D11RenderTargetView> rtv_;ComPtr<ID3D11DepthStencilView> dsv_;
    std::vector<DrawChunk> chunks_,boxChunks_;
    UINT boxInstanceCount_=0;
    unsigned msaaPreference_=0,samplerAnisotropy_=16;
    bool boxInstancingEnabled_=false,ellipseInstancingEnabled_=false,foliageInstancingEnabled_=false;
    const Obj* sceneKey_=nullptr;size_t objectCount_=0;uint64_t sceneIdentity_=0,sceneRevision_=0;UINT vertexCount_=0;UINT sampleCount_=1,sampleQuality_=0;int width_=0,height_=0;
};
}
RasterQualitySettings rasterQualityProfile(RasterQualityProfile preset){
    RasterQualitySettings q;
    switch(preset){
    case RasterQualityProfile::Performance:
        q.msaaSamples=1;q.anisotropy=2;q.lodQualityPercent=60;break;
    case RasterQualityProfile::Balanced:
        q.msaaSamples=2;q.anisotropy=8;q.lodQualityPercent=85;break;
    case RasterQualityProfile::High:
        q.msaaSamples=4;q.anisotropy=16;q.lodQualityPercent=100;break;
    case RasterQualityProfile::Ultra:
        q.msaaSamples=4;q.anisotropy=16;q.lodQualityPercent=200;
        q.dynamicLod=false;break;
    }
    return q;
}
RasterQualitySettings rasterQualitySettings(){
    return unpackQuality(gpuQualityBits.load(std::memory_order_acquire));
}
bool setRasterQualitySettings(const RasterQualitySettings& q,std::wstring& error){
    const auto validSample=q.msaaSamples==1||q.msaaSamples==2||q.msaaSamples==4;
    const auto validAniso=q.anisotropy==1||q.anisotropy==2||
        q.anisotropy==4||q.anisotropy==8||q.anisotropy==16;
    if(!validSample||!validAniso||q.lodQualityPercent<50||q.lodQualityPercent>200){
        error=L"Réglages DX11 invalides : MSAA 1/2/4, anisotropie 1/2/4/8/16, LOD 50..200%.";
        return false;
    }
    gpuQualityBits.store(packQuality(q),std::memory_order_release);
    return true;
}
static DirectX11Preview& rasterThreadContext(){
    thread_local DirectX11Preview renderer;
    return renderer;
}
bool renderDirectX11Preview(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error) {
    return rasterThreadContext().render(scene,shadows,cam,w,h,pixels,stats,error);
}
bool enqueueDirectX11Preview(const Scene& scene,const ShadowMap& shadows,const Camera& cam,
                            int w,int h,unsigned depth,std::wstring& error){
    return rasterThreadContext().enqueue(scene,shadows,cam,w,h,depth,error);
}
bool receiveDirectX11Preview(std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
    return rasterThreadContext().receive(pixels,stats,error);
}
}
#else
namespace m3d {
RasterQualitySettings rasterQualityProfile(RasterQualityProfile p){
    RasterQualitySettings q;
    if(p==RasterQualityProfile::Performance){q.msaaSamples=1;q.anisotropy=2;q.lodQualityPercent=60;}
    else if(p==RasterQualityProfile::Balanced){q.msaaSamples=2;q.anisotropy=8;q.lodQualityPercent=85;}
    else if(p==RasterQualityProfile::High){q.lodQualityPercent=100;}
    else if(p==RasterQualityProfile::Ultra){q.lodQualityPercent=200;q.dynamicLod=false;}
    return q;
}
RasterQualitySettings rasterQualitySettings(){return RasterQualitySettings{};}
bool setRasterQualitySettings(const RasterQualitySettings&,std::wstring& error){error=L"DirectX 11 indisponible sur cet OS.";return false;}
bool renderDirectX11Preview(const Scene&,const ShadowMap&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error) { error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false; }
bool enqueueDirectX11Preview(const Scene&,const ShadowMap&,const Camera&,int,int,unsigned,std::wstring& error) {error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false;}
bool receiveDirectX11Preview(std::vector<uint32_t>&,Stats&,std::wstring& error) {error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false;}
}
#endif
