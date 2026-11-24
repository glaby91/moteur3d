#include "m3d_engine.h"
#include "m3d_gi_bake.h"
#include "m3d_lumatrace_bvh4.h"
#include "m3d_directx11_raster_shader.h"
#include "m3d_directx11_sky_shader.h"
#include "m3d_directx11_composite_shader.h"
#include "m3d_directx11_mesh.h"
#include "../../../../v17/weather/clouds.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "m3d_lumatrace_gpu_d3d11.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <cmath>
#include <limits>

using Microsoft::WRL::ComPtr;

namespace m3d {
namespace {
using d3d11::GpuVertex;
using d3d11::TransparentTriangle;
using d3d11::MeshBuilder;
using d3d11::DrawChunk;
using d3d11::makeFoliageTemplate;
using d3d11::uvScaleForMaterial;
using d3d11::visibleChunk;
// Single atomic configuration word: host and GUI may update quality while
// the dedicated GPU worker is rendering. Each frame gets one full snapshot.
// Bits: 0..7 MSAA, 8..15 anisotropy, 16..31 LOD quality percent,
// 32 culling, 33 LOD.
constexpr uint64_t defaultQualityBits=4ull|(16ull<<8)|(100ull<<16)|(1ull<<32)|(1ull<<33);
std::atomic<uint64_t> gpuQualityBits{defaultQualityBits};
// -1 = legacy environment-driven benchmark; GUI sets 0 at idle, 1 only on motion.
// A single atomic snapshot per submitted frame avoids UI/worker environment races.
std::atomic<int> peripheralOverride{-1};
// Opt-in: keep all reference benchmarks and pre-existing renders pixel-exact.
std::atomic<bool> hybridLighting{false};
// Separate, independently reversible display-referred highlight compression.
std::atomic<bool> filmicTonemap{false};
// Corrected linear-energy material lighting (core OFF for parity, GUI ON).
std::atomic<bool> colorEnergyBalance{false};
std::atomic<float> colorExposure{1.f};
std::atomic<float> cloudTimeScale{1.f};
std::atomic<bool> rasterCloudsEnabled{true};
// The GUI sets this to scene.json.m3dgi. Updated only on user action or
// scene loading; render worker snapshots it when creating its probe texture.
std::mutex giPathMutex;
std::filesystem::path giFilePath;
std::atomic<uint64_t> giFileRevision{1};
std::atomic<bool> giTableLoaded{false};
struct HourlyIrradiance { V sky,bounce,sun; };
// Bake 25 half-hour samples once. This is a temporal irradiance LUT, not
// a substitute for fully ray-traced light transport. Spatial influence comes
// from the separate scene-dependent ray-sampled volume below.
HourlyIrradiance irradianceAt(float hour){
    static const auto samples=[](){
        std::array<HourlyIrradiance,25> lut{};
        for(int i=0;i<25;++i){
            const float h=7.f+float(i)*.5f;
            const float day=std::max(0.f,std::sin(PI*clampf((h-6.f)/13.f,0.f,1.f)));
            const float dusk=clampf(1.f-std::abs(h-18.3f)/2.3f,0.f,1.f);
            const float dawn=clampf(1.f-std::abs(h-7.1f)/1.8f,0.f,1.f);
            const float warm=std::max(dusk,dawn*.72f);
            lut[i].sky={.08f+day*(.27f+.05f*warm),
                        .10f+day*(.31f-.02f*warm),
                        .15f+day*(.37f-.16f*warm)};
            lut[i].bounce={.13f+day*(.10f+.22f*warm),
                           .10f+day*(.10f+.08f*warm),
                           .08f+day*(.11f-.05f*warm)};
            lut[i].sun={1.f,1.f-.14f*warm,1.f-.32f*warm};
        }
        return lut;
    }();
    const float u=clampf((hour-7.f)*2.f,0.f,24.f);
    const size_t i=size_t(u);
    const size_t j=std::min<size_t>(i+1,24);
    const float a=u-float(i);
    return {samples[i].sky*(1.f-a)+samples[j].sky*a,
            samples[i].bounce*(1.f-a)+samples[j].bounce*a,
            samples[i].sun*(1.f-a)+samples[j].sun*a};
}

uint64_t packQuality(const RasterQualitySettings& q){
    return uint64_t(q.msaaSamples)|(uint64_t(q.anisotropy)<<8)
        |(uint64_t(q.lodQualityPercent)<<16)
        |(uint64_t(q.frustumCulling?1u:0u)<<32)
        |(uint64_t(q.dynamicLod?1u:0u)<<33);
}
RasterQualitySettings unpackQuality(uint64_t bits){
    RasterQualitySettings q;
    q.msaaSamples=uint32_t(bits&255u);
    q.anisotropy=uint32_t((bits>>8)&255u);
    q.lodQualityPercent=uint32_t((bits>>16)&65535u);
    q.frustumCulling=((bits>>32)&1u)!=0;
    q.dynamicLod=((bits>>33)&1u)!=0;
    return q;
}
struct GpuVertexLuma {
    float p[3];
    uint32_t normalPacked;
    float uv[2];
    uint32_t material,primitiveId;
};
static_assert(sizeof(GpuVertexLuma)==32,"LumaTrace packed vertex layout");
uint32_t packLumaNormal(const float n[3]){
    const auto pack=[](float v)->uint32_t{
        const uint32_t q=uint32_t(std::lround((std::clamp(v,-1.f,1.f)*0.5f+0.5f)*1023.f));
        return q&0x3ffu;
    };
    return pack(n[0])|(pack(n[1])<<10)|(pack(n[2])<<20);
}
// Box geometry is immutable and shared. Per-object bounds/materials occupy
// 32 bytes instead of duplicating 36 vertex records (~1.3 kB) per box.
struct GpuBoxInstance {
    float lo[3], uvScale;
    float hi[3]; uint32_t material;
    float size[3]; uint32_t uvMode;
};
static_assert(sizeof(GpuBoxInstance)==48,"D3D11 per-instance layout must be 48 bytes");
// Ellipsoids use a single dynamic instance buffer per frame. Three immutable
// unit-sphere meshes provide GPU LOD without regenerating scene geometry.
struct GpuEllipseInstance {
    float center[3];uint32_t material;
    float radii[3];float padding;
};
static_assert(sizeof(GpuEllipseInstance)==32,"D3D11 ellipsoid instance size");
struct FrameConstants {
    float eye[3], tanHalf;
    float forward[3], aspect;
    float right[3], nearPlane;
    float up[3], farPlane;
    float light[3], ambient;
    float sky[3]; uint32_t materialCount;
    float shadowBase[3], shadowSpan;
    float shadowRight[3], shadowDist;
    float shadowUp[3], shadowSize;
    float materialGlow[256][4];
    float materialSpecular[256][4];
    float materialOpacity[256][4];
    float materialDiffuse[256][4];
    float materialEmission[256][4];
    float materialTextureDetail[256][4];
    float lightPositionRadius[256][4];
    float lightColorIntensity[256][4];
    uint32_t lightCount; float padding[3];
    float indirectSky[4];
    float indirectBounce[4];
    float indirectSun[4];
    float probeOrigin[4];
    float probeInvSpan[4];
    float hybridControls[4];
    float colorControls[4];
    float peripheralControls[4];
};

// Independent sky constants avoid disturbing the existing PBR/shadow shader layout.
struct CloudConstants {
    float forward[3],tanHalf;
    float right[3],aspect;
    float up[3],pad0;
    float sun[3],day;
    float zenith[3],opacity;
    float horizon[3],coverage;
    float wind[4];
};



// World-locked, hemispherical cirrus. The procedural field is evaluated only
// on uncovered background pixels; its cloud mask is never applied over objects.



class DirectX11Preview {
    using Clock=std::chrono::steady_clock;
    struct ReadbackSlot {
        ComPtr<ID3D11Texture2D> staging;
        ComPtr<ID3D11Query> disjoint,start,afterDraw,afterCopy;
        bool queryReady=false;
        int width=0,height=0;
        Clock::time_point cpuStarted{},copyQueued{};
        Stats submitted{};
    };
public:
    bool render(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,
                std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error) {
        // The exported offline benchmark and PNG path are unchanged.
        releasePresenter();
        return enqueue(scene,shadows,cam,w,h,1,error)&&receive(pixels,stats,error);
    }
    bool renderLumaTrace(const Scene& scene,const LumaTraceBvh4& bvh,
                const ShadowMap& shadows,const Camera& cam,int w,int h,
                std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
        releasePresenter();
        return enqueue(scene,shadows,cam,w,h,1,error,nullptr,nullptr,&bvh)&&
               receive(pixels,stats,error);
    }
    bool present(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,
                 HWND hwnd,Stats& stats,std::wstring& error,
                 const LumaTraceBvh4* lumaBvh=nullptr) {
        if(!hwnd || !IsWindow(hwnd)){error