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
 std::vector<uint32_t> upscaleInput{0xff010203,0xff102030,0xff405060,0xff708090};
 auto enlarged=upscaleBilinear(upscaleInput,2,2,4,4,1);
 if(enlarged.front()!=upscaleInput.front()||enlarged.back()!=upscaleInput.back())return 7;
 std::vector<uint32_t> edgeInput(9,0xff000000);edgeInput[4]=0xff000064;
 auto softened=antialiasEdges4(edgeInput,3,3,1);
 if((softened[4]&255)!=55)return 8;
 Scene deep;constexpr int internalCount=130,leafCount=internalCount+1;deep.root=0;deep.node.resize(internalCount+leafCount);deep.obj.resize(leafCount);deep.order.resize(leafCount);
 for(int i=0;i<leafCount;i++){Obj&o=deep.obj[i];o.kind=Kind::Ellipse;o.mat=0;o.mn={-1,-1,-1};o.mx={1,1,1};o.a={0,0,0};o.b={1,1,1};deep.order[i]=(uint32_t)i;}
 for(int i=0;i<internalCount;i++){Node&n=deep.node[i];n.mn={-1,-1,-1};n.mx={1,1,1};n.left=internalCount+i;n.right=i+1<internalCount?i+1:internalCount+internalCount;n.start=0;n.count=0;}
 for(int i=0;i<leafCount;i++){Node&n=deep.node[internalCount+i];n.mn={-1,-1,-1};n.mx={1,1,1};n.start=i;n.count=1;}
 Stats deepStats;Hit deepHit;closest(deep,Ray{{0,0,-4},{0,0,1}},deepHit,&deepStats,false);
 if(deepHit.id<0||deepStats.nodes!=deep.node.size())return 9;
 updateLighting(12.f);Stats shadowStats;auto shadows=buildShadow(scene,shadowStats);
 for(int y=2;y<shadows.size-2;y++)for(int x=2;x<shadows.size-2;x++){int i=y*shadows.size+x;for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){float d=shadows.d[(y+dy)*shadows.size+x+dx];if(shadows.min[i]>d||shadows.max[i]<d)return 10;}}
 constexpr int w=192,h=108;std::vector<uint32_t> pixels(size_t(w)*h),parallelPixels(size_t(w)*h),fsaaPixels(size_t(w)*h);std::atomic<uint64_t> generation{1};
 Camera cam=camera(-43.f,7.f,12.f,w,h);cam.tanHalf=std::tan(50.f*PI/360.f);cam.pixelAngle=2*cam.tanHalf/w;
 Stats stats=render(scene,shadows,cam,w,h,pixels,1,false,generation,1);Stats parallelStats=render(scene,shadows,cam,w,h,parallelPixels,32,false,generation,1);Stats fsaaStats=render(scene,shadows,cam,w,h,fsaaPixels,32,false,generation,1,16);
 std::vector<uint32_t> colors=pixels;std::sort(colors.begin(),colors.end());colors.erase(std::unique(colors.begin(),colors.end()),colors.end());
 if(!stats.rays||!stats.nodes||!stats.hits||parallelStats.threads!=32||pixels!=parallelPixels||fsaaStats.rays<=parallelStats.rays||colors.size()<16){std::cerr<<"Rendu de contrôle vide.\n";return 4;}
 std::ofstream bad("invalid-scene-smoke.bin",std::ios::binary);bad.write("M3D1314x",8);bad.close();Scene rejected;std::wstring badError;
 if(loadScene(L"invalid-scene-smoke.bin",rejected,badError)||badError.empty()){std::cerr<<"Signature invalide acceptée.\n";return 5;}
 {std::ofstream truncated("invalid-scene-smoke.bin",std::ios::binary|std::ios::trunc);truncated.write("M3D1314",7);}
 if(loadScene(L"invalid-scene-smoke.bin",rejected,badError)||badError.empty()){std::cerr<<"En-tête tronqué accepté.\n";return 11;}
 std::remove("invalid-scene-smoke.bin");
 std::cout<<"Smoke OK: objects="<<scene.obj.size()<<" triangles="<<triangles<<" nodes="<<scene.node.size()<<" rays="<<stats.rays<<" hits="<<stats.hits<<" threads="<<parallelStats.threads<<" fsaa16_rays="<<fsaaStats.rays<<" colors="<<colors.size()<<"\n";return 0;
}
