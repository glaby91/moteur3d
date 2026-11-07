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
using Microsoft::WRL::ComPtr;
namespace m3d {
namespace {
struct alignas(16) GpuNode { float mn[3]; uint32_t left; float mx[3]; uint32_t right; uint32_t first,count,pad0,pad1; };
struct alignas(16) GpuObject {
 float mn[3]; uint32_t kind;
 float mx[3]; uint32_t material;
 float a[3]; float r;
 float b[3]; float r1;
 float c[3]; float k;
 float n0[3]; float len;
 float n1[3]; float len2;
 float n2[3]; float pad0;
 float center[3]; float pad1;
 float core[3]; float pad2;
 float uv0[3]; float pad3;
 float uv1[3]; float pad4;
 float uv2[3]; float pad5;
};
struct alignas(16) GpuMaterial { float shade[4]; float params[4]; };
static_assert(sizeof(GpuObject)==208,"GpuObject/HLSL layout mismatch");
static_assert(sizeof(GpuMaterial)==32,"GpuMaterial/HLSL layout mismatch");
struct alignas(16) Params { float eye[3],tanHalf,forward[3],aspect,right[3],nearPlane,up[3],farPlane; uint32_t width,height,nodeCount,objectCount,rootIndex,maxTraversal,reserved0,reserved1; float sunDir[3],ambient; float skyZenith[3],sunIntensity; float skyHorizon[3],reservedSky; };
void v3(float* dst,V v){dst[0]=v.x;dst[1]=v.y;dst[2]=v.z;}
template<class T> bool makeSrv(ID3D11Device* d,const std::vector<T>& values,ComPtr<ID3D11ShaderResourceView>& srv){
 if(values.empty())return false;
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(values.size()*sizeof(T));bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_SHADER_RESOURCE;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;bd.StructureByteStride=sizeof(T);
 D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=values.data();ComPtr<ID3D11Buffer> buf;
 if(FAILED(d->CreateBuffer(&bd,&sd,&buf)))return false;
 D3D11_SHADER_RESOURCE_VIEW_DESC vd{};vd.Format=DXGI_FORMAT_UNKNOWN;vd.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;vd.Buffer.NumElements=UINT(values.size());
 return SUCCEEDED(d->CreateShaderResourceView(buf.Get(),&vd,&srv));
}
}
bool validateCompute11Shader(std::wstring& error){
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
 const auto shaderPath=std::filesystem::path(exe).parent_path()/L"shaders"/L"m3d_raytrace_compute.hlsl";
 ComPtr<ID3DBlob> bytecode,diagnostics;
 const HRESULT hr=D3DCompileFromFile(shaderPath.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                      "CSMain","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&diagnostics);
 if(FAILED(hr)){
  error=L"Shader HLSL invalide : "+shaderPath.wstring();
  if(diagnostics&&diagnostics->GetBufferPointer()){
   const char* msg=static_cast<const char*>(diagnostics->GetBufferPointer());
   for(size_t i=0;i<std::min<size_t>(diagnostics->GetBufferSize(),3000);++i)error+=wchar_t(uint8_t(msg[i]));
  }
  return false;
 }
 return true;
}
// Persist device, program, uploaded scene and texture atlases on Qt's dedicated
// render worker thread. The previous implementation recreated EVERYTHING
// for each frame; it heavily penalised Intel integrated GPUs.
struct ComputeCache {
 ComPtr<ID3D11Device> device;
 ComPtr<ID3D11DeviceContext> context;
 ComPtr<ID3D11ComputeShader> shader;
 ComPtr<ID3D11SamplerState> sampler;
 ComPtr<ID3D11Buffer> constants;
 std::array<ComPtr<ID3D11ShaderResourceView>,7> srv{};
 ComPtr<ID3D11Texture2D> output,staging;
 ComPtr<ID3D11UnorderedAccessView> outputUav;
 const Obj* sceneKey=nullptr;
 size_t count=0,matCount=0,nodeCount=0;
 int width=0,height=0;
 bool ensureDevice(std::wstring& error){
  if(device)return true;
  D3D_FEATURE_LEVEL level{};
  HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,
                               D3D11_SDK_VERSION,&device,&level,&context);
  if(FAILED(hr)||level<D3D_FEATURE_LEVEL_11_0){
   error=L"GPU D3D11 Feature Level 11_0 requis";return false;
  }
  wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
  const auto shaderPath=std::filesystem::path(exe).parent_path()/L"shaders"/L"m3d_raytrace_compute.hlsl";
  ComPtr<ID3DBlob> code,diag;
  hr=D3DCompileFromFile(shaderPath.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,
      "CSMain","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&diag);
  if(FAILED(hr)){
   error=L"Shader Compute PBR invalide : "+shaderPath.wstring();
   if(diag&&diag->GetBufferPointer()){
    const char* str=static_cast<const char*>(diag->GetBufferPointer());
    for(size_t i=0;i<std::min<size_t>(diag->GetBufferSize(),2048);i++)error+=wchar_t(uint8_t(str[i]));
   }
   return false;
  }
  if(FAILED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader))){
    error=L"Creation de Compute Shader impossible";return false;
  }
  D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Params);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  if(FAILED(device->CreateBuffer(&cb,nullptr,&constants))){
    error=L"Allocation des constantes Compute impossible";return false;
  }
  D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxLOD=D3D11_FLOAT32_MAX;
  if(FAILED(device->CreateSamplerState(&sd,&sampler))){
    error=L"Creation du sampler PBR impossible";return false;
  }
  return true;
 }
 bool ensureScene(const Scene& scene,std::wstring& error){
  if(sceneKey==scene.obj.data()&&count==scene.obj.size()&&
      matCount==scene.mat.size()&&nodeCount==scene.node.size()&&srv[6])return true;
  for(auto& s:srv)s.Reset();
  sceneKey=nullptr;
  if(scene.root<0||scene.node.empty()||scene.order.empty()||
     scene.obj.empty()||scene.mat.empty()||scene.mat.size()>2048||
     scene.obj.size()>UINT32_MAX||scene.node.size()>UINT32_MAX){
    error=L"Scene ou BVH incompatible avec Compute GPU";return false;
  }
  std::vector<GpuNode> nodes;nodes.reserve(scene.node.size());
  for(const auto& n:scene.node){
   if(n.count<0||n.start<0||uint64_t(n.start)+uint64_t(n.count)>scene.order.size()||
      (n.count==0&&(n.left<0||n.right<0))){
    error=L"BVH invalide pour le GPU";return false;
   }
   GpuNode g{};v3(g.mn,n.mn);v3(g.mx,n.mx);g.left=uint32_t(n.left);
   g.right=uint32_t(n.right);g.first=uint32_t(n.start);g.count=uint32_t(n.count);
   nodes.push_back(g);
  }
  std::vector<GpuObject> objects;objects.reserve(scene.obj.size());
  for(const auto& o:scene.obj){
   GpuObject g{};v3(g.mn,o.mn);v3(g.mx,o.mx);g.kind=uint32_t(o.kind);g.material=o.mat;
   v3(g.a,o.a);g.r=o.r;v3(g.b,o.b);g.r1=o.r1;v3(g.c,o.c);g.k=o.k;
   v3(g.n0,o.n0);g.len=o.len;v3(g.n1,o.n1);g.len2=o.len2;
   v3(g.n2,o.n2);v3(g.center,o.center);v3(g.core,o.core);
   v3(g.uv0,o.uv0);v3(g.uv1,o.uv1);v3(g.uv2,o.uv2);
   objects.push_back(g);
  }
  for(uint32_t id:scene.order)if(id>=scene.obj.size()){
   error=L"Index BVH incorrect";return false;
  }
  std::vector<GpuMaterial> materials;materials.reserve(scene.mat.size());
  const UINT atlasSize=512;
  const size_t pixelCount=size_t(atlasSize)*atlasSize;
  const UINT matN=UINT(scene.mat.size());
  std::vector<std::vector<uint32_t>> colors(matN),normals(matN);
  std::vector<std::vector<uint8_t>> roughs(matN);
  std::vector<D3D11_SUBRESOURCE_DATA> subColors(matN),subNormals(matN),subRoughs(matN);
  for(UINT mIndex=0;mIndex<matN;mIndex++){
   const Material& m=scene.mat[mIndex];
   GpuMaterial g{};
   const std::string& name=m.name;
   g.shade[3]=float(m.reflectivityPercent)/100.f;
   g.params[0]=name=="concrete"?.34f:name=="stone"||name=="brick"?.28f:
       name=="glass"?.16f:name=="mural"?.075f:name=="seatVinylBrown"?.85f:.65f;
   g.params[1]=float(m.transmissionPercent)/100.f;
   materials.push_back(g);
   colors[mIndex].resize(pixelCount);normals[mIndex].resize(pixelCount);
   roughs[mIndex].resize(pixelCount);
   const ColorLevel* co=m.colorMip.empty()?nullptr:&m.colorMip.front();
   const Level* old=m.mip.empty()?nullptr:&m.mip.front();
   const Level* nm=m.normalMip.empty()?nullptr:&m.normalMip.front();
   const Level* rm=m.roughnessMip.empty()?nullptr:&m.roughnessMip.front();
   for(UINT y=0;y<atlasSize;y++)for(UINT x=0;x<atlasSize;x++){
    const size_t dst=size_t(y)*atlasSize+x;
    const auto sampleIndex=[&](uint32_t size)->size_t{
     if(!size)return 0;
     const UINT sx=std::min(size-1,uint32_t((uint64_t(x)*size)/atlasSize));
     const UINT sy=std::min(size-1,uint32_t((uint64_t(y)*size)/atlasSize));
     return size_t(sy)*size+sx;
    };
    uint32_t rgb=0x777777u;
    if(co&&!co->p.empty())rgb=co->p[sampleIndex(co->size)]&0xffffffu;
    else if(old&&!old->p.empty()){
     const uint16_t v=old->p[sampleIndex(old->size)];
     rgb=((((v>>11)&31u)*255u/31u)<<16)|((((v>>5)&63u)*255u/63u)<<8)|((v&31u)*255u/31u);
    }
    const uint32_t r=(rgb>>16)&255u,gc=(rgb>>8)&255u,bl=rgb&255u;
    colors[mIndex][dst]=0xff000000u|(bl<<16)|(gc<<8)|r;
    uint32_t packedNormal=0xffFF8080u; // memory byte order: 80 80 FF FF
    if(nm&&!nm->p.empty()){
     const uint16_t v=nm->p[sampleIndex(nm->size)];
     const uint32_t nr=((v>>11)&31u)*255u/31u;
     const uint32_t ng=((v>>5)&63u)*255u/63u;
     const uint32_t nb=(v&31u)*255u/31u;
     packedNormal=0xff000000u|(nb<<16)|(ng<<8)|nr;
    }
    normals[mIndex][dst]=packedNormal;
    roughs[mIndex][dst]=rm&&!rm->p.empty()
      ? uint8_t((((rm->p[sampleIndex(rm->size)]>>11)&31u)*255u)/31u)
      : uint8_t(186);
   }
   subColors[mIndex].pSysMem=colors[mIndex].data();subColors[mIndex].SysMemPitch=atlasSize*4;
   subNormals[mIndex].pSysMem=normals[mIndex].data();subNormals[mIndex].SysMemPitch=atlasSize*4;
   subRoughs[mIndex].pSysMem=roughs[mIndex].data();subRoughs[mIndex].SysMemPitch=atlasSize;
  }
  if(!makeSrv(device.Get(),nodes,srv[0])||
     !makeSrv(device.Get(),objects,srv[1])||
     !makeSrv(device.Get(),scene.order,srv[2])||
     !makeSrv(device.Get(),materials,srv[3])){
     error=L"Allocation des buffers GPU (BVH/objets/materiaux) impossible";return false;
  }
  const auto atlas=[&](DXGI_FORMAT format,const std::vector<D3D11_SUBRESOURCE_DATA>& src,
                       ComPtr<ID3D11ShaderResourceView>& out)->bool{
   D3D11_TEXTURE2D_DESC td{};td.Width=atlasSize;td.Height=atlasSize;td.ArraySize=matN;
   td.MipLevels=1;td.Format=format;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;
   td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   ComPtr<ID3D11Texture2D> tex;
   if(FAILED(device->CreateTexture2D(&td,src.data(),&tex)))return false;
   D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=format;
   sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
   sd.Texture2DArray.MipLevels=1;sd.Texture2DArray.ArraySize=matN;
   return SUCCEEDED(device->CreateShaderResourceView(tex.Get(),&sd,&out));
  };
  if(!atlas(DXGI_FORMAT_R8G8B8A8_UNORM,subColors,srv[4])||
     !atlas(DXGI_FORMAT_R8G8B8A8_UNORM,subNormals,srv[5])||
     !atlas(DXGI_FORMAT_R8_UNORM,subRoughs,srv[6])){
    error=L"Allocation des atlases PBR sur GPU impossible";return false;
  }
  sceneKey=scene.obj.data();count=scene.obj.size();nodeCount=scene.node.size();matCount=scene.mat.size();
  return true;
 }
 bool ensureTarget(int w,int h,std::wstring& error){
  if(output&&outputUav&&staging&&width==w&&height==h)return true;
  output.Reset();outputUav.Reset();staging.Reset();width=0;height=0;
  D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.ArraySize=1;
  td.MipLevels=1;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
  td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
  if(FAILED(device->CreateTexture2D(&td,nullptr,&output))||
     FAILED(device->CreateUnorderedAccessView(output.Get(),nullptr,&outputUav))){
    error=L"Framebuffer GPU Compute indisponible";return false;
  }
  td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
  if(FAILED(device->CreateTexture2D(&td,nullptr,&staging))){
    error=L"Readback Compute indisponible";return false;
  }
  width=w;height=h;return true;
 }
};
bool renderCompute11Preview(const Scene& scene,const Camera& cam,int w,int h,
   std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
 const auto start=std::chrono::steady_clock::now();
 if(w<=0||h<=0||w>8192||h>8192){error=L"Resolution GPU invalide";return false;}
 static thread_local ComputeCache cache;
 if(!cache.ensureDevice(error)||!cache.ensureScene(scene,error)||
    !cache.ensureTarget(w,h,error))return false;
 Params p{};v3(p.eye,cam.eye);p.tanHalf=cam.tanHalf;
 v3(p.forward,cam.f);p.aspect=cam.aspect;
 v3(p.right,cam.r);p.nearPlane=.0001f;
 v3(p.up,cam.u);p.farPlane=100000.f;
 p.width=uint32_t(w);p.height=uint32_t(h);
 p.nodeCount=uint32_t(scene.node.size());p.objectCount=uint32_t(scene.obj.size());
 p.rootIndex=uint32_t(scene.root);
 p.maxTraversal=uint32_t(std::min<size_t>(1000000,scene.node.size()*4+1));
 v3(p.sunDir,norm(sun));p.ambient=clampf(daylight,.05f,1.f);
 v3(p.skyZenith,skyZenith/255.f);v3(p.skyHorizon,skyHorizon/255.f);
 p.sunIntensity=clampf(daylight,0.f,1.f);
 cache.context->UpdateSubresource(cache.constants.Get(),0,nullptr,&p,0,0);
 ID3D11ShaderResourceView* srvs[7]={};
 for(size_t i=0;i<7;i++)srvs[i]=cache.srv[i].Get();
 ID3D11UnorderedAccessView* uavs[]={cache.outputUav.Get()};
 ID3D11Buffer* constant[]={cache.constants.Get()};
 ID3D11SamplerState* samplers[]={cache.sampler.Get()};
 cache.context->CSSetShader(cache.shader.Get(),nullptr,0);
 cache.context->CSSetShaderResources(0,7,srvs);
 cache.context->CSSetUnorderedAccessViews(0,1,uavs,nullptr);
 cache.context->CSSetConstantBuffers(0,1,constant);
 cache.context->CSSetSamplers(0,1,samplers);
 cache.context->Dispatch(UINT((w+7)/8),UINT((h+7)/8),1);
 ID3D11UnorderedAccessView* nullUav=nullptr;
 ID3D11ShaderResourceView* nullSrvs[7]={};
 cache.context->CSSetUnorderedAccessViews(0,1,&nullUav,nullptr);
 cache.context->CSSetShaderResources(0,7,nullSrvs);
 cache.context->CopyResource(cache.staging.Get(),cache.output.Get());
 D3D11_MAPPED_SUBRESOURCE mapped{};
 if(FAILED(cache.context->Map(cache.staging.Get(),0,D3D11_MAP_READ,0,&mapped))){
  error=L"Lecture GPU Compute impossible";return false;
 }
 pixels.resize(size_t(w)*size_t(h));
 for(int y=0;y<h;y++){
  const uint8_t* row=static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch;
  for(int x=0;x<w;x++){
   const uint8_t* px=row+size_t(x)*4;
   pixels[size_t(y)*w+x]=0xff000000u|
       (uint32_t(px[0])<<16)|(uint32_t(px[1])<<8)|uint32_t(px[2]);
  }
 }
 cache.context->Unmap(cache.staging.Get(),0);
 stats.gpu=true;stats.ms=std::chrono::duration<double,std::milli>(
    std::chrono::steady_clock::now()-start).count();
 stats.tests=uint64_t(w)*uint64_t(h);
 return true;
}
}
#else
namespace m3d { bool validateCompute11Shader(std::wstring& error){error=L"Compute D3D11 uniquement sous Windows";return false;} bool renderCompute11Preview(const Scene&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"Compute D3D11 uniquement sous Windows";return false;} }
#endif
