// CSE WARP V17.32 Khronos diagnostic.
// This executable calls the ORIGINAL m3d::renderVulkanDirectPresent(), using
// its actual VulkanRaster, VkSurfaceKHR, VkSwapchainKHR, shader pipeline and GPU.
// A synthetic scene avoids scene-loading/asset IO as a confounding variable.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "m3d_engine.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
int main(){std::cerr<<"Native Vulkan Win32 benchmark requires Windows\n";return 2;}
#else
using Clock=std::chrono::steady_clock;
namespace {
constexpr float pi=3.14159265358979323846f;
m3d::V rotateY(m3d::V v,float a){
    return {v.x*std::cos(a)+v.z*std::sin(a),v.y,-v.x*std::sin(a)+v.z*std::cos(a)};
}
void addSpinningCube(m3d::Scene& scene,float angle){
    static constexpr int faces[6][4]={
       {0,1,3,2},{4,6,7,5},{0,2,6,4},
       {1,5,7,3},{0,4,5,1},{2,3,7,6}};
    m3d::V p[8]{};
    for(int i=0;i<8;i++){
        const float x=(i&1)?1.f:-1.f;
        const float y=(i&2)?1.f:-1.f;
        const float z=(i&4)?1.f:-1.f;
        p[i]=rotateY({x,y,z},angle);
    }
    scene.obj.clear();scene.obj.reserve(12);
    for(const auto& quad:faces){
        const int ts[2][3]={{quad[0],quad[1],quad[2]},
                            {quad[0],quad[2],quad[3]}};
        for(const auto& tri:ts){
            m3d::Obj o{};o.kind=m3d::Kind::Triangle;o.mat=0;
            o.a=p[tri[0]];o.b=p[tri[1]];o.c=p[tri[2]];
            const auto n=m3d::norm(m3d::cross(o.b-o.a,o.c-o.a));
            o.n0=o.n1=o.n2=n;
            o.uv0={0.f,0.f,0.f};o.uv1={1.f,0.f,0.f};o.uv2={1.f,1.f,0.f};
            o.mn={std::min({o.a.x,o.b.x,o.c.x}),std::min({o.a.y,o.b.y,o.c.y}),std::min({o.a.z,o.b.z,o.c.z})};
            o.mx={std::max({o.a.x,o.b.x,o.c.x}),std::max({o.a.y,o.b.y,o.c.y}),std::max({o.a.z,o.b.z,o.c.z})};
            scene.obj.push_back(o);
        }
    }
}
m3d::Camera makeCamera(float elapsed,int w,int h,const std::string& motion){
    m3d::Camera c{};
    m3d::V look{0.f,0.f,0.f};
    if(motion=="orbit"){
        c.eye={6.2f*std::sin(elapsed*.48f),2.2f,6.2f*std::cos(elapsed*.48f)};
    }else if(motion=="translate"){
        const float x=2.0f*std::sin(elapsed*.65f);
        c.eye={x,2.0f,7.f};look={x,0.f,0.f};
    }else{
        c.eye={0.f,1.9f,7.f};
    }
    c.f=m3d::norm(look-c.eye);
    c.r=m3d::norm(m3d::cross(c.f,{0.f,1.f,0.f}));
    c.u=m3d::norm(m3d::cross(c.r,c.f));
    c.tanHalf=.65f;
    c.aspect=float(w)/float(h);
    c.pixelAngle=.65f/float(w);
    return c;
}
m3d::Scene makeScene(int cubes,int lights){
    m3d::Scene scene;
    scene.gpuCacheIdentity=0x43726838117ull;
    scene.useLegacyWorldLights=false;
    scene.mat.resize(1);
    scene.mat[0].name="khronos_cube_silver";
    scene.mat[0].metallic=.35f;
    scene.mat[0].roughnessScale=.40f;
    m3d::ColorLevel color; color.size=4;
    color.p.assign(16,0x00b8c3d3u);
    scene.mat[0].colorMip.push_back(std::move(color));
    const int side=std::max(1,int(std::ceil(std::sqrt(float(cubes)))));
    for(int i=0;i<cubes;i++){
        const int ix=i%side,iz=i/side;
        const float x=(float(ix)-.5f*float(side-1))*2.65f;
        const float z=(float(iz)-.5f*float(side-1))*2.65f;
        m3d::Obj cube{};
        cube.kind=m3d::Kind::Box;cube.mat=0;
        cube.mn={x-1.f,-1.f,z-1.f};
        cube.mx={x+1.f,1.f,z+1.f};
        scene.obj.push_back(cube);
    }
    scene.lights.reserve(size_t(lights));
    for(int i=0;i<lights;i++){
        const float a=float(i)*2.f*pi/float(std::max(1,lights));
        m3d::Light l{};
        l.p={4.0f*std::cos(a),1.7f+0.4f*std::sin(3.f*a),4.0f*std::sin(a)};
        l.radius=8.f;
        l.intensity=1.3f;
        l.color={90.f+165.f*std::abs(std::sin(a)),90.f+165.f*std::abs(std::cos(a)),170.f};
        scene.lights.push_back(l);
    }
    return scene;
}
LRESULT CALLBACK wndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_KEYDOWN && w==VK_ESCAPE){PostQuitMessage(0);return 0;}
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,w,l);
}
int argNumber(int argc,char**argv,const char*key,int fallback){
    for(int i=1;i<argc-1;i++)if(std::strcmp(argv[i],key)==0)return std::atoi(argv[i+1]);
    return fallback;
}
std::string argText(int argc,char**argv,const char*key,const char*fallback){
    for(int i=1;i<argc-1;i++)if(std::strcmp(argv[i],key)==0)return argv[i+1];
    return fallback;
}
}
int main(int argc,char** argv){
    const int w=argNumber(argc,argv,"--width",1366);
    const int h=argNumber(argc,argv,"--height",768);
    const int cubes=argNumber(argc,argv,"--cubes",1);
    const int lights=argNumber(argc,argv,"--lights",16);
    const int seconds=argNumber(argc,argv,"--seconds",15);
    const auto motion=argText(argc,argv,"--motion","orbit");
    const bool continuous=argText(argc,argv,"--continuous","0")=="1";
    if(w<64||h<64||w>4096||h>4096||cubes<1||cubes>512||
       lights<1||lights>64||(!continuous&&(seconds<1||seconds>600))||
       (motion!="orbit"&&motion!="fixed"&&motion!="translate"&&motion!="spin")){
        std::cerr<<"Usage: v17_vulkan_khronos_bench --cubes 1..512 --lights 1..64 "
                 <<"--motion orbit|fixed|translate|spin --seconds 1..600 "
                 <<"--continuous (Esc closes) --width 1366 --height 768\n";
        return 2;
    }
    const std::string preflight=argText(argc,argv,"--offscreen-check","");
    if(!preflight.empty()){
        // Offscreen reference uses the same shaders and rasterizer, but does
        // NOT use Win32 WSI. Save an actual 32-bit BMP to check rendering.
        m3d::updateLighting(14.75f);
        const m3d::Scene reference=makeScene(cubes,lights);
        m3d::ShadowMap shadow{};shadow.size=1;shadow.d={1.e9f};
        shadow.span=10.f;shadow.dist=20.f;
        const auto camera=makeCamera(0.f,w,h,motion);
        m3d::Stats stats{};std::wstring error;
        std::vector<uint32_t> pixels;
        if(!m3d::renderVulkanPreview(reference,shadow,camera,w,h,pixels,stats,error)){
            std::wcerr<<L"[WARP Khronos] OFFSCREEN FAILED: "<<error<<L"\n";return 6;
        }
        if(pixels.size()!=size_t(w)*size_t(h)){
            std::cerr<<"[WARP Khronos] OFFSCREEN FAILED incorrect pixel count\n";return 7;
        }
        BITMAPFILEHEADER fh{};fh.bfType=0x4d42;
        fh.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
        fh.bfSize=fh.bfOffBits+DWORD(pixels.size()*sizeof(uint32_t));
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);
        info.biWidth=w;info.biHeight=-h;info.biPlanes=1;
        info.biBitCount=32;info.biCompression=BI_RGB;
        info.biSizeImage=DWORD(pixels.size()*sizeof(uint32_t));
        std::ofstream image(preflight,std::ios::binary|std::ios::trunc);
        if(!image){std::cerr<<"Cannot open offscreen BMP path\n";return 8;}
        image.write(reinterpret_cast<const char*>(&fh),sizeof(fh));
        image.write(reinterpret_cast<const char*>(&info),sizeof(info));
        image.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(info.biSizeImage));
        image.close();
        unsigned brightest=0;size_t nonBlack=0,different=0;
        const uint32_t first=pixels.front()&0x00ffffffu;
        for(const uint32_t px:pixels){
            const uint32_t rgb=px&0x00ffffffu;
            brightest=std::max(brightest,unsigned(std::max({rgb&255u,(rgb>>8)&255u,(rgb>>16)&255u})));
            if(rgb!=first)++different;
            if(rgb>0x000a0a0au)++nonBlack;
        }
        std::cerr<<"[WARP Khronos] OFFSCREEN reference saved: "<<preflight
                 <<" gpu_draw_ms="<<stats.gpuDrawMs
                 <<" gpu_valid="<<stats.gpuTimingValid
                 <<" brightest="<<brightest<<" pixels_different="<<different
                 <<" nonblack_approx="<<nonBlack<<" total_pixels="<<pixels.size()<<"\n";
        // Release DLL-owned Vulkan resources before CRT shutdown.
        std::cerr<<"[WARP Khronos] OFFSCREEN explicit cleanup BEGIN"<<std::endl;
        m3d::releaseVulkanPreviewForCurrentThread();
        std::cerr<<"[WARP Khronos] OFFSCREEN explicit cleanup COMPLETE"<<std::endl;
        if(brightest<15 || different==0){
            std::cerr<<"[WARP Khronos] OFFSCREEN BLACK/FLAT: no valid image; do not trust visual perf\n";
            return 9;
        }
        return 0;
    }
    const bool probe=argText(argc,argv,"--visual-probe","0")=="1";
    if(probe){
        _putenv_s("CSE_WARP_VK_KHRONOS_VISUAL_PROBE","1");
        std::cerr<<"[WARP Khronos] VISUAL PROBE ACTIVE: window MUST be magenta.\n";
    }
    SetProcessDPIAware();
    WNDCLASSW wc{};wc.lpfnWndProc=wndProc;wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=L"CSEWARP_Khronos_Vulkan_Window";
    if(!RegisterClassW(&wc)){std::cerr<<"Win32 RegisterClass failed\n";return 3;}
    const bool borderless=std::getenv("CSE_WARP_VK_BENCH_BORDERLESS")!=nullptr;
    const DWORD style=borderless?WS_POPUP:WS_OVERLAPPEDWINDOW;
    RECT size{0,0,w,h};AdjustWindowRect(&size,style,FALSE);
    const int windowWidth=size.right-size.left,windowHeight=size.bottom-size.top;
    const int posX=borderless?std::max(0,(GetSystemMetrics(SM_CXSCREEN)-windowWidth)/2):CW_USEDEFAULT;
    const int posY=borderless?std::max(0,(GetSystemMetrics(SM_CYSCREEN)-windowHeight)/2):CW_USEDEFAULT;
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"CSE WARP Vulkan direct — cube tournant (Échap ferme)",
        style,posX,posY,windowWidth,windowHeight,
        nullptr,nullptr,wc.hInstance,nullptr);
    if(!hwnd){std::cerr<<"Win32 CreateWindow failed\n";return 3;}
    ShowWindow(hwnd,SW_SHOWNORMAL);UpdateWindow(hwnd);
    m3d::updateLighting(14.75f);
    // Scene-local lights are passed through Scene::lights; do not link to
    // the unexported global pointLights symbol from the core DLL.
    m3d::Scene scene=makeScene(cubes,lights);
    if(motion=="spin")addSpinningCube(scene,0.f); // intentionally rebuild each frame
    m3d::ShadowMap shadow{};
    shadow.size=1;shadow.d={1.0e9f};
    shadow.span=10.f;shadow.dist=20.f;
    std::cerr<<"[WARP Khronos] Real original Vulkan backend. "
             <<"cubes="<<cubes<<" activeLights="<<lights
             <<" motion="<<motion<<" duration="<<seconds<<"s"
             <<" client="<<w<<"x"<<h<<"\n";
    if(motion=="spin")std::cerr<<"[WARP Khronos] spin deliberately updates the tiny cube geometry each frame.\n";
    if(continuous)std::cerr<<"[WARP Khronos] continuous mode: press Escape in the window to close.\n";
    std::wstring error;
    const auto start=Clock::now();
    uint64_t frames=0;
    bool stopped=false;
    while(!stopped){
        MSG msg{};
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){stopped=true;break;}
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(stopped)break;
        const float t=float(std::chrono::duration<double>(Clock::now()-start).count());
        if(!continuous&&t>=float(seconds))break;
        if(motion=="spin"){
            addSpinningCube(scene,t*.65f);
            scene.markGpuGeometryChanged();
        }
        const auto cam=makeCamera(t,w,h,motion);
        m3d::Stats stats{};
        if(!m3d::renderVulkanDirectPresent(scene,shadow,cam,w,h,hwnd,stats,error)){
            std::wcerr<<L"Vulkan direct presentation failed: "<<error<<L"\n";
            std::cerr<<"[WARP Khronos] DIRECT explicit cleanup BEGIN"<<std::endl;
            m3d::releaseVulkanDirectForCurrentThread();
            std::cerr<<"[WARP Khronos] DIRECT explicit cleanup COMPLETE"<<std::endl;
            DestroyWindow(hwnd);
            return 4;
        }
        ++frames;
        if(frames%120==0 && std::getenv("CSE_WARP_VK_VERBOSE_FRAMES")){
            std::cerr<<"[WARP Khronos] frame="<<frames
                     <<" cpu_ms="<<stats.ms
                     <<" gpu_draw_ms="<<stats.gpuDrawMs
                     <<" gpu_transfer_ms="<<stats.gpuTransferMs
                     <<" wait_ms="<<stats.cpuMapWaitMs<<"\n";
        }
    }
    // Destroy VkSurfaceKHR while its Win32 HWND is still valid.
    std::cerr<<"[WARP Khronos] DIRECT explicit cleanup BEGIN"<<std::endl;
    m3d::releaseVulkanDirectForCurrentThread();
    std::cerr<<"[WARP Khronos] DIRECT explicit cleanup COMPLETE"<<std::endl;
    DestroyWindow(hwnd);
    std::cerr<<"[WARP Khronos] completed frames="<<frames
             <<" duration_seconds="<<seconds<<" (CSV in CSE_WARP_VK_KHRONOS_CSV)\n";
    return frames?0:5;
}
// MSVC GUI subsystem entry point: no extra DOS console per benchmark case.
// __argc/__argv are supplied by the MSVC CRT and preserve the existing CLI.
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int){
    return main(__argc,__argv);
}
#endif
