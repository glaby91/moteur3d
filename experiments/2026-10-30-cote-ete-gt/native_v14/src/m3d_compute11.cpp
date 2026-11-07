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
};
struct alignas(16) GpuMaterial { float shade[4]; };
static_assert(sizeof(GpuObject)==160,"GpuObject/HLSL layout mismatch");
static_assert(sizeof(GpuMaterial)==16,"GpuMaterial/HLSL layout mismatch");
struct alignas(16) Params { float eye[3],tanHalf,forward[3],aspect,right[3],nearPlane,up[3],farPlane; uint32_t width,height,nodeCount,objectCount,rootIndex,maxTraversal,reserved0,reserved1; float sunDir[3],ambient; };
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
bool renderCompute11Preview(const Scene& scene,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
 const auto start=std::chrono::steady_clock::now();
 if(w<=0||h<=0||w>8192||h>8192){error=L"Résolution compute invalide";return false;}
 if(scene.root<0||scene.node.empty()||scene.order.empty()){error=L"BVH absent";return false;}
 if(scene.obj.size()>UINT32_MAX||scene.node.size()>UINT32_MAX||scene.order.size()>UINT32_MAX){error=L"Scène trop grande";return false;}
 std::vector<GpuNode> nodes;nodes.reserve(scene.node.size());
 for(const auto& n:scene.node){GpuNode g{};v3(g.mn,n.mn);v3(g.mx,n.mx);g.left=uint32_t(n.left);g.right=uint32_t(n.right);g.first=uint32_t(n.start);g.count=uint32_t(n.count);
 if(n.count<0||n.start<0||uint64_t(n.start)+uint64_t(n.count)>scene.order.size()||(n.count==0&&(n.left<0||n.right<0))){error=L"BVH invalide";return false;}nodes.push_back(g);}
 std::vector<GpuObject> objects;objects.reserve(scene.obj.size());
 for(const auto& o:scene.obj){
  GpuObject g{};v3(g.mn,o.mn);v3(g.mx,o.mx);g.kind=uint32_t(o.kind);g.material=o.mat;
  v3(g.a,o.a);g.r=o.r;v3(g.b,o.b);g.r1=o.r1;v3(g.c,o.c);g.k=o.k;
  v3(g.n0,o.n0);g.len=o.len;v3(g.n1,o.n1);g.len2=o.len2;v3(g.n2,o.n2);
  v3(g.center,o.center);v3(g.core,o.core);objects.push_back(g);
 }
 if(objects.empty()||scene.mat.empty()){error=L"Scène Compute sans géométrie ou matériaux";return false;}
 std::vector<GpuMaterial> materials;materials.reserve(scene.mat.size());
 for(const auto& m:scene.mat){
  GpuMaterial g{};uint64_t rr=0,gg=0,bb=0,count=0;
  if(!m.colorMip.empty()&&!m.colorMip.front().p.empty()){
   const auto& image=m.colorMip.front().p;const size_t step=std::max<size_t>(1,image.size()/128);
   for(size_t i=0;i<image.size();i+=step){const auto v=image[i];rr+=(v>>16)&255;gg+=(v>>8)&255;bb+=v&255;++count;}
  }else if(!m.mip.empty()&&!m.mip.front().p.empty()){
   const auto& image=m.mip.front().p;const size_t step=std::max<size_t>(1,image.size()/128);
   for(size_t i=0;i<image.size();i+=step){const auto v=image[i];rr+=((v>>11)&31)*255/31;gg+=((v>>5)&63)*255/63;bb+=(v&31)*255/31;++count;}
  }
  if(!count)count=1;
  g.shade[0]=float(rr)/float(count*255);g.shade[1]=float(gg)/float(count*255);g.shade[2]=float(bb)/float(count*255);
  g.shade[3]=float(m.reflectivityPercent)/100.f;materials.push_back(g);
 }
 for(uint32_t index:scene.order)if(index>=objects.size()){error=L"Index de primitive invalide";return false;}
 UINT flags=0;
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;D3D_FEATURE_LEVEL level{};
 HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,nullptr,0,D3D11_SDK_VERSION,&device,&level,&ctx);
 if(FAILED(hr)||level<D3D_FEATURE_LEVEL_11_0){error=L"Compute Shader 5.0 non disponible sur cet adaptateur";return false;}
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
 auto shaderPath=std::filesystem::path(exe).parent_path()/L"shaders"/L"m3d_raytrace_compute.hlsl";
 ComPtr<ID3DBlob> bytecode,diagnostics;
 hr=D3DCompileFromFile(shaderPath.c_str(),nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,"CSMain","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&diagnostics);
 if(FAILED(hr)){error=L"Compilation compute HLSL impossible : "+shaderPath.wstring();if(diagnostics&&diagnostics->GetBufferPointer()){auto msg=static_cast<const char*>(diagnostics->GetBufferPointer());const size_t n=std::min<size_t>(diagnostics->GetBufferSize(),1600);for(size_t i=0;i<n;++i)error+=wchar_t(uint8_t(msg[i]));}return false;}
 ComPtr<ID3D11ComputeShader> shader;
 if(FAILED(device->CreateComputeShader(bytecode->GetBufferPointer(),bytecode->GetBufferSize(),nullptr,&shader))){error=L"CreateComputeShader impossible";return false;}
 ComPtr<ID3D11ShaderResourceView> srv0,srv1,srv2,srv3;
 if(!makeSrv(device.Get(),nodes,srv0)||!makeSrv(device.Get(),objects,srv1)||!makeSrv(device.Get(),scene.order,srv2)||!makeSrv(device.Get(),materials,srv3)){error=L"Allocation des buffers BVH GPU impossible";return false;}
 Params params{};v3(params.eye,cam.eye);params.tanHalf=cam.tanHalf;v3(params.forward,cam.f);params.aspect=cam.aspect;v3(params.right,cam.r);params.nearPlane=0.0001f;v3(params.up,cam.u);params.farPlane=100000.f;params.width=uint32_t(w);params.height=uint32_t(h);params.nodeCount=uint32_t(nodes.size());params.objectCount=uint32_t(objects.size());v3(params.sunDir,norm(sun));params.ambient=std::max(.10f,std::min(1.f,daylight));params.rootIndex=uint32_t(scene.root);params.maxTraversal=uint32_t(std::min<size_t>(1000000,nodes.size()*4+1));
 D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Params);cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA init{};init.pSysMem=&params;ComPtr<ID3D11Buffer> constant;
 if(FAILED(device->CreateBuffer(&cb,&init,&constant))){error=L"Constantes GPU indisponibles";return false;}
 D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
 ComPtr<ID3D11Texture2D> output;ComPtr<ID3D11UnorderedAccessView> uav;
 if(FAILED(device->CreateTexture2D(&td,nullptr,&output))||FAILED(device->CreateUnorderedAccessView(output.Get(),nullptr,&uav))){error=L"Texture compute GPU indisponible";return false;}
 td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
 ComPtr<ID3D11Texture2D> staging;
 if(FAILED(device->CreateTexture2D(&td,nullptr,&staging))){error=L"Texture de lecture GPU indisponible";return false;}
 ID3D11ShaderResourceView* srvs[]={srv0.Get(),srv1.Get(),srv2.Get(),srv3.Get()};ID3D11UnorderedAccessView* uavs[]={uav.Get()};ID3D11Buffer* cbuff[]={constant.Get()};
 ctx->CSSetShader(shader.Get(),nullptr,0);ctx->CSSetShaderResources(0,4,srvs);ctx->CSSetUnorderedAccessViews(0,1,uavs,nullptr);ctx->CSSetConstantBuffers(0,1,cbuff);
 ctx->Dispatch(UINT((w+7)/8),UINT((h+7)/8),1);
 ID3D11UnorderedAccessView* nullUav=nullptr;ID3D11ShaderResourceView* nullSrvs[4]={};ctx->CSSetUnorderedAccessViews(0,1,&nullUav,nullptr);ctx->CSSetShaderResources(0,4,nullSrvs);
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
