#include "m3d_engine.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstddef>

using Microsoft::WRL::ComPtr;

namespace m3d {
namespace {
struct GpuVertex { float p[3], n[3], uv[2]; uint32_t material; };
struct FrameConstants {
    float eye[3], tanHalf;
    float forward[3], aspect;
    float right[3], nearPlane;
    float up[3], farPlane;
    float light[3], ambient;
    float sky[3]; uint32_t materialCount;
};

constexpr char kShader[] = R"(
cbuffer Frame : register(b0) {
    float3 eye; float tanHalf;
    float3 forward; float aspect;
    float3 right; float nearPlane;
    float3 up; float farPlane;
    float3 lightDir; float ambient;
    float3 skyColor; uint materialCount;
};
Texture2DArray<float4> albedo : register(t0);
SamplerState wrapSampler : register(s0);
struct VSIn { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; uint mat:MATERIAL; };
struct VSOut { float4 p:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; nointerpolation uint mat:MATERIAL; };
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
    o.p = float4(nx * safeZ, ny * safeZ, depth * z, z);
    o.n = i.n; o.uv = i.uv; o.mat = min(i.mat, materialCount - 1);
    return o;
}
float4 PSMain(VSOut i):SV_TARGET {
    float3 base = albedo.Sample(wrapSampler, float3(frac(i.uv), i.mat)).rgb;
    float diffuse = saturate(dot(normalize(i.n), normalize(-lightDir)));
    float light = ambient + (1.0 - ambient) * diffuse;
    return float4(base * light, 1.0);
}
)";

void put(GpuVertex& v,V p,V n,float u,float vv,uint32_t mat) {
    v={{p.x,p.y,p.z},{n.x,n.y,n.z},{u,vv},mat};
}

class MeshBuilder {
public:
    std::vector<GpuVertex> vertices;
    void tri(V a,V b,V c,V na,V nb,V nc,float ua,float va,float ub,float vb,float uc,float vc,uint32_t mat) {
        if(vertices.size()>12000000u)return;
        GpuVertex v{};put(v,a,na,ua,va,mat);vertices.push_back(v);
        put(v,b,nb,ub,vb,mat);vertices.push_back(v);
        put(v,c,nc,uc,vc,mat);vertices.push_back(v);
    }
    void face(V a,V b,V c,V d,V n,uint32_t mat) {
        tri(a,b,c,n,n,n,0,0,1,0,1,1,mat);tri(a,c,d,n,n,n,0,0,1,1,0,1,mat);
    }
    void box(V mn,V mx,uint32_t mat) {
        face({mn.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mn.y,mn.z},{0,0,-1},mat);
        face({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},{0,0,1},mat);
        face({mn.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},{-1,0,0},mat);
        face({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},{mx.x,mn.y,mx.z},{1,0,0},mat);
        face({mn.x,mn.y,mx.z},{mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mn.y,mx.z},{0,-1,0},mat);
        face({mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mx.y,mn.z},{0,1,0},mat);
    }
    void ellipsoid(V c,V r,uint32_t mat) {
        constexpr int rings=4,slices=8;
        auto point=[&](int y,int x){float phi=PI*float(y)/rings,theta=2*PI*float(x)/slices;return V{c.x+r.x*std::sin(phi)*std::cos(theta),c.y+r.y*std::cos(phi),c.z+r.z*std::sin(phi)*std::sin(theta)};};
        auto normal=[&](V p){return norm({(p.x-c.x)/std::max(1e-5f,r.x*r.x),(p.y-c.y)/std::max(1e-5f,r.y*r.y),(p.z-c.z)/std::max(1e-5f,r.z*r.z)});};
        for(int y=0;y<rings;y++)for(int x=0;x<slices;x++){
            V p00=point(y,x),p01=point(y,x+1),p10=point(y+1,x),p11=point(y+1,x+1);
            V n00=normal(p00),n01=normal(p01),n10=normal(p10),n11=normal(p11);
            float u0=float(x)/slices,u1=float(x+1)/slices,v0=float(y)/rings,v1=float(y+1)/rings;
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

class DirectX11Preview {
public:
    bool render(const Scene& scene,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error) {
        const auto started=std::chrono::steady_clock::now();
        if(w<=0||h<=0){error=L"Résolution GPU invalide.";return false;}
        if(!ensureDevice(error)||!ensureScene(scene,error)||!ensureTargets(w,h,error))return false;
        FrameConstants cb{};
        cb.eye[0]=cam.eye.x;cb.eye[1]=cam.eye.y;cb.eye[2]=cam.eye.z;cb.tanHalf=cam.tanHalf;
        cb.forward[0]=cam.f.x;cb.forward[1]=cam.f.y;cb.forward[2]=cam.f.z;cb.aspect=cam.aspect;
        cb.right[0]=cam.r.x;cb.right[1]=cam.r.y;cb.right[2]=cam.r.z;cb.nearPlane=.08f;
        cb.up[0]=cam.u.x;cb.up[1]=cam.u.y;cb.up[2]=cam.u.z;cb.farPlane=10000.f;
        cb.light[0]=sun.x;cb.light[1]=sun.y;cb.light[2]=sun.z;cb.ambient=.24f;
        V bg=scene.underwaterEnvironment?V{7,27,39}:skyHorizon*.65f+skyZenith*.35f;
        cb.sky[0]=bg.x/255.f;cb.sky[1]=bg.y/255.f;cb.sky[2]=bg.z/255.f;cb.materialCount=uint32_t(std::max<size_t>(1,scene.mat.size()));
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr=context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        if(FAILED(hr)){error=L"Impossible de transmettre les paramètres à DirectX 11.";return false;}
        std::memcpy(mapped.pData,&cb,sizeof(cb));context_->Unmap(constants_.Get(),0);
        const float clear[4]={cb.sky[0],cb.sky[1],cb.sky[2],1.f};
        context_->OMSetRenderTargets(1,rtv_.GetAddressOf(),dsv_.Get());context_->ClearRenderTargetView(rtv_.Get(),clear);context_->ClearDepthStencilView(dsv_.Get(),D3D11_CLEAR_DEPTH,1.f,0);
        D3D11_VIEWPORT vp{};vp.Width=float(w);vp.Height=float(h);vp.MinDepth=0;vp.MaxDepth=1;context_->RSSetViewports(1,&vp);
        UINT stride=sizeof(GpuVertex),offset=0;ID3D11Buffer* vb=vertices_.Get();context_->IASetInputLayout(layout_.Get());context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->RSSetState(raster_.Get());context_->OMSetDepthStencilState(depth_.Get(),0);context_->VSSetShader(vs_.Get(),nullptr,0);context_->PSSetShader(ps_.Get(),nullptr,0);
        ID3D11Buffer* cbuff=constants_.Get();context_->VSSetConstantBuffers(0,1,&cbuff);context_->PSSetConstantBuffers(0,1,&cbuff);
        ID3D11ShaderResourceView* srv=albedo_.Get();context_->PSSetShaderResources(0,1,&srv);ID3D11SamplerState* sampler=sampler_.Get();context_->PSSetSamplers(0,1,&sampler);
        context_->Draw(vertexCount_,0);context_->CopyResource(staging_.Get(),target_.Get());
        hr=context_->Map(staging_.Get(),0,D3D11_MAP_READ,0,&mapped);
        if(FAILED(hr)){error=L"Impossible de relire l’image calculée par DirectX 11.";return false;}
        pixels.resize(size_t(w)*h);
        for(int y=0;y<h;y++){const uint8_t* row=static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch;for(int x=0;x<w;x++){const uint8_t* p=row+size_t(x)*4;pixels[size_t(y)*w+x]=0xff000000u|(uint32_t(p[0])<<16)|(uint32_t(p[1])<<8)|uint32_t(p[2]);}}
        context_->Unmap(staging_.Get(),0);stats={};stats.gpu=true;stats.threads=0;stats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();stats.tests=vertexCount_/3;return true;
    }
private:
    bool ensureDevice(std::wstring& error) {
        if(device_)return true;
        D3D_FEATURE_LEVEL feature{};
        HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,&feature,&context_);
        if(FAILED(hr)){error=L"Aucun périphérique Direct3D 11 matériel n’est disponible.";return false;}
        ComPtr<ID3DBlob> vsCode,psCode,errors;
        hr=D3DCompile(kShader,sizeof(kShader)-1,"m3d_directx11_preview",nullptr,nullptr,"VSMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsCode,&errors);
        if(FAILED(hr)){error=L"Compilation du vertex shader DirectX 11 impossible.";return false;}
        hr=D3DCompile(kShader,sizeof(kShader)-1,"m3d_directx11_preview",nullptr,nullptr,"PSMain","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psCode,&errors);
        if(FAILED(hr)){error=L"Compilation du pixel shader DirectX 11 impossible.";return false;}
        if(FAILED(device_->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,&vs_))||FAILED(device_->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps_))){error=L"Création des shaders DirectX 11 impossible.";return false;}
        const D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertex,p),D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertex,n),D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(GpuVertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},{"MATERIAL",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertex,material),D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(device_->CreateInputLayout(input,4,vsCode->GetBufferPointer(),vsCode->GetBufferSize(),&layout_))){error=L"Création du format de sommet DirectX 11 impossible.";return false;}
        D3D11_BUFFER_DESC cbd{};cbd.ByteWidth=sizeof(FrameConstants);cbd.Usage=D3D11_USAGE_DYNAMIC;cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cbd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(device_->CreateBuffer(&cbd,nullptr,&constants_))){error=L"Création du buffer de constantes DirectX 11 impossible.";return false;}
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
        if(FAILED(device_->CreateRasterizerState(&rd,&raster_))){error=L"Création de l’état raster DirectX 11 impossible.";return false;}
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        if(FAILED(device_->CreateDepthStencilState(&dd,&depth_))){error=L"Création du depth buffer DirectX 11 impossible.";return false;}
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device_->CreateSamplerState(&sd,&sampler_))){error=L"Création du sampler DirectX 11 impossible.";return false;}return true;
    }
    bool ensureScene(const Scene& scene,std::wstring& error) {
        if(sceneKey_==scene.obj.data()&&objectCount_==scene.obj.size()&&vertices_)return true;
        MeshBuilder mesh;
        for(const Obj& o:scene.obj){
            const uint32_t m=o.mat;
            if(o.kind==Kind::Triangle)mesh.tri(o.a,o.b,o.c,o.n0,o.n1,o.n2,o.uv0.x,o.uv0.y,o.uv1.x,o.uv1.y,o.uv2.x,o.uv2.y,m);
            else if(o.kind==Kind::Box||o.kind==Kind::RoundBox)mesh.box(o.mn,o.mx,m);
            else if(o.kind==Kind::Ellipse)mesh.ellipsoid(o.a,o.b,m);
            else if(o.kind==Kind::Capsule){V mid=(o.a+o.b)*.5f;V r{(o.mx.x-o.mn.x)*.5f,(o.mx.y-o.mn.y)*.5f,(o.mx.z-o.mn.z)*.5f};mesh.ellipsoid(mid,r,m);}
            else if(o.kind==Kind::Cone)mesh.frustum(o.a,o.n0,o.len,o.r,o.r1,m);
            if(mesh.vertices.size()>12000000u){error=L"Scène trop grande pour l’aperçu DirectX 11 (limite de sécurité mémoire).";return false;}
        }
        mesh.ground(scene);
        if(mesh.vertices.size()>12000000u){error=L"Scène trop grande pour l’aperçu DirectX 11 (limite de sécurité mémoire).";return false;}
        if(mesh.vertices.empty()){error=L"La scène ne contient aucune géométrie affichable par DirectX 11.";return false;}
        D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(mesh.vertices.size()*sizeof(GpuVertex));bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=mesh.vertices.data();
        if(FAILED(device_->CreateBuffer(&bd,&init,&vertices_))){error=L"Allocation de la géométrie dans la mémoire GPU impossible.";return false;}vertexCount_=UINT(mesh.vertices.size());
        const UINT texSize=512,matCount=UINT(std::max<size_t>(1,scene.mat.size()));std::vector<std::vector<uint32_t>> rgba(matCount,std::vector<uint32_t>(size_t(texSize)*texSize,0xff808080u));
        for(UINT m=0;m<scene.mat.size();m++)if(!scene.mat[m].mip.empty()){const Level& l=scene.mat[m].mip.front();if(l.size!=texSize||l.p.size()<size_t(texSize)*texSize)continue;for(size_t i=0;i<rgba[m].size();i++){uint16_t c=l.p[i];uint32_t r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;rgba[m][i]=0xff000000u|(r<<16)|(g<<8)|b;}}
        std::vector<D3D11_SUBRESOURCE_DATA> sub(matCount);for(UINT i=0;i<matCount;i++){sub[i].pSysMem=rgba[i].data();sub[i].SysMemPitch=texSize*4;sub[i].SysMemSlicePitch=texSize*texSize*4;}
        D3D11_TEXTURE2D_DESC td{};td.Width=texSize;td.Height=texSize;td.MipLevels=1;td.ArraySize=matCount;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;ComPtr<ID3D11Texture2D> texture;
        if(FAILED(device_->CreateTexture2D(&td,sub.data(),&texture))){error=L"Chargement des textures dans DirectX 11 impossible.";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;sv.Texture2DArray.MipLevels=1;sv.Texture2DArray.ArraySize=matCount;
        if(FAILED(device_->CreateShaderResourceView(texture.Get(),&sv,&albedo_))){error=L"Création de la table de textures DirectX 11 impossible.";return false;}sceneKey_=scene.obj.data();objectCount_=scene.obj.size();return true;
    }
    bool ensureTargets(int w,int h,std::wstring& error) {
        if(w==width_&&h==height_&&target_)return true;width_=w;height_=h;target_.Reset();rtv_.Reset();staging_.Reset();depthTexture_.Reset();dsv_.Reset();
        D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&target_))||FAILED(device_->CreateRenderTargetView(target_.Get(),nullptr,&rtv_))){error=L"Création de la cible image DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;td.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&depthTexture_))||FAILED(device_->CreateDepthStencilView(depthTexture_.Get(),nullptr,&dsv_))){error=L"Création du tampon de profondeur DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&staging_))){error=L"Création du tampon de lecture DirectX 11 impossible.";return false;}return true;
    }
    ComPtr<ID3D11Device> device_;ComPtr<ID3D11DeviceContext> context_;ComPtr<ID3D11VertexShader> vs_;ComPtr<ID3D11PixelShader> ps_;ComPtr<ID3D11InputLayout> layout_;
    ComPtr<ID3D11Buffer> constants_,vertices_;ComPtr<ID3D11RasterizerState> raster_;ComPtr<ID3D11DepthStencilState> depth_;ComPtr<ID3D11SamplerState> sampler_;ComPtr<ID3D11ShaderResourceView> albedo_;
    ComPtr<ID3D11Texture2D> target_,depthTexture_,staging_;ComPtr<ID3D11RenderTargetView> rtv_;ComPtr<ID3D11DepthStencilView> dsv_;
    const Obj* sceneKey_=nullptr;size_t objectCount_=0;UINT vertexCount_=0;int width_=0,height_=0;
};
}
bool renderDirectX11Preview(const Scene& scene,const ShadowMap&,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error) {
    thread_local DirectX11Preview renderer;return renderer.render(scene,cam,w,h,pixels,stats,error);
}
}
#else
namespace m3d {
bool renderDirectX11Preview(const Scene&,const ShadowMap&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error) { error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false; }
}
#endif
