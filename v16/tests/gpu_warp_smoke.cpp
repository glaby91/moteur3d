#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QDebug>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

static void logger(QtMsgType,const QMessageLogContext&,const QString& message){
 const QByteArray text=message.toUtf8();
 std::fprintf(stderr,"V16 GPU WARP TEST: %s\n",text.constData());
 std::fflush(stderr);
}
int main(int argc,char** argv) {
 qInstallMessageHandler(logger);
 QCoreApplication app(argc,argv);
 if(argc!=3){qCritical()<<"Expected scene.json and GPU PNG output path";return 2;}
 const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
 if(!loaded){qCritical()<<loaded.error;return 1;}
 auto runtime=m3d::v16::buildRuntime(loaded.scene,512);
 if(!runtime){qCritical()<<runtime.error;return 1;}
 if(runtime.runtime.cameras.isEmpty()){qCritical()<<"Missing camera";return 1;}
 m3d::updateLighting(runtime.runtime.daylightHour);
 constexpr int width=1920,height=1080; // Full HD GPU WARP reference screenshot
 const auto cam=m3d::v16::makeCamera(runtime.runtime.cameras.front(),width,height);
 std::vector<uint32_t> pixels;
 m3d::Stats stats;std::wstring error;
 if(!m3d::renderCompute11Preview(runtime.runtime.renderScene,cam,width,height,pixels,stats,error)){
  qCritical()<<"D3D11 WARP rendering failed:"<<QString::fromStdWString(error);return 1;
 }
 if(!stats.gpu||pixels.size()!=size_t(width)*height){qCritical()<<"Incomplete WARP image";return 1;}
 std::set<uint32_t> colors(pixels.begin(),pixels.end());
 if(colors.size()<128){qCritical()<<"WARP image has too few colors:"<<colors.size();return 1;}
 QImage image(width,height,QImage::Format_ARGB32);
 for(int y=0;y<height;++y)
  std::memcpy(image.scanLine(y),pixels.data()+size_t(y)*width,width*4);
 if(!image.save(QString::fromLocal8Bit(argv[2]),"PNG")){qCritical()<<"Cannot write WARP PNG";return 1;}
 // The cached render must be pixel-identical for unchanged scene and camera.
 std::vector<uint32_t> warmPixels;
 m3d::Stats warmStats;std::wstring warmError;
 if(!m3d::renderCompute11Preview(runtime.runtime.renderScene,cam,width,height,
                                 warmPixels,warmStats,warmError)||warmPixels!=pixels){
  qCritical()<<"Cached WARP rendering differs from cold result:"<<QString::fromStdWString(warmError);
  return 1;
 }
 // Editing a material must invalidate the GPU atlas and scene cache.
 auto& renderScene=runtime.runtime.renderScene;
 if(renderScene.mat.empty()){qCritical()<<"Missing materials";return 1;}
 const float originalMetallic=renderScene.mat[0].metallic;
 renderScene.mat[0].metallic=originalMetallic>.5f?0.f:1.f;
 std::vector<uint32_t> editedPixels;
 m3d::Stats editedStats;std::wstring editedError;
 if(!m3d::renderCompute11Preview(renderScene,cam,width,height,
                                 editedPixels,editedStats,editedError)||editedPixels==pixels){
  qCritical()<<"GPU scene cache not invalidated on material edit:"<<QString::fromStdWString(editedError);
  return 1;
 }
 renderScene.mat[0].metallic=originalMetallic;
 qInfo()<<"D3D11 WARP cold:"<<stats.ms<<"ms, cached:"<<warmStats.ms
        <<"ms, edited:"<<editedStats.ms<<"ms. Cache output and invalidation PASS.";
 qInfo()<<"D3D11 WARP shader executed:"<<width<<"x"<<height
        <<"unique colors:"<<colors.size()<<"milliseconds:"<<stats.ms;
 return 0;
}
