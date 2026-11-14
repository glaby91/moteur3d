// GPU A/B test: load the SAME V17 scene/camera, compare Vulkan against DirectX11.
// Standalone executable; does not claim shader pixel parity on the first port.
#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QDir>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <vector>

using Clock=std::chrono::steady_clock;
namespace {
double median(std::vector<double> t){
 if(t.empty())return -1.0;
 std::sort(t.begin(),t.end());
 const size_t n=t.size();
 return n%2?t[n/2]:(t[n/2-1]+t[n/2])*.5;
}
double p95(std::vector<double> t){
 if(t.empty())return -1.0;
 std::sort(t.begin(),t.end());
 size_t index=std::min(t.size()-1,size_t(std::ceil(double(t.size())*.95)-1));
 return t[index];
}
uint64_t checksum(const std::vector<uint32_t>& pixels){
 uint64_t hash=14695981039346656037ull;
 for(uint32_t p:pixels){hash^=p;hash*=1099511628211ull;}
 return hash;
}
bool png(QString name,const std::vector<uint32_t>& px,int w,int h){
 if(px.size()!=size_t(w)*h)return false;
 QImage image(w,h,QImage::Format_ARGB32);
 memcpy(image.bits(),px.data(),px.size()*4);
 return image.save(name,"PNG");
}
}
int main(int argc,char** argv) {
 QCoreApplication app(argc,argv);
 if(argc<3){
  std::cerr<<"Usage: v17_vulkan_ab_bench <scene.json> <output-dir> [frames=20] [width=1366] [height=768]\n";return 1;
 }
 const QString sceneFile=QString::fromLocal8Bit(argv[1]);
 const QString outDir=QString::fromLocal8Bit(argv[2]);
 const int frames=argc>3?std::max(3,std::min(100,std::atoi(argv[3]))):20;
 const int width=argc>4?std::atoi(argv[4]):1366;
 const int height=argc>5?std::atoi(argv[5]):768;
 if(width<=0||height<=0||width>4096||height>4096){std::cerr<<"Invalid resolution\n";return 1;}
 if(!QDir().mkpath(outDir)){std::cerr<<"Cannot create output folder\n";return 1;}
 const auto loaded=m3d::v16::loadScene(sceneFile);
 if(!loaded){std::cerr<<"Scene load: "<<loaded.error.toStdString()<<"\n";return 2;}
 const auto built=m3d::v16::buildRuntime(loaded.scene,1024);
 if(!built){std::cerr<<"Scene build: "<<built.error.toStdString()<<"\n";return 2;}
 const auto& scene=built.runtime.renderScene;
 if(built.runtime.cameras.isEmpty()){std::cerr<<"No scene cameras\n";return 2;}
 m3d::updateLighting(14.75f);
 m3d::Stats shadowStats{};
 const auto shadow=m3d::buildShadow(scene,shadowStats);
 const auto base=built.runtime.cameras.first();
 std::vector<double> dxTimes,vkTimes;
 QJsonArray samples;
 std::vector<uint32_t> dxPixels,vkPixels;
 uint64_t dxHash=0,vkHash=0;
 for(int i=-2;i<frames;i++){
  auto preset=base;
  float frac=float(std::max(0,i))/float(std::max(1,frames-1));
  float dx=(frac-.5f)*2.5f;
  preset.eye.x+=dx;preset.target.x+=dx;
  const auto camera=m3d::v16::makeCamera(preset,width,height);
  m3d::Stats dxStats{},vkStats{};
  std::wstring dxError,vkError;
  const auto beginDX=Clock::now();
  if(!m3d::renderDirectX11Preview(scene,shadow,camera,width,height,dxPixels,dxStats,dxError)){
   std::wcerr<<L"D3D11 failure: "<<dxError<<L"\n";return 3;
  }
  const auto endDX=Clock::now();
  const auto beginVK=Clock::now();
  if(!m3d::renderVulkanPreview(scene,shadow,camera,width,height,vkPixels,vkStats,vkError)){
   std::wcerr<<L"Vulkan failure: "<<vkError<<L"\n";return 4;
  }
  const auto endVK=Clock::now();
  if(i<0)continue; // first two rounds heat up scene/PBR and GPU pipelines
  const double dxMs=std::chrono::duration<double,std::milli>(endDX-beginDX).count();
  const double vkMs=std::chrono::duration<double,std::milli>(endVK-beginVK).count();
  dxTimes.push_back(dxMs);vkTimes.push_back(vkMs);
  if(i==0){png(QDir(outDir).filePath("dx11.png"),dxPixels,width,height);
           png(QDir(outDir).filePath("vulkan.png"),vkPixels,width,height);}
  dxHash=checksum(dxPixels);vkHash=checksum(vkPixels);
  QJsonObject s;
  s["frame"]=i;
  s["dx11_wall_ms"]=dxMs;s["vulkan_wall_ms"]=vkMs;
  s["dx11_draw_ms"]=dxStats.gpuTimingValid?QJsonValue(dxStats.gpuDrawMs):QJsonValue();
  s["vulkan_draw_ms"]=vkStats.gpuTimingValid?QJsonValue(vkStats.gpuDrawMs):QJsonValue();
  s["vulkan_submit_ms"]=vkStats.cpuSubmitMs;
  s["vulkan_pixel_convert_ms"]=vkStats.cpuPixelConvertMs;
  s["vulkan_frame_hash"]=QString::number(checksum(vkPixels),16);
  s["vulkan_fence_wait_ms"]=vkStats.cpuMapWaitMs;
  s["vulkan_draw_calls"]=qint64(vkStats.gpuDrawCalls);
  s["vulkan_box_instances"]=qint64(vkStats.gpuBoxInstances);
  s["vulkan_foliage_instances"]=qint64(vkStats.gpuFoliageInstances);
  samples.append(s);
  std::cout<<i<<" dx11="<<dxMs<<"ms vulkan="<<vkMs<<"ms GPU Vulkan="<<vkStats.gpuDrawMs<<"ms\n";
 }
 QJsonObject doc;
 doc["target"]="CSE WARP V17 Vulkan API 1.3.204 minimum on loader and GPU";
 doc["scene"]=sceneFile;doc["width"]=width;doc["height"]=height;doc["frames"]=frames;
 const char* shaderMode=std::getenv("CSE_WARP_VK_FAST_FRAGMENT");
 doc["vulkan_fragment_profile"]=(shaderMode && std::strcmp(shaderMode,"1")==0)?"optimized":"baseline";
 const char* readbackMode=std::getenv("CSE_WARP_VK_BGRA_READBACK");
 doc["vulkan_readback_profile"]=(readbackMode && std::strcmp(readbackMode,"1")==0)?"bgra8_native":"rgba8_legacy";
 doc["dx11_median_ms"]=median(dxTimes);doc["vulkan_median_ms"]=median(vkTimes);
 doc["dx11_p95_ms"]=p95(dxTimes);doc["vulkan_p95_ms"]=p95(vkTimes);
 doc["vulkan_vs_dx11_median_speedup"]=median(vkTimes)>0?median(dxTimes)/median(vkTimes):0.0;
 doc["dx11_last_hash"]=QString::number(dxHash,16);
 doc["vulkan_last_hash"]=QString::number(vkHash,16);
 doc["pixel_parity_claim"]=false; // first-port PBR/shader parity explicitly NOT established
 doc["samples"]=samples;
 QFile f(QDir(outDir).filePath("ab_results.json"));
 if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)||
    f.write(QJsonDocument(doc).toJson(QJsonDocument::Indented))<=0){
  std::cerr<<"Failed to write benchmark JSON\n";return 5;
 }
 std::cout<<"A/B median D3D11="<<median(dxTimes)<<"ms Vulkan="<<median(vkTimes)<<"ms\n";
 return 0;
}
