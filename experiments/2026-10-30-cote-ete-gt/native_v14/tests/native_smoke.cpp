#include "../m3d_engine.h"
#include <iostream>
int main(){
 using namespace m3d;Scene scene;std::wstring error;
 if(!loadScene(L"moteur3d_v1314_scene.bin",scene,error)){std::wcerr<<L"Chargement scène échoué: "<<error<<L"\n";return 1;}
 if(scene.obj.empty()||scene.node.empty()||scene.mat.empty()||scene.root<0)return 2;
 const auto sea=std::find_if(scene.mat.begin(),scene.mat.end(),[](const Material&m){return m.name=="sea";});
 const auto car=std::find_if(scene.mat.begin(),scene.mat.end(),[](const Material&m){return m.name=="carSilver";});
 const auto metal=std::find_if(scene.mat.begin(),scene.mat.end(),[](const Material&m){return m.name=="metal";});
 if(sea==scene.mat.end()||sea->reflectivityPercent!=51||car==scene.mat.end()||car->reflectivityPercent!=95||metal==scene.mat.end()||metal->reflectivityPercent!=35||physics::physXAvailable())return 6;
 const auto triangles=std::count_if(scene.obj.begin(),scene.obj.end(),[](const Obj&o){return o.kind==Kind::Triangle;});
 if(!triangles||scene.order.size()!=scene.obj.size())return 3;
 updateLighting(12.f);Stats shadowStats;auto shadows=buildShadow(scene,shadowStats);
 constexpr int w=192,h=108;std::vector<uint32_t> pixels(size_t(w)*h),parallelPixels(size_t(w)*h),fsaaPixels(size_t(w)*h);std::atomic<uint64_t> generation{1};
 Camera cam=camera(-43.f,7.f,12.f,w,h);cam.tanHalf=std::tan(50.f*PI/360.f);cam.pixelAngle=2*cam.tanHalf/w;
 Stats stats=render(scene,shadows,cam,w,h,pixels,1,false,generation,1);Stats parallelStats=render(scene,shadows,cam,w,h,parallelPixels,32,false,generation,1);Stats fsaaStats=render(scene,shadows,cam,w,h,fsaaPixels,32,false,generation,1,16);
 std::vector<uint32_t> colors=pixels;std::sort(colors.begin(),colors.end());colors.erase(std::unique(colors.begin(),colors.end()),colors.end());
 if(!stats.rays||!stats.nodes||!stats.hits||parallelStats.threads!=32||pixels!=parallelPixels||fsaaStats.rays<=parallelStats.rays||colors.size()<16){std::cerr<<"Rendu de contrôle vide.\n";return 4;}
 std::ofstream bad("invalid-scene-smoke.bin",std::ios::binary);bad.write("M3D1314",7);bad.close();Scene rejected;std::wstring badError;
 if(loadScene(L"invalid-scene-smoke.bin",rejected,badError)||badError.empty()){std::cerr<<"En-tête tronqué accepté.\n";return 5;}
 std::remove("invalid-scene-smoke.bin");
 std::cout<<"Smoke OK: objects="<<scene.obj.size()<<" triangles="<<triangles<<" nodes="<<scene.node.size()<<" rays="<<stats.rays<<" hits="<<stats.hits<<" threads="<<parallelStats.threads<<" fsaa16_rays="<<fsaaStats.rays<<" colors="<<colors.size()<<"\n";return 0;
}
