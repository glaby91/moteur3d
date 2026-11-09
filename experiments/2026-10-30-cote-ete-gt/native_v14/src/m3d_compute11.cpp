#include "m3d_engine.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <cstdlib>
using Microsoft::WRL::ComPtr;
namespace m3d {
namespace {
struct alignas(16) GpuNode { float mn[3]; uint32_t left; float mx[3]; uint32_t right; uint32_t first,count,pad0,pad1; };
struct alignas(16) GpuTri { float p0[3]; uint32_t material; float p1[3]; uint32_t pad0; float p2[3]; uint32_t pad1; float n0[3]; uint32_t pad2; float n1[3]; uint32_t pad3; float n2[3]; uint32_t pad4; float uv0[2],uv1[2],uv2[2],uvPad[2]; };
static_assert(sizeof(GpuTri)==128,"GPU triangle layout differs from HLSL");
// Source albedo/roughness/normal maps are uploaded separately to a texture array.
struct alignas(16) GpuMaterial { float albedo[3]; float roughnessScale; float metallic,transmission,ior,pad; };
static_assert(sizeof(GpuMaterial)==32, "GPU material buffer layout mismatch");
struct alignas(16) Params { float eye[3],tanHalf,forward[3],aspect,right[3],nearPlane,up[3],farPlane; uint32_t width,height,nodeCount,triangleCount,rootIndex,maxTraversal,materialCount,textureBaseSize; float sunDirection[3],ambientStrength; float skyZenith[3],skyStrength; float skyHorizon[3],sunStrength; };
static_assert(sizeof(Params)%16==0, "GPU constant buffer must be aligned");
void v3(float* dst,V v){dst[0]=v.x;dst[1]=v.y;dst[2]=v.z;}
template<class T> bool makeSrv(ID3D11Device* d,const std::vector<T>& values,ComPtr<ID3D11ShaderResourceView>& srv){
 if(values.empty())return false;
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(values.size()*sizeof(T));bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_SHADER_RESOURCE;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;bd.StructureByteStride=sizeof(T);
 D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=values.data();ComPtr<ID3D11Buffer> buf;
 if(FAILED(d->CreateBuffer(&bd,&sd,&buf)))return false;
 D3D11_SHADER_RESOURCE_VIEW_DESC vd{};vd.Format=DXGI_FORMAT_UNKNOWN;vd.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;vd.Buffer.NumElements=UINT(values.size());
 return SUCCEEDED(d->CreateShaderResourceView(buf.Get(),&vd,&srv));
}

// Shared per-material PBR mip chains, converted to an RGBA8 D3D11 texture array.
// Slices: material 0 (albedo, normal, roughness), material 1 (...), etc.
// Keep the GPU preview's memory bounded on integrated Intel GPUs.
constexpr uint32_t gpuTextureBaseSize=512;
uint32_t rgba8(uint32_t r,uint32_t g,uint32_t b){
 return 0xff000000u|(b<<16)|(g<<8)|r;
}
uint32_t colorRgba(uint32_t rgb){
 return rgba8((rgb>>16)&255u,(rgb>>8)&255u,rgb&255u);
}
uint32_t packed565Rgba(uint16_t packed){
 return rgba8(uint32_t((packed>>11)&31u)*255u/31u,
              uint32_t((packed>>5)&63u)*255u/63u,
              uint32_t(packed&31u)*255u/31u);
}
bool makePbrTextureArray(ID3D11Device* device,const std::vector<Material>& materials,
                         ComPtr<ID3D11ShaderResourceView>& view,std::wstring& error){
 if(materials.empty()||materials.size()>256){error=L"Nombre de matériaux incompatible avec PBR GPU";return false;}
 constexpr UINT levels=10; // 512,256,128,64,32,16,8,4,2,1
 const UINT slices=UINT(materials.size())*3u;
 const size_t subresourceCount=size_t(levels)*slices;
 std::vector<std::vector<uint32_t>> pixels(subresourceCount);
 std::vector<D3D11_SUBRESOURCE_DATA> subresources(subresourceCount);
 for(UINT materialIndex=0;materialIndex<materials.size();++materialIndex){
  const auto& material=materials[materialIndex];
  for(UINT kind=0;kind<3;++kind)for(UINT mip=0;mip<levels;++mip){
   const uint32_t size=gpuTextureBaseSize>>mip;
   const UINT index=D3D11CalcSubresource(mip,materialIndex*3u+kind,levels);
   auto& colors=pixels[index];colors.resize(size_t(size)*size);
   if(kind==0){
    const auto level=std::find_if(material.colorMip.begin(),material.colorMip.end(),
              [&](const ColorLevel& value){return value.size==size;});
    if(level==material.colorMip.end()||level->p.size()!=colors.size()){
     error=L"Chaîne mip couleur manquante sur un matériau";return false;
    }
    for(size_t k=0;k<colors.size();++k)colors[k]=colorRgba(level->p[k]);
   } else {
    const auto& chain=kind==1?material.normalMip:material.roughnessMip;
    const auto level=std::find_if(chain.begin(),chain.end(),
              [&](const Level& value){return value.size==size;});
    if(level==chain.end()||level->p.size()!=colors.size()){
     error=L"Chaîne mip normale/rugosité manquante sur un matériau";return false;
    }
    for(size_t k=0;k<colors.size();++k){
     const uint32_t rgb=packed565Rgba(level->p[k]);
     if(kind==1)colors[k]=rgb;
     else {
      const uint32_t rough=rgb&255u;
      colors[k]=rgba8(rough,rough,rough);
     }
    }
   }
   subresources[index].pSysMem=colors.data();
   subresources[index].SysMemPitch=size*4;
  }
 }
 D3D11_TEXTURE2D_DESC td{};
 td.Width=gpuTextureBaseSize;td.Height=gpuTextureBaseSize;
 td.ArraySize=slices;td.MipLevels=levels;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> texture;
 if(FAILED(device->CreateTexture2D(&td,subresources.data(),&texture))){
  error=L"Allocation atlas de textures PBR GPU impossible";return false;
 }
 D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
 desc.Format=td.Format;desc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
 desc.Texture2DArray.MostDetailedMip=0;
 desc.Texture2DArray.MipLevels=levels;
 desc.Texture2DArray.FirstArraySlice=0;
 desc.Texture2DArray.ArraySize=slices;
 if(FAILED(device->CreateShaderResourceView(texture.Get(),&desc,&view))){
  error=L"Lecture atlas PBR GPU impossible";return false;
 }
 return true;
}
}
bool renderCompute11Preview(const Scene& scene,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
 const auto start=std::chrono::steady_clock::now();
 if(w<=0||h<=0||w>8192||h>8192){error=L"Résolution compute invalide";return false;}
 if(scene.root<0||scene.node.empty()||scene.order.empty()){error=L"BVH absent";return false;}
 if(scene.obj.size()>UINT32_MAX||scene.node.size()>UINT32_MAX||scene.order.size()>UINT32_MAX){error=L"Scène trop grande";return false;}
 // Phase 1 supports triangle-only BVH. Never silently drop boxes, cones, capsules, etc.
 for(const auto& o:scene.obj)if(o.kind!=Kind::Triangle){error=L"Compute phase 1 : BVH contenant des primitives non triangulaires ; repli CPU";return false;}
 std::vector<GpuNode> nodes;nodes.reserve(scene.node.size());
 for(const auto& n:scene.node){GpuNode g{};v3(g.mn,n.mn);v3(g.mx,n.mx);g.left=uint32_t(n.left);g.right=uint32_t(n.right);g.first=uint32_t(n.start);g.count=uint32_t(n.count);
 if(n.count<0||n.start<0||uint64_t(n.start)+uint64_t(n.count)>scene.order.size()||(n.count==0&&(n.left<0||n.right<0))){error=L"BVH invalide";return false;}nodes.push_back(g);}
 std::vector<GpuTri> triangles;triangles.reserve(scene.obj.size());
 for(const auto& o:scene.obj){GpuTri t{};v3(t.p0,o.a);v3(t.p1,o.b);v3(t.p2,o.c);v3(t.n0,o.n0);v3(t.n1,o.n1);v3(t.n2,o.n2);t.material=o.mat;t.uv0[0]=o.uv0.x;t.uv0[1]=o.uv0.y;t.uv1[0]=o.uv1.x;t.uv1[1]=o.uv1.y;t.uv2[0]=o.uv2.x;t.uv2[1]=o.uv2.y;triangles.push_back(t);}
 std::vector<GpuMaterial> materials;materials.reserve(scene.mat.size());
 for(const auto& m:scene.mat){
  GpuMaterial g{};
  uint32_t rgb=0xb0b0b0u;
  if(!m.colorMip.empty()&&!m.colorMip.back().p.empty())rgb=m.colorMip.back().p[0];
  g.albedo[0]=float((rgb>>16)&255u)/255.f;
  g.albedo[1]=float((rgb>>8)&255u)/255.f;
  g.albedo[2]=float(rgb&255u)/255.f;
  float rough=0.65f;
  if(!m.roughnessMip.empty()&&!m.roughnessMip.back().p.empty())
   rough=float((m.roughnessMip.back().p[0]>>11)&31u)/31.f;
  g.roughnessScale=m.roughnessScale;
  g.metallic=std::clamp(m.metallic,0.f,1.f);
  g.transmission=float(m.transmissionPercent)/100.f;
  g.ior=m.refractiveIndex;
  materials.push_back(g);
 }
 if(materials.empty()||materials.size()>UINT32_MAX){error=L"Matériaux Compute absents ou trop nombreux";return false;}
 for(uint32_t index:scene.order)if(index>=triangles.size()){error=L"Index de primitive invalide";return false;}
 UINT flags=0;
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL level{};
 // Software WARP is opt-in for CI parity tests only; normal users always select hardware.
 const char* forceWarp=std::getenv("M3D_V16_FORCE_WARP");
 const D3D_DRIVER_TYPE driver=(forceWarp&&std::strcmp(forceWarp,"1")==0)
     ?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
 HRESULT hr=D3D11CreateDevice(nullptr,driver,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&device,&level,&ctx);
 if(FAILED(hr)||level<D3D_FEATURE_LEVEL_11_0){error=L"Compute Shader 5.0 non disponible sur cet adaptateur";return false;}
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
 auto shaderPath=std::filesystem::path(exe).parent_path()/L"shaders"/L"m3d_raytrace_compute.hlsl";
 ComPtr<ID3DBlob> bytecode,diagnostics;
 hr=D3DCompileFromFile(shaderPath.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,"CSMain","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&diagnostics);
 if(FAILED(hr)){error=L"Compilation compute HLSL impossible : "+shaderPath.wstring();return false;}
 ComPtr<ID3D11ComputeShader> shader;
 if(FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&shader))){error=L"CreateComputeShader impossible";return false;}
 ComPtr<ID3D11ShaderResourceView> srv0,srv1,srv2,srv3,srv4;
 if(!makeSrv(device.Get(),nodes,srv0)||!makeSrv(device.Get(),triangles,srv1)||!makeSrv(device.Get(),scene.order,srv2)||!makeSrv(device.Get(),materials,srv3)){error=L"Allocation BVH/matériaux GPU impossible";return false;}
 if(!makePbrTextureArray(device.Get(),scene.mat,srv4,error))return false;
 Params params{};v3(params.eye,cam.eye);params.tanHalf=cam.tanHalf;v3(params.forward,cam.f);params.aspect=cam.aspect;v3(params.right,cam.r);params.nearPlane=0.0001f;v3(params.up,cam.u);params.farPlane=100000.f;params.width=uint32_t(w);params.height=uint32_t(h);params.nodeCount=uint32_t(nodes.size());params.triangleCount=uint32_t(triangles.size());params.rootIndex=uint32_t(scene.root);params.maxTraversal=uint32_t(std::min<size_t>(1000000,nodes.size()*4+1));params.materialCount=uint32_t(materials.size());params.textureBaseSize=gpuTextureBaseSize;v3(params.sunDirection,sun);params.ambientStrength=0.60f;params.skyZenith[0]=skyZenith.x/255.f;params.skyZenith[1]=skyZenith.y/255.f;params.skyZenith[2]=skyZenith.z/255.f;params.skyHorizon[0]=skyHorizon.x/255.f;params.skyHorizon[1]=skyHorizon.y/255.f;params.skyHorizon[2]=skyHorizon.z/255.f;params.skyStrength=1.f;params.sunStrength=0.90f*std::max(.05f,daylight);
 D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Params);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=&params;ComPtr<ID3D11Buffer> constant;
 if(FAILED(device->CreateBuffer(&cb,&init,&constant))){error=L"Constantes GPU indisponibles";return false;}
 D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
 ComPtr<ID3D11Texture2D> output;ComPtr<ID3D11UnorderedAccessView> uav;
 if(FAILED(device->CreateTexture2D(&td,nullptr,&output))||FAILED(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav))){error=L"Texture compute GPU indisponible";return false;}
 td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> staging;
 if(FAILED(device->CreateTexture2D(&td,nullptr,&staging))){error=L"Texture de lecture GPU indisponible";return false;}
 D3D11_SAMPLER_DESC sampleDesc{};sampleDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampleDesc.AddressU=D3D11_TEXTURE_ADDRESS_WRAP;sampleDesc.AddressV=D3D11_TEXTURE_ADDRESS_WRAP;sampleDesc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sampleDesc.MinLOD=0;sampleDesc.MaxLOD=D3D11_FLOAT32_MAX;
 ComPtr<ID3D11SamplerState> sampler;
 if(FAILED(device->CreateSamplerState(&sampleDesc,&sampler))){error=L"Échantillonneur PBR GPU indisponible";return false;}
 ID3D11ShaderResourceView* srvs[]={srv0.Get(),srv1.Get(),srv2.Get(),srv3.Get(),srv4.Get()};ID3D11UnorderedAccessView* uavs[]={uav.Get()};ID3D11Buffer* cbuff[]={constant.Get()};ID3D11SamplerState* samplers[]={sampler.Get()};
 ctx->CSSetShader(shader.Get(),nullptr,0);ctx->CSSetShaderResources(0,5,srvs);ctx->CSSetSamplers(0,1,samplers);ctx->CSSetUnorderedAccessViews(0,1,uavs,nullptr);ctx->CSSetConstantBuffers(0,1,cbuff);
 ctx->Dispatch(UINT((w+7)/8),UINT((h+7)/8),1);
 ID3D11UnorderedAccessView* nullUav=nullptr;ID3D11ShaderResourceView* nullSrvs[5]={};ID3D11SamplerState* nullSampler=nullptr;ctx->CSSetUnorderedAccessViews(0,1,&nullUav,nullptr);ctx->CSSetShaderResources(0,5,nullSrvs);ctx->CSSetSamplers(0,1,&nullSampler);
 ctx->CopyResource(staging.Get(),output.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};
 if(FAILED(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped))){error=L"Lecture résultat compute impossible";return false;}
 pixels.resize(size_t(w)*size_t(h));
 for(int y=0;y<h;++y){const float* row=reinterpret_cast<const float*>(static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch);
 for(int x=0;x<w;++x){const float* p=row+size_t(x)*4;auto c=[](float f)->uint32_t{return uint32_t(std::max(0.f,std::min(255.f,f*255.f)));};pixels[size_t(y)*w+x]=0xff000000u|(c(p[0])<<16)|(c(p[1])<<8)|c(p[2]);}}
 ctx->Unmap(staging.Get(),0);
 stats.gpu=true;stats.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 return true;
}
}
#else
namespace m3d { bool renderCompute11Preview(const Scene&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"Compute D3D11 uniquement sous Windows";return false;} }
#endif
