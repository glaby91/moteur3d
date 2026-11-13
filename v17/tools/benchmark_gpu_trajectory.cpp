// CSE WARP V17.432 -- dedicated hardware DirectX11 GPU trajectory benchmark.
// Only one existing scene, one glass-area camera, a six-second virtual movement.
// Keeps the V17 raster shader and image quality unchanged. No full campaign.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QStringList>
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>
#pragma comment(lib,"psapi.lib")
#pragma comment(lib,"pdh.lib")
#pragma comment(lib,"dxgi.lib")
using namespace m3d;
using Clock=std::chrono::steady_clock;
using Microsoft::WRL::ComPtr;

namespace {
double elapsed(Clock::time_point a,Clock::time_point b){
 return std::chrono::duration<double,std::milli>(b-a).count();
}
double filetimeMs(const FILETIME &t){
 ULARGE_INTEGER x{};x.LowPart=t.dwLowDateTime;x.HighPart=t.dwHighDateTime;
 return double(x.QuadPart)/10000.0;
}
double processCpuMs(){
 FILETIME a{},b{},kernel{},user{};
 if(!GetProcessTimes(GetCurrentProcess(),&a,&b,&kernel,&user))return -1;
 return filetimeMs(kernel)+filetimeMs(user);
}
QJsonValue number(double x){return std::isfinite(x)&&x>=0?QJsonValue(x):QJsonValue(QJsonValue::Null);}
double mb(SIZE_T bytes){return double(bytes)/1048576.0;}
struct Memory {
 double workingMb=-1,privateMb=-1,peakWorkingMb=-1;
};
Memory memory(){
 PROCESS_MEMORY_COUNTERS_EX p{};p.cb=sizeof(p);
 if(!GetProcessMemoryInfo(GetCurrentProcess(),(PROCESS_MEMORY_COUNTERS*)&p,sizeof(p)))return {};
 return {mb(p.WorkingSetSize),mb(p.PrivateUsage),mb(p.PeakWorkingSetSize)};
}
class GpuUtil {
 PDH_HQUERY q_=nullptr;PDH_HCOUNTER c_=nullptr;
public:
 GpuUtil(){
  if(PdhOpenQueryW(nullptr,0,&q_)!=ERROR_SUCCESS){q_=nullptr;return;}
  if(PdhAddEnglishCounterW(q_,L"\\GPU Engine(*)\\Utilization Percentage",0,&c_)!=ERROR_SUCCESS){
   PdhCloseQuery(q_);q_=nullptr;return;
  }
  PdhCollectQueryData(q_);
 }
 ~GpuUtil(){if(q_)PdhCloseQuery(q_);}
 struct Load {double total=-1,threeD=-1;};
 Load sample(){
  Load load;
  if(!q_||PdhCollectQueryData(q_)!=ERROR_SUCCESS)return load;
  DWORD count=0,bytes=0;
  PDH_STATUS st=PdhGetFormattedCounterArrayW(c_,PDH_FMT_DOUBLE,&bytes,&count,nullptr);
  if(st!=PDH_MORE_DATA||bytes==0||bytes>16*1024*1024)return load;
  std::vector<uint8_t> raw(bytes+64);
  auto *items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(raw.data());
  if(PdhGetFormattedCounterArrayW(c_,PDH_FMT_DOUBLE,&bytes,&count,items)!=ERROR_SUCCESS)return load;
  const QString needle=QStringLiteral("pid_%1_").arg(GetCurrentProcessId());
  load.total=0;load.threeD=0;bool any=false;
  for(DWORD i=0;i<count;++i){
   const QString n=QString::fromWCharArray(items[i].szName).toLower();
   if(!n.contains(needle)||items[i].FmtValue.CStatus!=PDH_CSTATUS_VALID_DATA)continue;
   const double v=items[i].FmtValue.doubleValue;
   if(v<0||!std::isfinite(v))continue;
   any=true;load.total+=v;
   if(n.contains(QStringLiteral("engtype_3d")))load.threeD+=v;
  }
  if(!any)return {};
  return load;
 }
};
class VideoMemory {
 ComPtr<IDXGIAdapter3> gpu_;
public:
 QString adapterName="indisponible",vendor="unknown";
 bool adapterFound=false;
 VideoMemory(){
  ComPtr<IDXGIFactory1> factory;
  if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return;
  ComPtr<IDXGIAdapter1> adapter;
  if(FAILED(factory->EnumAdapters1(0,&adapter)))return;
  DXGI_ADAPTER_DESC1 d{};
  if(SUCCEEDED(adapter->GetDesc1(&d))){
   adapterFound=true;
   adapterName=QString::fromWCharArray(d.Description);
   vendor=QStringLiteral("0x%1").arg(d.VendorId,4,16,QChar('0'));
  }
  adapter.As(&gpu_);
 }
 QJsonObject sample()const{
  QJsonObject obj;
  if(!gpu_)return obj;
  for(auto [name,group]:{std::pair<const char*,DXGI_MEMORY_SEGMENT_GROUP>{"local",DXGI_MEMORY_SEGMENT_GROUP_LOCAL},
                         {"shared",DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL}}){
   DXGI_QUERY_VIDEO_MEMORY_INFO info{};
   if(SUCCEEDED(gpu_->QueryVideoMemoryInfo(0,group,&info))){
    obj[QString::fromLatin1(name)+"UsageMb"]=mb(info.CurrentUsage);
    obj[QString::fromLatin1(name)+"BudgetMb"]=mb(info.Budget);
    obj[QString::fromLatin1(name)+"ReservationMb"]=mb(info.CurrentReservation);
   }
  }
  return obj;
 }
};
double pct(const std::vector<double>& values,double p){
 if(values.empty())return -1;
 auto x=values;
 std::sort(x.begin(),x.end());
 const double i=(x.size()-1)*p;
 const size_t a=size_t(i),b=std::min(x.size()-1,a+1);
 return x[a]+(x[b]-x[a])*(i-a);
}
bool saveJson(const QString &path,const QJsonObject& obj){
 QFile f(path);
 return f.open(QIODevice::WriteOnly|QIODevice::Truncate)
  &&f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented))>0;
}
bool savePng(const QString&path,const std::vector<uint32_t>& px,int w,int h){
 if(px.size()!=size_t(w)*h)return false;
 QImage im(w,h,QImage::Format_ARGB32);
 for(int y=0;y<h;++y)std::memcpy(im.scanLine(y),px.data()+size_t(y)*w,size_t(w)*4);
 return im.save(path,"PNG");
}
}
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);
 QTextStream out(stdout),err(stderr);
 QStringList args=app.arguments();
 if(args.size()<3){err<<"Usage: CSE-WARP-GPU-TRAJET-BENCH.exe scene.json dossier_sortie [--ci-smoke] [--z-prepass=on|off] [--light-aabb=on|off]\n";return 2;}
 const auto configureSwitch=[&](const char* flag,const char* env)->bool{
  const QString option=QString::fromLatin1(flag);
  const bool on=args.contains(option+"=on"),off=args.contains(option+"=off");
  if(on&&off){err<<"Contradictory option: "<<option<<"\n";return false;}
  if(on)qputenv(env,"1");
  if(off)qputenv(env,"0");
  return true;
 };
 if(!configureSwitch("--z-prepass","M3D_V17_EXPERIMENT_Z_PREPASS")||
    !configureSwitch("--light-aabb","M3D_V17_EXPERIMENT_LIGHT_AABB_CULL"))
    return 2;
 const bool zEnabled=qgetenv("M3D_V17_EXPERIMENT_Z_PREPASS")=="1";
 const bool lightAabbEnabled=qgetenv("M3D_V17_EXPERIMENT_LIGHT_AABB_CULL")=="1";
 const bool smoke=args.contains("--ci-smoke");
 const int w=smoke?640:1366,h=smoke?360:768;
 const int frames=smoke?4:90;
 const double virtualDuration=6.0;
 const QString sceneFile=QFileInfo(args[1]).absoluteFilePath();
 const QString rootDir=QFileInfo(args[2]).absoluteFilePath();
 const QString stamp=QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
 const QString target=QDir(rootDir).filePath(QStringLiteral("BENCH-GPU-HD620-%1").arg(stamp));
 if(!QDir().mkpath(target)){err<<"Impossible de creer le dossier de resultats\n";return 3;}
 QDir results(target);
 // Hardware GPU is the default. The software renderer is allowed only for CI.
 if(smoke)qputenv("M3D_V16_FORCE_WARP","1");
 else {
  qunsetenv("M3D_V16_FORCE_WARP");
  if(!VideoMemory{}.adapterFound){err<<"No DirectX GPU adapter, hardware benchmark impossible\n";return 4;}
 }
 const auto started=Clock::now();
 auto loaded=v16::loadScene(sceneFile);
 if(!loaded){err<<"Scene load error: "<<loaded.error<<"\n";return 5;}
 auto built=v16::buildRuntime(loaded.scene,smoke?256:2048);
 if(!built){err<<"Runtime build error: "<<built.error<<"\n";return 5;}
 auto &rt=built.runtime;
 const auto baseIt=std::find_if(rt.cameras.begin(),rt.cameras.end(),[](const v16::CameraPreset& c){
  return c.id==QStringLiteral("v8_glazing_raccords");
 });
 if(baseIt==rt.cameras.end()){err<<"Camera v8_glazing_raccords missing from current scene\n";return 5;}
 auto quality=rasterQualitySettings();
 quality.msaaSamples=2;
 std::wstring qualityError;
 if(!setRasterQualitySettings(quality,qualityError)){
  err<<"Invalid MSAA 2 quality setting: "<<QString::fromStdWString(qualityError)<<"\n";return 5;
 }
 updateLighting(14.75f);
 Stats shadowStats;
 auto shadow=buildShadow(rt.renderScene,shadowStats);
 VideoMemory video;
 GpuUtil gpu;
 SYSTEM_INFO sys{};GetSystemInfo(&sys);
 const unsigned logical=std::max(1u,unsigned(sys.dwNumberOfProcessors));
 const auto initialized=Clock::now();
 out<<"CSE WARP GPU D3D11 Raster trajectory, scene V17.430\n"
    <<"Device: "<<video.adapterName<<"; "<<w<<"x"<<h<<"; frames="<<frames
    <<"; virtual seconds="<<virtualDuration<<"; MSAA=2, PBR="<<(smoke?256:2048)
     <<"; Z-prepass="<<(zEnabled?"ON":"OFF")
     <<"; Local light AABB="<<(lightAabbEnabled?"ON":"OFF")<<"\n";
 out.flush();

 const auto shifted=[&](int n){
  const float t=float(n)/float(std::max(1,frames-1));
  auto preset=*baseIt;
  const float dx=(t-.5f)*5.4f; // same look direction, camera glides 5.4 m along glass
  preset.eye.x+=dx;preset.target.x+=dx;
  return v16::makeCamera(preset,w,h);
 };
 std::vector<uint32_t> pixels;
 Stats stats{};std::wstring reason;
 for(int i=0;i<2;++i){
  if(!renderDirectX11Preview(rt.renderScene,shadow,shifted(i*(frames-1)),w,h,pixels,stats,reason)){
   err<<"DirectX11 warmup failed: "<<QString::fromStdWString(reason)<<"\n";return 6;
  }
 }
 const QString csvPath=results.filePath("frames.csv");
 QFile csvFile(csvPath);
 if(!csvFile.open(QIODevice::WriteOnly|QIODevice::Text)){err<<"CSV unavailable\n";return 7;}
 QTextStream csv(&csvFile);
 csv<<"frame,z_prepass,light_aabb_cull,virtual_seconds,camera_x_m,camera_y_m,camera_z_m,wall_ms,throughput_fps,"
       "process_cpu_ms,process_cpu_percent_total,process_cpu_percent_one_core,"
       "working_set_mb,private_commit_mb,peak_working_set_mb,"
       "gpu_3d_util_pct,gpu_all_engines_pct,dx11_gpu_timing_valid,"
       "dx11_draw_ms,dx11_transfer_ms,dx11_gpu_total_ms,"
       "dx11_cpu_submit_ms,dx11_map_wait_ms,dx11_pixel_convert_ms,"
       "draw_calls,submitted_triangles,gpu_local_usage_mb,gpu_shared_usage_mb\n";
 QJsonArray samples;
 std::vector<double> times,procCpu,gpuDrawTimes;
 int timingValid=0,gpuLoadValid=0;
 double gpu3dSum=0;
 const auto runBegin=Clock::now();
 for(int i=0;i<frames;++i){
  const double virtualT=virtualDuration*double(i)/double(frames-1);
  const auto cam=shifted(i);
  const double cpuStart=processCpuMs();
  const auto before=Clock::now();
  reason.clear();
  const bool ok=renderDirectX11Preview(rt.renderScene,shadow,cam,w,h,pixels,stats,reason);
  const auto after=Clock::now();
  if(!ok){
   err<<"Render error at frame "<<i<<": "<<QString::fromStdWString(reason)<<"\n";
   csv.flush();return 8;
  }
  const double wall=elapsed(before,after);
  const double cpuUsed=processCpuMs()-cpuStart;
  const double totalLoad=wall>0?cpuUsed/(wall*logical)*100.0:-1;
  const auto mem=memory();
  const auto gl=gpu.sample();
  const auto vm=video.sample();
  if(gl.threeD>=0){gpuLoadValid++;gpu3dSum+=gl.threeD;}
  if(stats.gpuTimingValid){timingValid++;gpuDrawTimes.push_back(stats.gpuDrawMs);}
  times.push_back(wall);
  procCpu.push_back(cpuUsed);
  auto take=[&](double v){return v>=0?QString::number(v,'f',4):QString();};
  csv<<i<<','<<(zEnabled?1:0)<<','<<(lightAabbEnabled?1:0)<<','<<QString::number(virtualT,'f',4)<<','
     <<cam.eye.x<<','<<cam.eye.y<<','<<cam.eye.z<<','
     <<take(wall)<<','<<take(wall>0?1000/wall:-1)<<','
     <<take(cpuUsed)<<','<<take(totalLoad)<<','
     <<take(wall>0?cpuUsed/wall*100.0:-1)<<','
     <<take(mem.workingMb)<<','<<take(mem.privateMb)<<','<<take(mem.peakWorkingMb)<<','
     <<take(gl.threeD)<<','<<take(gl.total)<<','<<(stats.gpuTimingValid?1:0)<<','
     <<(stats.gpuTimingValid?take(stats.gpuDrawMs):QString())<<','
     <<(stats.gpuTimingValid?take(stats.gpuTransferMs):QString())<<','
     <<(stats.gpuTimingValid?take(stats.gpuTotalMs):QString())<<','
     <<take(stats.cpuSubmitMs)<<','<<take(stats.cpuMapWaitMs)<<','
     <<take(stats.cpuPixelConvertMs)<<','<<stats.gpuDrawCalls<<','
     <<stats.tests<<','
     <<(vm.contains("localUsageMb")?QString::number(vm["localUsageMb"].toDouble(),'f',3):QString())<<','
     <<(vm.contains("sharedUsageMb")?QString::number(vm["sharedUsageMb"].toDouble(),'f',3):QString())<<"\n";
  csv.flush(); // Keep results from preceding frames even after an interruption.
  QJsonObject row{{"frame",i},{"zPrepassEnabled",zEnabled},
                   {"lightAabbCullEnabled",lightAabbEnabled},
                   {"virtualSeconds",virtualT},{"wallMs",wall},
                   {"cpuProcessMs",cpuUsed},{"cpuPercentOfTotal",number(totalLoad)},
                   {"ramWorkingSetMb",number(mem.workingMb)},
                   {"ramPrivateCommitMb",number(mem.privateMb)},
                   {"gpu3dUtilPercent",number(gl.threeD)},
                   {"gpuAllEnginesPercent",number(gl.total)},
                   {"dx11GpuTimingValid",stats.gpuTimingValid},
                   {"dx11GpuDrawMs",stats.gpuTimingValid?number(stats.gpuDrawMs):QJsonValue(QJsonValue::Null)},
                   {"dx11CpuMapWaitMs",stats.cpuMapWaitMs},
                   {"drawCalls",double(stats.gpuDrawCalls)},
                   {"adapterVideoMemory",vm}};
  samples.append(row);
  if(i==0||i==frames/2||i==frames-1)
   if(!savePng(results.filePath(QStringLiteral("frame_%1.png").arg(i,3,10,QChar('0'))),pixels,w,h)){
    err<<"Unable to save validation PNG\n";return 8;
   }
  if(i==0||(i+1)%10==0||i+1==frames){out<<"Frame "<<(i+1)<<"/"<<frames
      <<"  "<<QString::number(wall,'f',2)<<" ms  RAM "<<QString::number(mem.workingMb,'f',1)<<" MiB\n";out.flush();}
 }
 const auto runEnd=Clock::now();
 csvFile.close();
 const double avg=std::accumulate(times.begin(),times.end(),0.)/times.size();
 const double cpuAvg=std::accumulate(procCpu.begin(),procCpu.end(),0.)/procCpu.size();
 const auto mem=memory();
 QJsonObject summary{
  {"name","CSE WARP V17.432 GPU D3D11 Raster -- glass movement benchmark"},
  {"driverMode",smoke?"Microsoft software WARP, CI only":"DirectX11 HARDWARE -- Intel HD620 when run on the Dell"},
  {"adapterName",video.adapterName},{"adapterVendorId",video.vendor},
  {"deviceIdentification","DXGI default adapter descriptor; verify HD620 on local machine"},
  {"scene",sceneFile},{"camera","v8_glazing_raccords"},
  {"zPrepassEnabled",zEnabled},{"lightAabbCullEnabled",lightAabbEnabled},
  {"width",w},{"height",h},{"msaaRequested",2},{"pbrTexturePx",smoke?256:2048},
  {"sampleFrames",frames},{"virtualMovementSeconds",virtualDuration},
  {"movementDistanceMeters",5.4},{"fixedScene",true},
  {"logicalCpuCount",int(logical)},{"wallRenderSeconds",elapsed(runBegin,runEnd)/1000.0},
  {"preparationSeconds",elapsed(started,initialized)/1000.0},
  {"fpsDuringCalculation",1000.0/avg},
  {"meanWallMs",avg},{"medianWallMs",pct(times,.5)},
  {"p95WallMs",pct(times,.95)},{"p99WallMs",pct(times,.99)},
  {"meanProcessCpuMs",cpuAvg},
  {"processRamFinalWorkingSetMb",mem.workingMb},
  {"processRamFinalPrivateCommitMb",mem.privateMb},
  {"validGpuTimestampFrames",timingValid},
  {"validGpuLoadSamples",gpuLoadValid},
  {"gpu3dUtilMeanPercent",gpuLoadValid?number(gpu3dSum/gpuLoadValid):QJsonValue(QJsonValue::Null)},
  {"gpuDrawMeanMs",gpuDrawTimes.empty()?QJsonValue(QJsonValue::Null):
       number(std::accumulate(gpuDrawTimes.begin(),gpuDrawTimes.end(),0.)/gpuDrawTimes.size())},
  {"memoryNote","DXGI local/shared usage represents adapter-wide usage, not this process only."},
  {"gpuNote","GPU Engine utilization counters may be unavailable on some Windows versions. Blank CSV means unavailable; no fabricated zeros."},
  {"timingNote","Six seconds of deterministic virtual camera movement over 90 frames; elapsed benchmark time can differ."},
  {"featureFlagsNote","Z-prepass and experimental light AABB flags are captured from the actual environment used for this run."},
  {"files","frames.csv, summary.json, frame_000.png, frame_045.png, frame_089.png"}
 };
 if(!saveJson(results.filePath("summary.json"),QJsonObject{{"summary",summary},{"frames",samples}})){
  err<<"Cannot save summary.json\n";return 9;
 }
 out<<"SUCCESS: targeted GPU benchmark with per-frame CSV + JSON in "<<target<<"\n";out.flush();
 return 0;
}
