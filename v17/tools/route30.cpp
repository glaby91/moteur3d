#include "m3d_engine.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iostream>
using namespace m3d;
using Clock=std::chrono::steady_clock;
static double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if(argc!=3)return 2;
 auto scene=v16::loadScene(app.arguments()[1]); if(!scene){std::cerr<<scene.error.toStdString();return 3;}
 auto built=v16::buildRuntime(scene.scene,2048); if(!built){std::cerr<<built.error.toStdString();return 4;}
 const auto& rt=built.runtime;
 auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[](const auto& c){return c.id=="v7_pool_fresco";});
 if(it==rt.cameras.end()){std::cerr<<"Missing v7_pool_fresco";return 5;}
 updateLighting(14.75f); setRasterCloudTimeScale(0.f);
 Stats ss{};auto shadow=buildShadow(rt.renderScene,ss);
 QFile file(app.arguments()[2]);if(!file.open(QIODevice::WriteOnly|QIODevice::Text))return 6;
 QTextStream csv(&file);csv<<"backend,camera,hour,width,height,frames_completed_30s,elapsed_s,frames_started,mean_render_ms,fps_30s\n";
 for(int backend=0;backend<3;++backend){
 const char* name=backend==0?"DX11":backend==1?"Vulkan":"LumaTrace";
 std::vector<uint32_t> pixels; std::wstring reason;
 auto render=[&](double t){
 auto pose=*it;
 float dx=float(2.7*std::sin(6.283185307179586*t/30.0));
 pose.eye.x+=dx;pose.target.x+=dx;
 auto cam=v16::makeCamera(pose,1000,680);Stats stats{};
 if(backend==0)return renderDirectX11Preview(rt.renderScene,shadow,cam,1000,680,pixels,stats,reason);
 if(backend==1)return renderVulkanPreview(rt.renderScene,shadow,cam,1000,680,pixels,stats,reason);
 return renderLumaTracePreview(rt.renderScene,rt.lumaTraceBvh4,shadow,cam,1000,680,pixels,stats,reason);
 };
 std::cout<<name<<" warmup"<<std::endl;
 for(int i=0;i<2;++i)if(!render(0)){std::wcerr<<reason;return 7;}
 int completed=0,started=0;double renderTime=0;
 auto start=Clock::now();double t=0;
 while(t<30){auto before=Clock::now();if(!render(t)){std::wcerr<<reason;return 8;}
 renderTime+=seconds(before);++started;t=seconds(start);if(t<=30)++completed;}
 csv<<name<<",v7_pool_fresco,14.75,1000,680,"<<completed<<','<<t<<','<<started<<','<<(1000*renderTime/started)<<','<<(completed/30.)<<'\n';csv.flush();
 std::cout<<name<<" "<<completed<<" frames / 30 s"<<std::endl;
 }
 return 0;
}
