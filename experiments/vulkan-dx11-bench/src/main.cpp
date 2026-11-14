// CSE WARP V17 research: isolated Vulkan vs Direct3D11 COMPUTE microbenchmark.
// This is not a benchmark of the production V17 raster renderer or its PBR scene.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
using Clock=std::chrono::steady_clock;
static void must(bool ok,const std::string& reason){if(!ok)throw std::runtime_error(reason);}
static void vkc(VkResult r,const char* what){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(what)+" VkResult="+std::to_string(int(r)));}
static void dxc(HRESULT hr,const char* what){if(FAILED(hr))throw std::runtime_error(std::string(what)+" HRESULT="+std::to_string(static_cast<unsigned>(hr)));}
static std::string utf8(const wchar_t* s){if(!s)return "";int n=WideCharToMultiByte(CP_UTF8,0,s,-1,nullptr,0,nullptr,nullptr);std::string out(size_t(std::max(1,n)),0);WideCharToMultiByte(CP_UTF8,0,s,-1,out.data(),n,nullptr,nullptr);if(!out.empty()&&out.back()==0)out.pop_back();return out;}
static std::string esc(const std::string& s){std::string o;for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=char(c);}else if(c<32){o+=' ';}else o+=char(c);}return o;}
static uint64_t fnv(const std::vector<uint32_t>& x){uint64_t h=14695981039346656037ull;for(uint32_t v:x)for(int i=0;i<4;++i){h^=(v>>(i*8))&255u;h*=1099511628211ull;}return h;}
static std::string hex(uint64_t x){char buf[40];sprintf_s(buf,"%016llx",static_cast<unsigned long long>(x));return buf;}
struct Params{uint32_t w=1366,h=768,loops=32,seed=0xC5E17201u;};
struct Sample{std::string api;int index=0;double gpuMs=-1,wallMs=-1;uint64_t checksum=0;double recordMs=0,submitMs=0,waitMs=0,invalidateMs=0,copyCpuMs=0;};
struct Config{Params p;int warmup=5,repeat=30,vkInFlight=1;std::string api="both",output="vulkan_dx11_result",vkMode="legacy";bool probe=false,allowSoftware=false,warp=false,vkReuse=true;};
static Config parse(int argc,char**argv){Config c;for(int i=1;i<argc;++i){std::string k=argv[i];
 auto val=[&](){must(i+1<argc,"Missing value for "+k);return std::string(argv[++i]);};
 if(k=="--width")c.p.w=uint32_t(std::stoul(val()));else if(k=="--height")c.p.h=uint32_t(std::stoul(val()));
 else if(k=="--loops")c.p.loops=uint32_t(std::stoul(val()));else if(k=="--seed")c.p.seed=uint32_t(std::stoul(val()));
 else if(k=="--warmup")c.warmup=std::stoi(val());else if(k=="--repeat")c.repeat=std::stoi(val());
 else if(k=="--api")c.api=val();else if(k=="--output")c.output=val();else if(k=="--probe")c.probe=true;
 else if(k=="--vk-mode")c.vkMode=val();else if(k=="--vk-inflight")c.vkInFlight=std::stoi(val());
 else if(k=="--vk-record")c.vkReuse=false;
 else if(k=="--allow-software")c.allowSoftware=true;else if(k=="--warp")c.warp=true;
 else if(k=="--help"){std::cout<<"CSE WARP Vulkan vs D3D11 COMPUTE microbenchmark (NOT V17 Raster)\n"
 <<"--probe | --api both|vulkan|dx11 --width 1366 --height 768 --loops 32 --warmup 5 --repeat 30 --output prefix\n"
 <<"--vk-mode legacy|cached|direct|compute --vk-inflight 1|2|3 --vk-record (disable reusable commands)\n"
 <<"cached: HOST_CACHED staging, persistent mapping; direct: UMA accessible output; compute: no per-run readback\n"
 <<"--allow-software: permit CPU Vulkan ICD (diagnostic ONLY). --warp: D3D11 WARP (diagnostic ONLY).\n";exit(0);}
 else throw std::runtime_error("Unknown argument: "+k);
 }
 must(c.api=="both"||c.api=="vulkan"||c.api=="dx11","Invalid --api");
 must(c.vkMode=="legacy"||c.vkMode=="cached"||c.vkMode=="direct"||c.vkMode=="compute","Invalid --vk-mode");
 must(c.vkInFlight>=1&&c.vkInFlight<=3,"--vk-inflight must be 1,2 or 3");
 must(c.vkInFlight==1||c.api=="vulkan","Pipelined tests require --api vulkan (not paired)");
 must(c.vkInFlight==1||c.vkMode!="legacy","Legacy mode has no async ring; select cached/direct/compute");
 must(c.vkMode!="compute"||c.api=="vulkan","Compute-only mode has no per-dispatch readback: use --api vulkan");
 must(c.p.w>0&&c.p.h>0&&c.p.w<=4096&&c.p.h<=4096&&uint64_t(c.p.w)*c.p.h<=8000000,"Invalid image size");
 must(c.p.loops>0&&c.p.loops<=512&&c.warmup>=0&&c.warmup<=50&&c.repeat>=1&&c.repeat<=500,"Invalid workload");
 must(c.api!="both"||(!c.warp&&!c.allowSoftware),"Do not compare different software ICDs in paired mode");
 return c;}

struct AdapterInfo{ComPtr<IDXGIAdapter1> adapter;DXGI_ADAPTER_DESC1 desc{};};
static std::vector<AdapterInfo> dxgiAdapters(){std::vector<AdapterInfo> all;ComPtr<IDXGIFactory1> f;dxc(CreateDXGIFactory1(IID_PPV_ARGS(&f)),"DXGI factory");for(UINT n=0;;++n){AdapterInfo x;HRESULT hr=f->EnumAdapters1(n,&x.adapter);if(hr==DXGI_ERROR_NOT_FOUND)break;dxc(hr,"EnumAdapters1");dxc(x.adapter->GetDesc1(&x.desc),"GetDesc1");all.push_back(std::move(x));}return all;}
static const char* kHlsl=R"HLSL(
cbuffer Settings : register(b0) { uint width; uint height; uint loops; uint seed; };
RWStructuredBuffer<uint> output : register(u0);
[numthreads(16,16,1)]
void main(uint3 gid : SV_DispatchThreadID) {
 if(gid.x>=width||gid.y>=height)return;
 uint index=gid.y*width+gid.x;
 uint v=index^seed;
 [loop]for(uint n=0;n<loops;++n){
  v^=v<<13;v^=v>>17;v^=v<<5;
  v=v*1664525u+1013904223u;
 }
 output[index]=v;
}
)HLSL";
class DxBench{
 ComPtr<ID3D11Device> d;ComPtr<ID3D11DeviceContext> ctx;
 ComPtr<ID3D11ComputeShader> shader;ComPtr<ID3D11Buffer> gpu,staging,cb;
 ComPtr<ID3D11UnorderedAccessView> uav;
 ComPtr<ID3D11Query> disjoint,qStart,qEnd;
 std::vector<uint32_t> pixels;
 Params params;
public:
 std::string deviceName;uint32_t vendor=0,deviceId=0;bool software=false;
 DxBench(const Params& p,uint32_t matchVendor,uint32_t matchDevice,bool useWarp):params(p),pixels(size_t(p.w)*p.h){
  ID3D11Device* raw=nullptr;ID3D11DeviceContext* rawctx=nullptr;D3D_FEATURE_LEVEL feature{};
  const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
  ComPtr<IDXGIAdapter1> selected;
  if(useWarp){dxc(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,1,D3D11_SDK_VERSION,&raw,&feature,&rawctx),"D3D11 WARP");deviceName="Microsoft WARP software";software=true;}
  else{
   for(auto& x:dxgiAdapters()){
    if(x.desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
    if(matchVendor&&x.desc.VendorId!=matchVendor)continue;
    if(matchDevice&&x.desc.DeviceId!=matchDevice)continue;
    selected=x.adapter;vendor=x.desc.VendorId;deviceId=x.desc.DeviceId;deviceName=utf8(x.desc.Description);break;
   }
   must(bool(selected),"No DXGI hardware adapter matching Vulkan PCI vendor/device IDs");
   dxc(D3D11CreateDevice(selected.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,levels,1,D3D11_SDK_VERSION,&raw,&feature,&rawctx),"D3D11 hardware device");
  }
  d.Attach(raw);ctx.Attach(rawctx);
  ComPtr<ID3DBlob> blob,err;
  HRESULT compile=D3DCompile(kHlsl,strlen(kHlsl),"bench.hlsl",nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&err);
  if(FAILED(compile))throw std::runtime_error("HLSL shader: "+std::string(err?static_cast<const char*>(err->GetBufferPointer()):"unknown"));
  dxc(d->CreateComputeShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,&shader),"CreateComputeShader");
  D3D11_BUFFER_DESC b{};b.ByteWidth=UINT(pixels.size()*4u);b.Usage=D3D11_USAGE_DEFAULT;b.BindFlags=D3D11_BIND_UNORDERED_ACCESS;b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;b.StructureByteStride=4;
  dxc(d->CreateBuffer(&b,nullptr,&gpu),"D3D GPU output");
  D3D11_UNORDERED_ACCESS_VIEW_DESC ud{};ud.Format=DXGI_FORMAT_UNKNOWN;ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;ud.Buffer.NumElements=UINT(pixels.size());
  dxc(d->CreateUnorderedAccessView(gpu.Get(),&ud,&uav),"D3D UAV");
  b.Usage=D3D11_USAGE_STAGING;b.BindFlags=0;b.CPUAccessFlags=D3D11_CPU_ACCESS_READ;b.MiscFlags=0;b.StructureByteStride=0;
  dxc(d->CreateBuffer(&b,nullptr,&staging),"D3D staging");
  D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
  dxc(d->CreateBuffer(&bd,nullptr,&cb),"D3D constants");
  D3D11_QUERY_DESC q{};q.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;dxc(d->CreateQuery(&q,&disjoint),"D3D disjoint query");
  q.Query=D3D11_QUERY_TIMESTAMP;dxc(d->CreateQuery(&q,&qStart),"D3D start query");dxc(d->CreateQuery(&q,&qEnd),"D3D end query");
 }
 Sample run(int index,bool hashOutput){
  auto t=Clock::now();ctx->UpdateSubresource(cb.Get(),0,nullptr,&params,0,0);
  ID3D11UnorderedAccessView* ua=uav.Get();ID3D11Buffer* constant=cb.Get();ctx->CSSetShader(shader.Get(),nullptr,0);
  ctx->CSSetUnorderedAccessViews(0,1,&ua,nullptr);ctx->CSSetConstantBuffers(0,1,&constant);
  ctx->Begin(disjoint.Get());ctx->End(qStart.Get());ctx->Dispatch((params.w+15)/16,(params.h+15)/16,1);ctx->End(qEnd.Get());ctx->End(disjoint.Get());
  ctx->CopyResource(staging.Get(),gpu.Get());
  D3D11_MAPPED_SUBRESOURCE map{};dxc(ctx->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"D3D readback Map");
  memcpy(pixels.data(),map.pData,pixels.size()*4u);ctx->Unmap(staging.Get(),0);
  auto finish=Clock::now();
  D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj{};UINT64 start=0,end=0;
  while(ctx->GetData(disjoint.Get(),&dj,sizeof(dj),0)==S_FALSE)std::this_thread::yield();
  while(ctx->GetData(qStart.Get(),&start,sizeof(start),0)==S_FALSE)std::this_thread::yield();
  while(ctx->GetData(qEnd.Get(),&end,sizeof(end),0)==S_FALSE)std::this_thread::yield();
  Sample s;s.api="dx11";s.index=index;s.wallMs=std::chrono::duration<double,std::milli>(finish-t).count();
  if(!dj.Disjoint&&dj.Frequency>0&&end>=start)s.gpuMs=1000.0*double(end-start)/double(dj.Frequency);
  if(hashOutput)s.checksum=fnv(pixels);
  ID3D11UnorderedAccessView* none=nullptr;ctx->CSSetUnorderedAccessViews(0,1,&none,nullptr);
  return s;
 }
};

static std::vector<uint32_t> readSpirv(const std::string& path){std::ifstream f(path,std::ios::binary|std::ios::ate);must(bool(f),"Missing SPIR-V: "+path);std::streamsize sz=static_cast<std::streamsize>(f.tellg());must(sz>0&&sz%4==0,"Invalid SPIR-V size");std::vector<uint32_t> b(size_t(sz)/4);f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),sz);must(bool(f),"Failed to read SPIR-V");return b;}
struct VkMemory{VkBuffer buf=VK_NULL_HANDLE;VkDeviceMemory mem=VK_NULL_HANDLE;bool coherent=false;};
class VkBench{
 VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice d=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;uint32_t family=0;
 VkMemory output,staging;VkDescriptorSetLayout descLayout=VK_NULL_HANDLE;VkDescriptorPool pool=VK_NULL_HANDLE;VkDescriptorSet desc=VK_NULL_HANDLE;
 VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;VkCommandPool cmdPool=VK_NULL_HANDLE;VkCommandBuffer cmd=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;VkQueryPool queries=VK_NULL_HANDLE;
 std::vector<uint32_t> pixels;Params params;float timestampPeriod=0;uint32_t timestampBits=0;
 uint32_t memType(uint32_t bits,VkMemoryPropertyFlags want){VkPhysicalDeviceMemoryProperties m{};vkGetPhysicalDeviceMemoryProperties(physical,&m);for(uint32_t i=0;i<m.memoryTypeCount;i++)if((bits&(1u<<i))&&(m.memoryTypes[i].propertyFlags&want)==want)return i;throw std::runtime_error("Required Vulkan memory type not found");}
 VkMemory buffer(VkDeviceSize size,VkBufferUsageFlags usage,VkMemoryPropertyFlags want){VkMemory out{};
  VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=size;bi.usage=usage;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  vkc(vkCreateBuffer(d,&bi,nullptr,&out.buf),"vkCreateBuffer");VkMemoryRequirements req{};vkGetBufferMemoryRequirements(d,out.buf,&req);
  uint32_t index=0;try{index=memType(req.memoryTypeBits,want|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);}
  catch(...){index=memType(req.memoryTypeBits,want);}
  VkPhysicalDeviceMemoryProperties props{};vkGetPhysicalDeviceMemoryProperties(physical,&props);out.coherent=(props.memoryTypes[index].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=index;
  vkc(vkAllocateMemory(d,&ai,nullptr,&out.mem),"vkAllocateMemory");vkc(vkBindBufferMemory(d,out.buf,out.mem,0),"vkBindBufferMemory");return out;}
 void freeBuffer(VkMemory& b){if(b.buf)vkDestroyBuffer(d,b.buf,nullptr);if(b.mem)vkFreeMemory(d,b.mem,nullptr);b={};}
public:
 std::string deviceName;uint32_t vendor=0,deviceId=0;bool software=false;
 static std::vector<std::string> enumerate(){std::vector<std::string> a;VkInstance instance=VK_NULL_HANDLE;VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_0;VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
  if(vkCreateInstance(&ci,nullptr,&instance)!=VK_SUCCESS)return {"Vulkan loader/ICD unavailable"};
  uint32_t n=0;vkEnumeratePhysicalDevices(instance,&n,nullptr);std::vector<VkPhysicalDevice> v(n);if(n)vkEnumeratePhysicalDevices(instance,&n,v.data());
  for(auto x:v){VkPhysicalDeviceProperties p{};vkGetPhysicalDeviceProperties(x,&p);a.push_back(std::string(p.deviceName)+" (vendor="+std::to_string(p.vendorID)+", device="+std::to_string(p.deviceID)+")");}
  vkDestroyInstance(instance,nullptr);return a;}
 VkBench(const Params& p,const std::string& spirv,bool allowSoftware):params(p),pixels(size_t(p.w)*p.h){
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="CSE WARP DX11 Vulkan A/B";app.apiVersion=VK_API_VERSION_1_0;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;vkc(vkCreateInstance(&ici,nullptr,&instance),"vkCreateInstance");
  uint32_t count=0;vkc(vkEnumeratePhysicalDevices(instance,&count,nullptr),"vkEnumeratePhysicalDevices count");must(count>0,"No Vulkan physical device (Intel driver Vulkan runtime?)");
  std::vector<VkPhysicalDevice> devs(count);vkc(vkEnumeratePhysicalDevices(instance,&count,devs.data()),"vkEnumeratePhysicalDevices");
  int best=-1;for(auto x:devs){VkPhysicalDeviceProperties pr{};vkGetPhysicalDeviceProperties(x,&pr);
   if(pr.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU&&!allowSoftware)continue;
   uint32_t qc=0;vkGetPhysicalDeviceQueueFamilyProperties(x,&qc,nullptr);std::vector<VkQueueFamilyProperties> qs(qc);vkGetPhysicalDeviceQueueFamilyProperties(x,&qc,qs.data());
   for(uint32_t i=0;i<qc;i++)if(qs[i].queueFlags&VK_QUEUE_COMPUTE_BIT){int score=pr.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?100:pr.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?90:10;
    if(score>best){best=score;physical=x;family=i;timestampBits=qs[i].timestampValidBits;}break;}
  }
  must(physical!=VK_NULL_HANDLE,"No Vulkan GPU supporting compute; software ICDs are intentionally excluded");
  VkPhysicalDeviceProperties pp{};vkGetPhysicalDeviceProperties(physical,&pp);deviceName=pp.deviceName;vendor=pp.vendorID;deviceId=pp.deviceID;software=(pp.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU);timestampPeriod=pp.limits.timestampPeriod;
  float priority=1.f;VkDeviceQueueCreateInfo qc{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qc.queueFamilyIndex=family;qc.queueCount=1;qc.pQueuePriorities=&priority;
  VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qc;vkc(vkCreateDevice(physical,&dc,nullptr,&d),"vkCreateDevice");vkGetDeviceQueue(d,family,0,&queue);
  output=buffer(VkDeviceSize(pixels.size()*4),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  staging=buffer(VkDeviceSize(pixels.size()*4),VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  VkDescriptorSetLayoutBinding db{};db.binding=0;db.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;db.descriptorCount=1;db.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;
  VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};dl.bindingCount=1;dl.pBindings=&db;vkc(vkCreateDescriptorSetLayout(d,&dl,nullptr,&descLayout),"vkCreateDescriptorSetLayout");
  VkDescriptorPoolSize ps{};ps.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;ps.descriptorCount=1;
  VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=1;dpi.poolSizeCount=1;dpi.pPoolSizes=&ps;vkc(vkCreateDescriptorPool(d,&dpi,nullptr,&pool),"vkCreateDescriptorPool");
  VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};dai.descriptorPool=pool;dai.descriptorSetCount=1;dai.pSetLayouts=&descLayout;vkc(vkAllocateDescriptorSets(d,&dai,&desc),"vkAllocateDescriptorSets");
  VkDescriptorBufferInfo buff{output.buf,0,VkDeviceSize(pixels.size()*4)};VkWriteDescriptorSet wr{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};wr.dstSet=desc;wr.dstBinding=0;wr.descriptorCount=1;wr.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;wr.pBufferInfo=&buff;vkUpdateDescriptorSets(d,1,&wr,0,nullptr);
  VkPushConstantRange pc{};pc.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;pc.offset=0;pc.size=sizeof(Params);
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&descLayout;pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&pc;vkc(vkCreatePipelineLayout(d,&pli,nullptr,&layout),"vkCreatePipelineLayout");
  auto code=readSpirv(spirv);VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};smi.codeSize=code.size()*4;smi.pCode=code.data();VkShaderModule module{};vkc(vkCreateShaderModule(d,&smi,nullptr,&module),"vkCreateShaderModule");
  VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cpi.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cpi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cpi.stage.module=module;cpi.stage.pName="main";cpi.layout=layout;
  VkResult cr=vkCreateComputePipelines(d,VK_NULL_HANDLE,1,&cpi,nullptr,&pipeline);vkDestroyShaderModule(d,module,nullptr);vkc(cr,"vkCreateComputePipelines");
  VkCommandPoolCreateInfo cpc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpc.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cpc.queueFamilyIndex=family;vkc(vkCreateCommandPool(d,&cpc,nullptr,&cmdPool),"vkCreateCommandPool");
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};cai.commandPool=cmdPool;cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;cai.commandBufferCount=1;vkc(vkAllocateCommandBuffers(d,&cai,&cmd),"vkAllocateCommandBuffers");
  VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vkc(vkCreateFence(d,&fi,nullptr,&fence),"vkCreateFence");
  VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=2;vkc(vkCreateQueryPool(d,&qi,nullptr,&queries),"vkCreateQueryPool");
 }
 ~VkBench(){if(d){vkDeviceWaitIdle(d);if(queries)vkDestroyQueryPool(d,queries,nullptr);if(fence)vkDestroyFence(d,fence,nullptr);if(cmdPool)vkDestroyCommandPool(d,cmdPool,nullptr);if(pipeline)vkDestroyPipeline(d,pipeline,nullptr);if(layout)vkDestroyPipelineLayout(d,layout,nullptr);if(pool)vkDestroyDescriptorPool(d,pool,nullptr);if(descLayout)vkDestroyDescriptorSetLayout(d,descLayout,nullptr);freeBuffer(staging);freeBuffer(output);vkDestroyDevice(d,nullptr);}if(instance)vkDestroyInstance(instance,nullptr);}
 Sample run(int index,bool hashOutput){
  auto t=Clock::now();vkc(vkResetCommandBuffer(cmd,0),"vkResetCommandBuffer");VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};vkc(vkBeginCommandBuffer(cmd,&bi),"vkBeginCommandBuffer");
  vkCmdResetQueryPool(cmd,queries,0,2);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&desc,0,nullptr);
  vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(params),&params);
  if(timestampBits)vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,queries,0);
  vkCmdDispatch(cmd,(params.w+15)/16,(params.h+15)/16,1);
  if(timestampBits)vkCmdWriteTimestamp(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,queries,1);
  VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.buffer=output.buf;barrier.size=VK_WHOLE_SIZE;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&barrier,0,nullptr);
  VkBufferCopy region{};region.size=VkDeviceSize(pixels.size()*4);vkCmdCopyBuffer(cmd,output.buf,staging.buf,1,&region);
  barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;barrier.buffer=staging.buf;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
  vkc(vkEndCommandBuffer(cmd),"vkEndCommandBuffer");VkSubmitInfo sub{VK_STRUCTURE_TYPE_SUBMIT_INFO};sub.commandBufferCount=1;sub.pCommandBuffers=&cmd;
  vkc(vkQueueSubmit(queue,1,&sub,fence),"vkQueueSubmit");vkc(vkWaitForFences(d,1,&fence,VK_TRUE,UINT64_MAX),"vkWaitForFences");vkc(vkResetFences(d,1,&fence),"vkResetFences");
  void* map=nullptr;vkc(vkMapMemory(d,staging.mem,0,VK_WHOLE_SIZE,0,&map),"vkMapMemory");
  if(!staging.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=staging.mem;range.offset=0;range.size=VK_WHOLE_SIZE;vkc(vkInvalidateMappedMemoryRanges(d,1,&range),"vkInvalidateMappedMemoryRanges");}
  memcpy(pixels.data(),map,pixels.size()*4);vkUnmapMemory(d,staging.mem);auto finish=Clock::now();
  Sample s;s.api="vulkan";s.index=index;s.wallMs=std::chrono::duration<double,std::milli>(finish-t).count();
  if(timestampBits){uint64_t ts[2]={};vkc(vkGetQueryPoolResults(d,queries,0,2,sizeof(ts),ts,sizeof(uint64_t),VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WAIT_BIT),"vkGetQueryPoolResults");
   if(ts[1]>=ts[0])s.gpuMs=double(ts[1]-ts[0])*double(timestampPeriod)/1000000.0;}
  if(hashOutput)s.checksum=fnv(pixels);return s;
 }
};
#include "vulkan_turbo.h"
static double pct(const std::vector<double>& a,double p){auto x=a;std::sort(x.begin(),x.end());if(x.empty())return -1;return x[size_t(std::ceil(p*x.size()))-1];}
static double mean(const std::vector<double>& a){return a.empty()?-1:std::accumulate(a.begin(),a.end(),0.0)/double(a.size());}
static void summary(const std::vector<Sample>& all,const std::string& api){std::vector<double> g,w;for(const auto& s:all)if(s.api==api){if(s.gpuMs>=0)g.push_back(s.gpuMs);w.push_back(s.wallMs);}
 std::cout<<std::fixed<<std::setprecision(3)<<api<<" samples="<<w.size()<<" compute_gpu_p50="<<pct(g,.5)<<" ms p95="<<pct(g,.95)<<" ms wall_p50="<<pct(w,.5)<<" ms p95="<<pct(w,.95)<<" ms\n";}
int main(int argc,char**argv){try{
 Config c=parse(argc,argv);std::cout<<"CSE WARP V17 research | same integer compute shader, NOT production Raster\n";
 auto adapters=dxgiAdapters();for(const auto& x:adapters)std::cout<<"DXGI "<<utf8(x.desc.Description)<<" vendor="<<x.desc.VendorId<<" device="<<x.desc.DeviceId<<"\n";
 auto vs=VkBench::enumerate();for(const auto& v:vs)std::cout<<"Vulkan "<<v<<"\n";if(c.probe)return 0;
 char exe[MAX_PATH]{};GetModuleFileNameA(nullptr,exe,MAX_PATH);std::string shader=std::string(exe);auto pos=shader.find_last_of("/\\");shader=(pos==std::string::npos?std::string():shader.substr(0,pos+1))+"benchmark.comp.spv";
 std::unique_ptr<VkBench> vk;std::unique_ptr<VkTurboBench> turbo;std::unique_ptr<DxBench> dx;
 if(c.api!="dx11"){
  if(c.vkMode=="legacy")vk=std::make_unique<VkBench>(c.p,shader,c.allowSoftware);
  else turbo=std::make_unique<VkTurboBench>(c.p,shader,c.vkMode,c.vkInFlight,c.vkReuse,c.allowSoftware);
 }
 uint32_t vVendor=vk?vk->vendor:turbo?turbo->vendor:0;
 uint32_t vDevice=vk?vk->deviceId:turbo?turbo->deviceId:0;
 if(c.api!="vulkan")dx=std::make_unique<DxBench>(c.p,vVendor,vDevice,c.warp);
 if(vk)std::cout<<"SELECT Vulkan="<<vk->deviceName<<" vendor="<<vVendor<<" device="<<vDevice<<"\n";
 if(turbo)std::cout<<"SELECT Vulkan Turbo="<<turbo->deviceName<<" vendor="<<vVendor<<" device="<<vDevice<<"\n";
 if(dx)std::cout<<"SELECT D3D11="<<dx->deviceName<<" vendor="<<dx->vendor<<" device="<<dx->deviceId<<"\n";
 if((vk||turbo)&&dx)must(!(vk?vk->software:turbo->software)&&!dx->software&&vVendor==dx->vendor&&vDevice==dx->deviceId,"Paired benchmark requires the SAME PHYSICAL GPU");
 std::vector<Sample> samples;uint64_t hashDx=0,hashVk=0;
 auto one=[&](bool isVk,int index,bool check){Sample s=isVk?(turbo?turbo->run(index,check):vk->run(index,check)):dx->run(index,check);if(check){if(isVk)hashVk=s.checksum;else hashDx=s.checksum;}if(index>=0)samples.push_back(s);};
 if(turbo&&c.vkInFlight>1){
  samples=turbo->runBatch(c.warmup,c.repeat);hashVk=turbo->checksum();
  std::cout<<"TURBO PIPELINE jobs="<<c.repeat<<" elapsed_ms="<<turbo->batchElapsedMs()
           <<" jobs_per_second="<<1000.0*double(c.repeat)/turbo->batchElapsedMs()<<"\n";
 }else{
  for(int i=0;i<c.warmup;i++){if(dx)one(false,-1,false);if(vk||turbo)one(true,-1,false);}
  for(int i=0;i<c.repeat;i++){
   bool check=i==c.repeat-1;
   if(dx&&(vk||turbo)){if((i&1)==0){one(false,i,check);one(true,i,check);}else{one(true,i,check);one(false,i,check);}}
   else if(dx)one(false,i,check);else one(true,i,check);
  }
 }
 if((vk||turbo)&&dx){must(hashDx==hashVk,"DX11/Vulkan output hash mismatch: "+hex(hashDx)+" vs "+hex(hashVk));std::cout<<"OUTPUT BIT-EXACT PASS checksum="<<hex(hashVk)<<"\n";}
 else std::cout<<"OUTPUT checksum="<<hex((vk||turbo)?hashVk:hashDx)<<" (single API; no cross-API validation)\n";
 if(dx)summary(samples,"dx11");if(vk||turbo)summary(samples,"vulkan");
 std::ofstream csv(c.output+".csv");must(bool(csv),"Cannot write CSV");csv<<"order,api,repetition,gpu_compute_ms,wall_dispatch_copy_ms\n";int order=0;csv<<std::fixed<<std::setprecision(6);
 for(const auto&s:samples)csv<<order++<<","<<s.api<<","<<s.index<<","<<s.gpuMs<<","<<s.wallMs<<"\n";
 std::ofstream json(c.output+".json");must(bool(json),"Cannot write JSON");json<<"{\n  \"benchmark\": \"compute_shader_integer_stress_not_v17_raster\",\n  \"resolution\": ["<<c.p.w<<","<<c.p.h<<"],\n  \"loops\": "<<c.p.loops<<", \"warmup\": "<<c.warmup<<", \"repetitions\": "<<c.repeat<<",\n  \"device_dx11\": \""<<esc(dx?dx->deviceName:"")<<"\",\n  \"device_vulkan\": \""<<esc(vk?vk->deviceName:"")<<"\",\n  \"pci_vendor\": "<<(vk?vk->vendor:dx?dx->vendor:0)<<", \"pci_device\": "<<(vk?vk->deviceId:dx?dx->deviceId:0)<<",\n  \"bit_exact\": "<<(vk&&dx?"true":"null")<<", \"checksum\": \""<<hex(vk?hashVk:hashDx)<<"\",\n  \"samples\": [\n";
 for(size_t i=0;i<samples.size();i++){auto&s=samples[i];json<<"    {\"api\":\""<<s.api<<"\",\"repeat\":"<<s.index<<",\"gpu_compute_ms\":"<<(s.gpuMs>=0?std::to_string(s.gpuMs):"null")<<",\"wall_ms\":"<<std::to_string(s.wallMs)<<"}"<<(i+1<samples.size() ? ",":"")<<"\n";}
 json<<"  ]\n}\n";std::cout<<"Wrote "<<c.output<<".csv and .json\n";return 0;
 }catch(const std::exception&e){std::cerr<<"BENCH FAILED: "<<e.what()<<"\n";return 1;}}
