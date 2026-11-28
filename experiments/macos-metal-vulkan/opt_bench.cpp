#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <chrono>
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if(argc!=5)return 1;
 const int w=std::atoi(argv[3]),h=std::atoi(argv[4]);
 if(w<1||h<1||w>4096||h>4096)return 1;
 const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
 if(!loaded){std::cerr<<loaded.error.toStdString()<<"\n";return 2;}
 const auto built=m3d::v16::buildRuntime(loaded.scene,1024);
 if(!built||built.runtime.cameras.isEmpty())return 3;
 const auto& scene=built.runtime.renderScene;
 m3d::updateLighting(14.f);m3d::Stats shadowStats{};const auto shadow=m3d::buildShadow(scene,shadowStats);
 const auto base=built.runtime.cameras.first();
 QDir dir(QString::fromLocal8Bit(argv[2]));if(!dir.mkpath("."))return 4;
 QJsonArray rows;std::vector<uint32_t> pixels;
 for(int i=-3;i<24;++i){
  auto pose=base;
  if(i>=12){float offset=float(i-12)*.12f;pose.eye.x+=offset;pose.target.x+=offset;}
  const auto cam=m3d::v16::makeCamera(pose,w,h);
  m3d::Stats st{};std::wstring error;
  const auto start=std::chrono::steady_clock::now();
  if(!m3d::renderVulkanPreview(scene,shadow,cam,w,h,pixels,st,error)){std::wcerr<<error<<std::endl;return 5;}
  const double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  if(!st.gpu||pixels.size()!=size_t(w)*h||std::all_of(pixels.begin(),pixels.end(),[&](auto p){return p==pixels.front();}))return 6;
  auto bytes=QByteArray::fromRawData(reinterpret_cast<const char*>(pixels.data()),qsizetype(pixels.size()*4));
  const auto hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
  rows.append(QJsonObject{{"frame",i},{"phase",i<0?"warmup":i<12?"static":"moving"},{"wall_ms",wall},{"renderer_ms",st.ms},{"submit_ms",st.cpuSubmitMs},{"fence_ms",st.cpuMapWaitMs},{"readback_cpu_ms",st.cpuPixelConvertMs},{"draws",qint64(st.gpuDrawCalls)},{"gpu_valid",st.gpuTimingValid},{"gpu_ms",st.gpuTimingValid?QJsonValue(st.gpuTotalMs):QJsonValue()},{"sha256",hash}});
  if(i==0||i==23){
   QImage img(w,h,QImage::Format_ARGB32);std::memcpy(img.bits(),pixels.data(),pixels.size()*4);
   if(!img.save(dir.filePath(QString("frame-%1.png").arg(i))))return 7;
  }
  std::cout<<"frame "<<i<<" ms="<<wall<<" draws="<<st.gpuDrawCalls<<" hash="<<hash.toStdString()<<std::endl;
 }
 QFile f(dir.filePath("frames.json"));if(!f.open(QIODevice::WriteOnly))return 8;
 f.write(QJsonDocument(rows).toJson());return 0;
}
