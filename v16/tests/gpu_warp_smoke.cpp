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
 // Verify quality/performance switch against a true full-resolution
 // secondary-ray reference at the same camera, scene, lighting and sample count.
 // A small image keeps D3D11 software WARP test costs bounded.
 constexpr int abWidth=320,abHeight=180;
 const auto abCam=m3d::v16::makeCamera(runtime.runtime.cameras.front(),abWidth,abHeight);
 qputenv("M3D_V16_GPU_GI_ADAPTIVE",QByteArrayLiteral("0"));
 std::vector<uint32_t> fullGiPixels;
 m3d::Stats fullGiStats;std::wstring fullGiError;
 const bool fullGiOk=m3d::renderCompute11Preview(
     runtime.runtime.renderScene,abCam,abWidth,abHeight,
     fullGiPixels,fullGiStats,fullGiError);
 qputenv("M3D_V16_GPU_GI_ADAPTIVE",QByteArrayLiteral("1"));
 std::vector<uint32_t> halfGiPixels;
 m3d::Stats halfGiStats;std::wstring halfGiError;
 const bool halfGiOk=m3d::renderCompute11Preview(
     runtime.runtime.renderScene,abCam,abWidth,abHeight,
     halfGiPixels,halfGiStats,halfGiError);
 qunsetenv("M3D_V16_GPU_GI_ADAPTIVE");
 if(!fullGiOk||!halfGiOk||fullGiPixels.size()!=size_t(abWidth)*abHeight||
    halfGiPixels.size()!=fullGiPixels.size()) {
  qCritical()<<"Adaptive 2x2 GI mode comparison failed:"
             <<QString::fromStdWString(fullGiError)
             <<QString::fromStdWString(halfGiError);return 1;
 }
 double totalAbs=0.;
 uint64_t changedPixels=0;
 for(size_t i=0;i<fullGiPixels.size();++i){
  if(fullGiPixels[i]!=halfGiPixels[i])++changedPixels;
  for(int bit: {0,8,16}){
   const int a=int((fullGiPixels[i]>>bit)&255u);
   const int b=int((halfGiPixels[i]>>bit)&255u);
   totalAbs+=std::abs(a-b);
  }
 }
 const double meanAbs=totalAbs/double(fullGiPixels.size()*3u);
 if(changedPixels==0||meanAbs>65.){
  qCritical()<<"Adaptive GI invalid or severe visual regression:"
             <<meanAbs<<"levels RGB/255 changed pixels"<<changedPixels;
  return 1;
 }
 qInfo()<<"GI full-res vs adaptive 2x2 MAE"<<meanAbs
        <<"full-res ms"<<fullGiStats.ms<<"adaptive ms"<<halfGiStats.ms;
 // Confirm that the cached half-resolution GI can be reused by the full
 // 1920x1080 path after a resolution/mode switch, without stale buffers.
 std::vector<uint32_t> afterSwitchPixels;
 m3d::Stats afterSwitchStats;std::wstring afterSwitchError;
 if(!m3d::renderCompute11Preview(runtime.runtime.renderScene,cam,width,height,
                                 afterSwitchPixels,afterSwitchStats,afterSwitchError)||
    afterSwitchPixels!=pixels){
  qCritical()<<"GI cache invalidated incorrectly after changing resolution:"
             <<QString::fromStdWString(afterSwitchError);return 1;
 }
 // Confirm that the new secondary-bounce quality path genuinely changes
 // the final image, and can be disabled for Intel iGPU performance comparison.
 qputenv("M3D_V16_GPU_GI_SAMPLES",QByteArrayLiteral("0"));
 std::vector<uint32_t> withoutBouncePixels;
 m3d::Stats withoutBounceStats;std::wstring withoutBounceError;
 const bool withoutBounceOk=m3d::renderCompute11Preview(
     runtime.runtime.renderScene,cam,width,height,withoutBouncePixels,
     withoutBounceStats,withoutBounceError);
 qunsetenv("M3D_V16_GPU_GI_SAMPLES");
 if(!withoutBounceOk||withoutBouncePixels.size()!=pixels.size()||
    withoutBouncePixels==pixels){
  qCritical()<<"GPU secondary-bounce option did not affect actual pixels:"
             <<QString::fromStdWString(withoutBounceError);
  return 1;
 }
 // Compare identical scenes/images with indirect=0 and indirect=2 in CI.
 const QFileInfo baseImagePath(QString::fromLocal8Bit(argv[2]));
 const QString directOnlyPath=baseImagePath.absolutePath()+"/"+
                              baseImagePath.completeBaseName()+"-direct-only.png";
 QImage directOnlyImage(width,height,QImage::Format_ARGB32);
 for(int y=0;y<height;++y)
     std::memcpy(directOnlyImage.scanLine(y),
                 withoutBouncePixels.data()+size_t(y)*width,size_t(width)*4);
 if(!directOnlyImage.save(directOnlyPath,"PNG")){
  qCritical()<<"Unable to write direct-only GPU reference"<<directOnlyPath;
  return 1;
 }
 // Restore the normal two-sample path and require byte-for-byte determinism.
 std::vector<uint32_t> restoredBouncePixels;
 m3d::Stats bounceStats;std::wstring bounceError;
 if(!m3d::renderCompute11Preview(runtime.runtime.renderScene,cam,width,height,
                                 restoredBouncePixels,bounceStats,bounceError)||
                                 restoredBouncePixels!=pixels){
  qCritical()<<"GPU secondary-bounce restoration failed:"
             <<QString::fromStdWString(bounceError);
  return 1;
 }
 // The sunlight changes with time of day without replacing geometry.
 // V16 retains exactly the same cached triangles/PBR textures across this test.
 m3d::updateLighting(17.0f);
 std::vector<uint32_t> eveningPixels;
 m3d::Stats eveningStats;std::wstring eveningError;
 if(!m3d::renderCompute11Preview(runtime.runtime.renderScene,cam,width,height,
                                 eveningPixels,eveningStats,eveningError)||
                                 eveningPixels==pixels){
  qCritical()<<"Changing sun hour did not change GPU Full HD illumination:"
             <<QString::fromStdWString(eveningError);return 1;
 }
 QImage eveningImage(width,height,QImage::Format_ARGB32);
 for(int y=0;y<height;++y)
  std::memcpy(eveningImage.scanLine(y),
              eveningPixels.data()+size_t(y)*width,size_t(width)*4);
 const QFileInfo target(QString::fromLocal8Bit(argv[2]));
    const QString eveningPath=target.absolutePath()+"/"+target.completeBaseName()+"-17h.png";
 if(!eveningImage.save(eveningPath,"PNG")){
  qCritical()<<"Unable to save GPU sunset Full HD image";return 1;
 }
 m3d::updateLighting(runtime.runtime.daylightHour);
 std::vector<uint32_t> restoredPixels;
 m3d::Stats restoredStats;std::wstring restoredError;
 if(!m3d::renderCompute11Preview(runtime.runtime.renderScene,cam,width,height,
                                 restoredPixels,restoredStats,restoredError)||
                                 restoredPixels!=pixels){
  qCritical()<<"Restoring daylight does not restore identical GPU pixels:"
             <<QString::fromStdWString(restoredError);return 1;
 }
 // Editing a material must invalidate the GPU atlas and scene cache.
 auto& renderScene=runtime.runtime.renderScene;
 if(renderScene.mat.empty()){qCritical()<<"Missing materials";return 1;}
 const float originalMetallic=renderScene.mat[0].metallic;
 renderScene.mat[0].metallic=originalMetallic>.5f?0.f:1.f;
 renderScene.markGpuContentChanged(); // explicit V16 editor-like mutation
 std::vector<uint32_t> editedPixels;
 m3d::Stats editedStats;std::wstring editedError;
 if(!m3d::renderCompute11Preview(renderScene,cam,width,height,
                                 editedPixels,editedStats,editedError)||editedPixels==pixels){
  qCritical()<<"GPU scene cache not invalidated on material edit:"<<QString::fromStdWString(editedError);
  return 1;
 }
 renderScene.mat[0].metallic=originalMetallic;
 renderScene.markGpuContentChanged();
 // Legacy and external users without a tracked identity must continue to
 // detect in-place material edits using the complete scene content hash.
 renderScene.gpuCacheIdentity=0;
 std::vector<uint32_t> untrackedPixels;
 m3d::Stats fallbackStats;std::wstring fallbackError;
 if(!m3d::renderCompute11Preview(renderScene,cam,width,height,
                                 untrackedPixels,fallbackStats,fallbackError)||
                                 untrackedPixels!=pixels){
  qCritical()<<"Legacy full-hash fallback changed reference image:"
             <<QString::fromStdWString(fallbackError);
  return 1;
 }
 renderScene.mat[0].metallic=originalMetallic>.5f?0.f:1.f;
 std::vector<uint32_t> untrackedEdited;
 m3d::Stats fallbackEditedStats;std::wstring fallbackEditedError;
 if(!m3d::renderCompute11Preview(renderScene,cam,width,height,
                                 untrackedEdited,fallbackEditedStats,fallbackEditedError)||
                                 untrackedEdited==untrackedPixels){
  qCritical()<<"Legacy full-hash cache failed to detect untracked material edit:"
             <<QString::fromStdWString(fallbackEditedError);
  return 1;
 }
 renderScene.mat[0].metallic=originalMetallic;
 qInfo()<<"D3D11 WARP cold:"<<stats.ms<<"ms, cached:"<<warmStats.ms
        <<"ms, edited:"<<editedStats.ms<<"ms. Cache output and invalidation PASS.";
 qInfo()<<"D3D11 WARP shader executed:"<<width<<"x"<<height
        <<"unique colors:"<<colors.size()<<"milliseconds:"<<stats.ms;
 return 0;
}
