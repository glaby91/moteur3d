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
#include <mutex>
#include <system_error>
using Microsoft::WRL::ComPtr;
namespace m3d {
namespace {
struct alignas(16) GpuNode { float mn[3]; uint32_t left; float mx[3]; uint32_t right; uint32_t first,count,pad0,pad1; };
struct alignas(16) GpuTri { float p0[3]; uint32_t material; float p1[3]; uint32_t pad0; float p2[3]; uint32_t pad1; float n0[3]; uint32_t pad2; float n1[3]; uint32_t pad3; float n2[3]; uint32_t pad4; float uv0[2],uv1[2],uv2[2],uvPad[2]; };
static_assert(sizeof(GpuTri)==128,"GPU triangle layout differs from HLSL");
// Source albedo/roughness/normal maps are uploaded separately to a texture array.
struct alignas(16) GpuMaterial { float albedo[3]; float roughnessScale; float metallic,transmission,ior,dielectricF0; };
struct alignas(16) GpuLight { float position[3];float radius;float color[3];float intensity; };
static_assert(sizeof(GpuLight)==32, "GPU point light layout mismatch");
static_assert(sizeof(GpuMaterial)==32, "GPU material buffer layout mismatch");
struct alignas(16) Params { float eye[3],tanHalf,forward[3],aspect,right[3],nearPlane,up[3],farPlane; uint32_t width,height,nodeCount,triangleCount,rootIndex,maxTraversal,materialCount,textureBaseSize; float sunDirection[3],ambientStrength; float skyZenith[3],skyStrength; float skyHorizon[3],sunStrength; float sunColor[3],skyFillStrength; uint32_t lightCount;float sceneLightScale,indirectFill;uint32_t gpuIndirectSamples; };
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

uint64_t hashScene(const Scene& scene) {
 uint64_t h=14695981039346656037ull;
 auto bytes=[&](const void* data,size_t length){
  const auto* p=static_cast<const unsigned char*>(data);
  for(size_t i=0;i<length;++i){h^=p[i];h*=1099511628211ull;}
 };
 auto scalar=[&](const auto& x){bytes(&x,sizeof(x));};
 scalar(scene.root);
 for(const auto& obj:scene.obj){
  // Hash only meaningful geometry values, not padding or container addresses.
  scalar(obj.kind);scalar(obj.mat);
  for(const V v:{obj.mn,obj.mx,obj.a,obj.b,obj.c,obj.n0,obj.n1,obj.n2,obj.uv0,obj.uv1,obj.uv2}){
   scalar(v.x);scalar(v.y);scalar(v.z);
  }
 }
 for(const auto& node:scene.node){
  for(const V v:{node.mn,node.mx}){scalar(v.x);scalar(v.y);scalar(v.z);}
  scalar(node.left);scalar(node.right);scalar(node.start);scalar(node.count);
 }
 if(!scene.order.empty())bytes(scene.order.data(),scene.order.size()*sizeof(scene.order[0]));
 scalar(scene.useLegacyWorldLights);
 for(const auto& l:scene.lights){
  scalar(l.p.x);scalar(l.p.y);scalar(l.p.z);
  scalar(l.radius);scalar(l.intensity);
  scalar(l.color.x);scalar(l.color.y);scalar(l.color.z);
 }
 for(const auto& m:scene.mat){
  bytes(m.name.data(),m.name.size());
  scalar(m.reflectivityPercent);scalar(m.transmissionPercent);
  scalar(m.refractiveIndex);scalar(m.metallic);scalar(m.roughnessScale);
  for(const auto& mip:m.colorMip){
   scalar(mip.size);
   if(!mip.p.empty())bytes(mip.p.data(),mip.p.size()*sizeof(uint32_t));
  }
  for(const auto* chain:{&m.normalMip,&m.roughnessMip}){
   for(const auto& mip:*chain){
    scalar(mip.size);
    if(!mip.p.empty())bytes(mip.p.data(),mip.p.size()*sizeof(uint16_t));
   }
  }
 }
 return h;
}
struct ComputeCache {
 std::mutex mutex;
 bool warp=false;
 uint64_t sceneHash=0;
 uint64_t sceneRevision=0;
 bool trackedScene=false;
 bool sceneReady=false;
 bool shaderTimeSet=false;
 std::filesystem::file_time_type shaderTime{};
 UINT w=0,h=0,nodeCount=0,triangleCount=0,materialCount=0,lightCount=0;
 ComPtr<ID3D11Device> device;
 ComPtr<ID3D11DeviceContext> ctx;
 ComPtr<ID3D11ComputeShader> shader;
 ComPtr<ID3D11ShaderResourceView> srv0,srv1,srv2,srv3,srv4,srv5;
 ComPtr<ID3D11SamplerState> sampler;
 ComPtr<ID3D11Buffer> constant;
 ComPtr<ID3D11Texture2D> output,staging;
 ComPtr<ID3D11UnorderedAccessView> uav;
 void reset(){
  ctx.Reset();device.Reset();shader.Reset();
  srv0.Reset();srv1.Reset();srv2.Reset();srv3.Reset();srv4.Reset();srv5.Reset();
  sampler.Reset();constant.Reset();output.Reset();staging.Reset();uav.Reset();
  sceneReady=false;shaderTimeSet=false;w=0;h=0;sceneHash=0;sceneRevision=0;trackedScene=false;
 }
};
ComputeCache& computeCache(){static ComputeCache cache;return cache;}

}
bool renderCompute11Preview(const Scene& scene,const Camera& cam,int w,int h,
                             std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
 const auto begin=std::chrono::steady_clock::now();
 if(w<=0||h<=0||w>8192||h>8192){error=L"Résolution Compute invalide";return false;}
 if(scene.root<0||scene.node.empty()||scene.order.empty()){error=L"BVH absent";return false;}
 if(scene.obj.size()>UINT32_MAX||scene.node.size()>UINT32_MAX||scene.order.size()>UINT32_MAX){
  error=L"Scène trop grande";return false;
 }
 for(const auto& o:scene.obj)if(o.kind!=Kind::Triangle){
  error=L"Compute nécessite des triangles";return false;
 }
 if(scene.mat.empty()){error=L"Aucun matériau";return false;}
 // The D3D11 immediate context is single-threaded here. Hold this mutex
 // for the entire render, staging readback included, to prevent state races.
 auto& c=computeCache();
 std::lock_guard<std::mutex> guard(c.mutex);
 const char* forceWarp=std::getenv("M3D_V16_FORCE_WARP");
 const bool warp=forceWarp&&std::strcmp(forceWarp,"1")==0;
 if(c.device.Get()==nullptr||c.warp!=warp){
  c.reset();
  const D3D_DRIVER_TYPE driver=warp?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
  D3D_FEATURE_LEVEL feature{};
  const HRESULT hr=D3D11CreateDevice(nullptr,driver,nullptr,0,nullptr,0,
                      D3D11_SDK_VERSION,&c.device,&feature,&c.ctx);
  if(FAILED(hr)||feature<D3D_FEATURE_LEVEL_11_0){
   c.reset();error=L"Direct3D11 Compute Shader 5.0 indisponible";return false;
  }
  c.warp=warp;
 }
 wchar_t executable[MAX_PATH]{};
 GetModuleFileNameW(nullptr,executable,MAX_PATH);
 const auto shaderPath=std::filesystem::path(executable).parent_path()/L"shaders"/L"m3d_raytrace_compute.hlsl";
 std::error_code fsError;
 const auto modified=std::filesystem::last_write_time(shaderPath,fsError);
 if(fsError){error=L"Shader HLSL absent : "+shaderPath.wstring();return false;}
 if(c.shader.Get()==nullptr||!c.shaderTimeSet||c.shaderTime!=modified){
  ComPtr<ID3DBlob> bytecode,diagnostics;
  const HRESULT hr=D3DCompileFromFile(shaderPath.c_str(),nullptr,
       D3D_COMPILE_STANDARD_FILE_INCLUDE,"CSMain","cs_5_0",
       D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&diagnostics);
  if(FAILED(hr)){
   error=L"Compilation du shader HLSL impossible : "+shaderPath.wstring();
   return false;
  }
  ComPtr<ID3D11ComputeShader> shader;
  if(FAILED(c.device->CreateComputeShader(bytecode->GetBufferPointer(),
                            bytecode->GetBufferSize(),nullptr,&shader))){
   error=L"CreateComputeShader impossible";return false;
  }
  c.shader=shader;c.shaderTime=modified;c.shaderTimeSet=true;
 }
 // V16 runtime scenes have unique identity + explicit modification revision:
 // no full scan of their 2048-pixel PBR mip chains on every camera movement.
 // External/legacy scenes with no identity retain the original full hash;
 // they can still be edited without an explicit revision.
 const bool tracked=scene.gpuCacheIdentity!=0;
 const uint64_t key=tracked?scene.gpuCacheIdentity:hashScene(scene);
 const uint64_t revision=tracked?scene.gpuCacheRevision:0;
 const bool sceneUpload=!c.sceneReady||c.trackedScene!=tracked||
                         c.sceneHash!=key||c.sceneRevision!=revision;
 if(sceneUpload){
  std::vector<GpuNode> nodes;nodes.reserve(scene.node.size());
  for(const auto& n:scene.node){
   if(n.count<0||n.start<0||uint64_t(n.start)+uint64_t(n.count)>scene.order.size()||
      (n.count==0&&(n.left<0||n.right<0))){
    error=L"BVH invalide";return false;
   }
   GpuNode g{};v3(g.mn,n.mn);v3(g.mx,n.mx);g.left=uint32_t(n.left);
   g.right=uint32_t(n.right);g.first=uint32_t(n.start);
   g.count=uint32_t(n.count);nodes.push_back(g);
  }
  std::vector<GpuTri> triangles;triangles.reserve(scene.obj.size());
  for(const auto& o:scene.obj){
   GpuTri t{};v3(t.p0,o.a);v3(t.p1,o.b);v3(t.p2,o.c);
   v3(t.n0,o.n0);v3(t.n1,o.n1);v3(t.n2,o.n2);t.material=o.mat;
   t.uv0[0]=o.uv0.x;t.uv0[1]=o.uv0.y;
   t.uv1[0]=o.uv1.x;t.uv1[1]=o.uv1.y;
   t.uv2[0]=o.uv2.x;t.uv2[1]=o.uv2.y;
   if(t.material>=scene.mat.size()){error=L"Triangle sans matériau";return false;}
   triangles.push_back(t);
  }
  for(const uint32_t i:scene.order)if(i>=triangles.size()){
   error=L"Index de triangle BVH invalide";return false;
  }
  std::vector<GpuMaterial> materials;materials.reserve(scene.mat.size());
  for(const auto& m:scene.mat){
   GpuMaterial g{};
   uint32_t rgb=0xb0b0b0u;
   if(!m.colorMip.empty()&&!m.colorMip.back().p.empty())rgb=m.colorMip.back().p[0];
   g.albedo[0]=float((rgb>>16)&255u)/255.f;
   g.albedo[1]=float((rgb>>8)&255u)/255.f;
   g.albedo[2]=float(rgb&255u)/255.f;
   g.roughnessScale=m.roughnessScale;
   g.metallic=std::clamp(m.metallic,0.f,1.f);
   g.transmission=float(m.transmissionPercent)/100.f;
   g.ior=m.refractiveIndex;
   g.dielectricF0=m.reflectivityPercent?
       std::clamp(float(m.reflectivityPercent)*.01f,.02f,.95f):.04f;
   materials.push_back(g);
  }
  // Only scene-authored lights belong to the V16 scene. This is the same
  // scene.lights list used by CPU offlinePbrShade().
  std::vector<GpuLight> lights;lights.reserve(std::max<size_t>(1,scene.lights.size()));
  for(const auto& l:scene.lights){
   GpuLight g{};v3(g.position,l.p);g.radius=l.radius;
   g.color[0]=l.color.x/255.f;
   g.color[1]=l.color.y/255.f;
   g.color[2]=l.color.z/255.f;
   g.intensity=l.intensity;
   lights.push_back(g);
  }
  if(lights.empty())lights.push_back(GpuLight{}); // valid nonempty SRV with lightCount=0
  ComPtr<ID3D11ShaderResourceView> s0,s1,s2,s3,s4,s5;
  if(!makeSrv(c.device.Get(),nodes,s0)||!makeSrv(c.device.Get(),triangles,s1)||
     !makeSrv(c.device.Get(),scene.order,s2)||!makeSrv(c.device.Get(),materials,s3)){
   error=L"Allocation BVH/matériaux GPU impossible";return false;
  }
  if(!makePbrTextureArray(c.device.Get(),scene.mat,s4,error))return false;
  if(!makeSrv(c.device.Get(),lights,s5)){
   error=L"Allocation des lumieres V16 GPU impossible";return false;
  }
  c.srv0=s0;c.srv1=s1;c.srv2=s2;c.srv3=s3;c.srv4=s4;c.srv5=s5;
  c.sceneHash=key;c.sceneRevision=revision;c.trackedScene=tracked;
  c.nodeCount=uint32_t(nodes.size());
  c.triangleCount=uint32_t(triangles.size());c.materialCount=uint32_t(materials.size());
  c.lightCount=uint32_t(scene.lights.size());
  c.sceneReady=true;
 }
 if(c.constant.Get()==nullptr){
  D3D11_BUFFER_DESC desc{};desc.ByteWidth=sizeof(Params);
  desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  if(FAILED(c.device->CreateBuffer(&desc,nullptr,&c.constant))){
   error=L"Allocation des constantes GPU impossible";return false;
  }
 }
 if(c.sampler.Get()==nullptr){
  D3D11_SAMPLER_DESC desc{};desc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  desc.AddressU=D3D11_TEXTURE_ADDRESS_WRAP;
  desc.AddressV=D3D11_TEXTURE_ADDRESS_WRAP;
  desc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
  desc.MinLOD=0;desc.MaxLOD=D3D11_FLOAT32_MAX;
  if(FAILED(c.device->CreateSamplerState(&desc,&c.sampler))){
   error=L"Samplers PBR GPU indisponibles";return false;
  }
 }
 if(c.output.Get()==nullptr||c.w!=UINT(w)||c.h!=UINT(h)){
  D3D11_TEXTURE2D_DESC desc{};desc.Width=UINT(w);desc.Height=UINT(h);
  desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
  desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
  ComPtr<ID3D11Texture2D> output,staging;
  ComPtr<ID3D11UnorderedAccessView> view;
  if(FAILED(c.device->CreateTexture2D(&desc,nullptr,&output))||
     FAILED(c.device->CreateUnorderedAccessView(output.Get(),nullptr,&view))){
   error=L"Allocation de l'image GPU impossible";return false;
  }
  desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;
  desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  if(FAILED(c.device->CreateTexture2D(&desc,nullptr,&staging))){
   error=L"Lecture de l'image GPU impossible";return false;
  }
  c.output=output;c.staging=staging;c.uav=view;c.w=UINT(w);c.h=UINT(h);
 }
 Params params{};v3(params.eye,cam.eye);params.tanHalf=cam.tanHalf;
 v3(params.forward,cam.f);params.aspect=cam.aspect;v3(params.right,cam.r);
 params.nearPlane=.0001f;v3(params.up,cam.u);params.farPlane=100000.f;
 params.width=uint32_t(w);params.height=uint32_t(h);
 params.nodeCount=c.nodeCount;params.triangleCount=c.triangleCount;
 params.rootIndex=uint32_t(scene.root);
 params.maxTraversal=uint32_t(std::min<size_t>(1000000,scene.node.size()*4+1));
 params.materialCount=c.materialCount;params.textureBaseSize=gpuTextureBaseSize;
 v3(params.sunDirection,sun);params.ambientStrength=0.f;
 params.skyZenith[0]=skyZenith.x/255.f;params.skyZenith[1]=skyZenith.y/255.f;
 params.skyZenith[2]=skyZenith.z/255.f;
 params.skyHorizon[0]=skyHorizon.x/255.f;params.skyHorizon[1]=skyHorizon.y/255.f;
 params.skyHorizon[2]=skyHorizon.z/255.f;
 params.skyStrength=1.f;
 // Match CPU offlinePbrShade() direct solar radiance exactly.
 params.sunStrength=2.8f*daylight;
 params.sunColor[0]=sunTint.x/255.f;
 params.sunColor[1]=sunTint.y/255.f;
 params.sunColor[2]=sunTint.z/255.f;
 params.skyFillStrength=0.f;
 params.lightCount=std::min<uint32_t>(c.lightCount,64u);
 params.sceneLightScale=.42f+.58f*(1.f-daylight);
 params.indirectFill=0.f;
 // Two real secondary hemisphere samples by default. Setting 0 disables the
 // GPU bounce for performance comparisons; 1/2/4 enables graded quality.
 // Kept bounded to avoid long D3D11 kernels and WDDM TDR on Intel iGPUs.
 params.gpuIndirectSamples=2u;
 if(const char* quality=std::getenv("M3D_V16_GPU_GI_SAMPLES")){
  const int n=std::atoi(quality);
  params.gpuIndirectSamples=uint32_t(std::clamp(n,0,4));
 }
 c.ctx->UpdateSubresource(c.constant.Get(),0,nullptr,&params,0,0);
 ID3D11ShaderResourceView* srvs[]={c.srv0.Get(),c.srv1.Get(),c.srv2.Get(),
                                    c.srv3.Get(),c.srv4.Get(),c.srv5.Get()};
 ID3D11UnorderedAccessView* uavs[]={c.uav.Get()};
 ID3D11Buffer* cbuff[]={c.constant.Get()};
 ID3D11SamplerState* samplers[]={c.sampler.Get()};
 c.ctx->CSSetShader(c.shader.Get(),nullptr,0);
 c.ctx->CSSetShaderResources(0,6,srvs);
 c.ctx->CSSetSamplers(0,1,samplers);
 c.ctx->CSSetUnorderedAccessViews(0,1,uavs,nullptr);
 c.ctx->CSSetConstantBuffers(0,1,cbuff);
 c.ctx->Dispatch(UINT((w+7)/8),UINT((h+7)/8),1);
 ID3D11UnorderedAccessView* noUav=nullptr;
 ID3D11ShaderResourceView* noSrvs[6]={};
 ID3D11SamplerState* noSampler=nullptr;
 c.ctx->CSSetUnorderedAccessViews(0,1,&noUav,nullptr);
 c.ctx->CSSetShaderResources(0,6,noSrvs);
 c.ctx->CSSetSamplers(0,1,&noSampler);
 c.ctx->CopyResource(c.staging.Get(),c.output.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};
 if(FAILED(c.ctx->Map(c.staging.Get(),0,D3D11_MAP_READ,0,&mapped))){
  error=L"Lecture du résultat GPU impossible";
  if(FAILED(c.device->GetDeviceRemovedReason()))c.reset();
  return false;
 }
 pixels.resize(size_t(w)*size_t(h));
 // RGBA8 typed UAV is supported by D3D11 feature level 11:
 // it cuts output/staging bandwidth by 4x versus RGBA32F without an extra
 // float-to-byte conversion pass on the CPU.
 for(int y=0;y<h;++y){
  const uint8_t* row=static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch;
  for(int x=0;x<w;++x){
   const uint8_t* p=row+size_t(x)*4;
   pixels[size_t(y)*w+x]=0xff000000u|
                           (uint32_t(p[0])<<16)|
                           (uint32_t(p[1])<<8)|
                            uint32_t(p[2]);
  }
 }
 c.ctx->Unmap(c.staging.Get(),0);
 stats.gpu=true;
 stats.ms=std::chrono::duration<double,std::milli>(
              std::chrono::steady_clock::now()-begin).count();
 return true;
}
}

#else
namespace m3d { bool renderCompute11Preview(const Scene&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"Compute D3D11 uniquement sous Windows";return false;} }
#endif
