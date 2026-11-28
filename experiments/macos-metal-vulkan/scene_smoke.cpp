#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QImage>
#include <QDir>
#include <iostream>
#include <cstring>
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if(argc!=3)return 1;
 const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
 if(!loaded){std::cerr<<loaded.error.toStdString()<<"\n";return 2;}
 const auto built=m3d::v16::buildRuntime(loaded.scene,1024);
 if(!built||built.runtime.cameras.isEmpty()){std::cerr<<built.error.toStdString()<<"\n";return 3;}
 auto& scene=built.runtime.renderScene;
 std::cout<<"Scene objects="<<scene.obj.size()<<" cameras="<<built.runtime.cameras.size()<<" AVX2="<<m3d::avx2FmaAvailable()<<std::endl;
 m3d::updateLighting(14.f);m3d::Stats shadowStats{};auto shadow=m3d::buildShadow(scene,shadowStats);
 const int w=640,h=360;
 auto camera=m3d::v16::makeCamera(built.runtime.cameras.first(),w,h);
 QDir dir(QString::fromLocal8Bit(argv[2]));dir.mkpath(".");
 for(int i=0;i<3;i++){
  m3d::Stats st{};std::wstring error;std::vector<uint32_t> pixels;
  if(!m3d::renderVulkanPreview(scene,shadow,camera,w,h,pixels,st,error)){std::wcerr<<error<<std::endl;return 4;}
  if(!st.gpu||pixels.size()!=size_t(w*h)||std::all_of(pixels.begin(),pixels.end(),[&](auto p){return p==pixels.front();}))return 5;
  QImage img(w,h,QImage::Format_ARGB32);std::memcpy(img.bits(),pixels.data(),pixels.size()*4);
  if(!img.save(dir.filePath(QString("vulkan-scene-%1.png").arg(i))))return 6;
  std::cout<<"Vulkan full scene frame="<<i<<" ms="<<st.ms<<" draws="<<st.gpuDrawCalls<<std::endl;
 }
 return 0;
}
