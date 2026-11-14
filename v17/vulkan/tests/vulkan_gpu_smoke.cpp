// Real Vulkan GPU offscreen smoke to run on HD620; CI compiles it without requiring a Vulkan ICD.
#include "m3d_engine.h"
#include <QImage>
#include <chrono>
#include <filesystem>
#include <iostream>
int main(int argc,char**argv){
    using namespace m3d;
    const int w=320,h=180;
    Scene scene;
    scene.mat.resize(1);
    scene.mat[0].name="test_surface";
    ColorLevel img;img.size=4;img.p.resize(16,0x00e2b07cu);
    scene.mat[0].colorMip.push_back(std::move(img));
    Obj o{};o.kind=Kind::Triangle;o.mat=0;
    o.a={-1.f,0.f,-2.f};o.b={1.f,0.f,-2.f};o.c={0.f,2.f,-2.f};
    o.n0=o.n1=o.n2={0.f,0.f,1.f};
    o.uv0={0.f,0.f,0.f};o.uv1={1.f,0.f,0.f};o.uv2={.5f,1.f,0.f};
    o.mn={-1.f,0.f,-2.f};o.mx={1.f,2.f,-2.f};
    scene.obj.push_back(o);
    scene.gpuCacheIdentity=0x13579u;
    Camera c{};c.eye={0.f,1.f,2.f};c.f={0.f,0.f,-1.f};c.r={1.f,0.f,0.f};
    c.u={0.f,1.f,0.f};c.aspect=float(w)/h;c.tanHalf=.7f;
    ShadowMap sm{};sm.size=1;sm.d={1e9f};
    std::vector<uint32_t> pixels;
    Stats stats{};std::wstring error;
    if(!renderVulkanPreview(scene,sm,c,w,h,pixels,stats,error)){
        std::wcerr<<L"Vulkan renderer failed: "<<error<<L"\n";return 2;
    }
    // A single opaque triangle must be submitted twice: Z-only + shaded.
    if(pixels.size()!=size_t(w*h)||!stats.gpu||stats.gpuDrawCalls!=2){
        std::cerr<<"Expected exactly two draw calls for always-on Vulkan Z-prepass; got "
                 <<stats.gpuDrawCalls<<"\n";return 3;
    }
    bool nonBackground=false;
    const auto ref=pixels.front();
    for(auto p:pixels)if(p!=ref){nonBackground=true;break;}
    if(!nonBackground){std::cerr<<"Render is flat / no visible triangles\n";return 4;}
    if(argc>=2){
        QImage out(w,h,QImage::Format_ARGB32);
        memcpy(out.bits(),pixels.data(),pixels.size()*sizeof(uint32_t));
        if(!out.save(QString::fromLocal8Bit(argv[1])))return 5;
    }
    std::cout<<"PASS Vulkan hardware GPU raster "<<w<<"x"<<h
             <<" draw_ms="<<stats.gpuDrawMs<<" total_ms="<<stats.ms<<"\n";
    return 0;
}
