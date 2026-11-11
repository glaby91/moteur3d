#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "src/m3d_physics.h"
namespace m3d {
constexpr float PI=3.14159265358979323846f, INF=std::numeric_limits<float>::infinity();
struct V { float x=0,y=0,z=0; V operator+(V b)const{return{x+b.x,y+b.y,z+b.z};} V operator-(V b)const{return{x-b.x,y-b.y,z-b.z};} V operator*(float s)const{return{x*s,y*s,z*s};} V operator/(float s)const{return{x/s,y/s,z/s};} };
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;} inline V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};} inline float length(V a){return std::sqrt(dot(a,a));} inline V norm(V a){float l=length(a);return l>1e-20f?a/l:V{0,1,0};} inline float clampf(float x,float a,float b){return std::max(a,std::min(b,x));}
enum class Kind:uint8_t{Triangle=0,Box=1,RoundBox=2,Capsule=3,Cone=4,Ellipse=5};
struct Obj { Kind kind{}; uint8_t mat=0; V mn,mx,a,b,c,n0,n1,n2,uv0,uv1,uv2,center,core; float r=0,r1=0,k=0,len=0,len2=0; };
// Optional immutable source-box metadata for GPU raster instancing.
// The actual triangles remain in Scene::obj for CPU/Compute/BVH equivalence.
struct SourceBox {
    uint32_t firstTriangle=0, triangleCount=0;
    V mn{},mx{};
    uint32_t material=0;
};
// V16 foliage canopy source metadata: CPU/Compute triangles remain untouched,
// while the rasterizer can select a shared mesh at a distance-dependent LOD.
struct SourceFoliage {
    uint32_t firstTriangle=0,triangleCount=0;
    V center{},radii{};
    uint32_t material=0;
};
struct Node { V mn,mx; int32_t left=-1,right=-1,start=0,count=0; bool simdTrianglePair=false; };
struct Level { uint32_t size=0; std::vector<uint16_t> p; };
// Version-5 albedo mips keep full 8-bit RGB channels (0x00RRGGBB).
struct ColorLevel { uint32_t size=0; std::vector<uint32_t> p; };
// Both optical controls are percentages; transmission is thin, straight-through glass.
struct Ray { V o,d; };
struct Hit { float t=INF; V p,n; int mat=-1,id=-1; };
struct Camera { V eye,f,r,u; float tanHalf=0,aspect=1,pixelAngle=0,focusDistance=12,apertureRadius=0; };
struct RendererCapabilities { bool dxrHardware=false; bool dxrBackend=false; };
struct Stats { uint64_t rays=0,tests=0,aabb=0,nodes=0,hits=0,sky=0,texels=0,depthReads=0,shadowRays=0; uint64_t simdRayPackets=0,simdTrianglePackets=0,simdTextureSamples=0,simdLightBatches=0; double ms=0,postMs=0,shadowMs=0;
    // DX11-only hardware counters; CPU/Compute paths leave these at zero.
    uint64_t gpuDrawCalls=0,gpuBoxInstances=0,gpuEllipseInstances=0,gpuFoliageInstances=0;
    // Hardware D3D11 timestamp results, independent of the CPU-side Map wait.
    // Valid only if GPU query timestamps are supported and not disjoint.
    double gpuDrawMs=0,gpuTransferMs=0,gpuTotalMs=0;
    double cpuSubmitMs=0,cpuMapWaitMs=0,cpuPixelConvertMs=0;
    uint32_t readbackBufferCount=1;
    bool gpuTimingValid=false;
    unsigned threads=1; bool avx2=false,gpu=false; };
struct Light { V p; float radius,intensity; V color; };
struct Material { std::string name; uint8_t reflectivityPercent=0,transmissionPercent=0; float refractiveIndex=1.5f; float metallic=0.f, roughnessScale=1.f; std::vector<Level> mip,normalMip,roughnessMip; std::vector<ColorLevel> colorMip; };
struct Scene { bool underwaterEnvironment=false; bool finiteGround=false; bool useLegacyWorldLights=true; std::vector<Obj> obj; std::vector<SourceBox> sourceBoxes; std::vector<SourceFoliage> sourceFoliage; std::vector<Node> node; std::vector<uint32_t> order; std::vector<Material> mat; std::vector<Light> lights; int32_t root=-1;
 // V16 builder assigns a unique identity after scene construction. Tracked scenes
 // are immutable except for explicit editor mutations, which MUST call
 // markGpuContentChanged(). Legacy/untracked scenes (identity == 0) retain
 // the full byte-for-byte scene hash as a safe fallback.
 uint64_t gpuCacheIdentity=0, gpuCacheRevision=0;
 void markGpuContentChanged(){++gpuCacheRevision;}
};
struct ShadowMap { int size=1024; float span=30,dist=42; V base={0,1.5f,0},right,up;std::vector<float>d,min,max; };
extern V sun,skyZenith,skyHorizon,sunTint;
extern float daylight,lampFactor,solarHour;
extern std::vector<Light> pointLights;
void updateLighting(float hour);
bool loadScene(const std::filesystem::path& path,Scene& s,std::wstring& error);
inline bool loadScene(const wchar_t* path,Scene& s,std::wstring& error){return loadScene(std::filesystem::path(path),s,error);}
void closest(const Scene& s,const Ray& r,Hit& best,Stats* st,bool ground=true,int ignoreObject=-1,bool ignoreTransparent=false);
int groundMaterial(const Scene& s,V p);
ShadowMap buildShadow(const Scene& s,Stats& st);
V shade(const Scene& s,const ShadowMap& sm,const Camera& cam,const Ray& r,const Hit& h,Stats& st,bool fog,bool reflections=true);
V traceColor(const Scene& s,const ShadowMap& sm,const Camera& cam,const Ray& r,Stats& st,bool fog);
Camera camera(float angleDeg,float elevationDeg,float distance,int w,int h,V look=V{0,1,-8});
Stats renderHybrid(const Scene& s,const ShadowMap& sm,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,unsigned threads,bool fog,const std::atomic<uint64_t>& generation,uint64_t token);
Stats render(const Scene& s,const ShadowMap& sm,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,unsigned threads,bool fog,const std::atomic<uint64_t>& generation,uint64_t token,unsigned samplesPerPixel=1,bool depthOfField=false,bool useAvx2=false,bool offlinePathTrace=false,unsigned maxBounces=3);
V traceOfflineColor(const Scene& s,const ShadowMap& sm,const Camera& cam,const Ray& ray,Stats& st,bool fog,uint64_t seed,unsigned maxBounces);
/* Accumulate distinct, deterministic Monte-Carlo samples as linear HDR RGB.
 * Each pass updates accum and returns an sRGB/tonemapped preview of all samples
 * [0, firstSample+batchSamples). The same image must result whether rendered in
 * one batch or in several progressive passes. Cancellation is checked per tile.
 * Caller must reset accum on changes of scene, camera or render settings. */
Stats renderGiAccumulatePass(const Scene& scene,const ShadowMap& shadow,
    const Camera& cam,int w,int h,std::vector<V>& accum,
    std::vector<uint32_t>& preview,uint32_t firstSample,uint32_t batchSamples,
    unsigned threads,const std::atomic<uint64_t>& generation,uint64_t token,
    unsigned maxBounces);

RendererCapabilities detectRendererCapabilities() noexcept;
bool renderCompute11Preview(const Scene& s,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error);
bool renderDirectX11Preview(const Scene& s,const ShadowMap& sm,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error);
// Renderer quality is a shared public API: embedded host software and the
// standalone Qt interface modify exactly the same settings. Each DX11 frame
// takes one atomic snapshot, preventing partial UI/worker configuration.
// Hardware availability may reduce actual MSAA samples (4 -> 2 -> 1).
enum class RasterQualityProfile:uint8_t{Performance=0,Balanced=1,High=2,Ultra=3};
struct RasterQualitySettings {
    uint32_t msaaSamples=4;          // 1, 2, 4, 8 (auto fallback if unsupported)
    uint32_t anisotropy=16;          // 1, 2, 4, 8, 16
    uint32_t lodQualityPercent=100;  // 50 .. 200; higher retains details farther
    bool frustumCulling=true;        // never changes pixel fidelity
    bool dynamicLod=true;            // Ultra disables reduction of distant assets
};
RasterQualitySettings rasterQualityProfile(RasterQualityProfile preset);
RasterQualitySettings rasterQualitySettings();
// Experimental geometry-aware probe illumination. Default OFF for legacy pixel parity.
// GUI may opt in independently; the renderer snapshots this at frame boundaries.
bool rasterHybridLightingEnabled();
void setRasterHybridLightingEnabled(bool enabled);
// Filmic shoulder control is independent of probe GI, and defaults OFF.
bool rasterFilmicTonemapEnabled();
void setRasterFilmicTonemapEnabled(bool enabled);
// Selects scene-specific offline GI cache, reloaded on every explicit set.
void setRasterGiCachePath(const std::filesystem::path& path);
bool rasterGiCacheLoaded();
bool setRasterQualitySettings(const RasterQualitySettings& settings,std::wstring& error);

// Experimental pipelined D3D11 readback. Enqueue issues an ordered frame render
// and CopyResource without waiting for CPU pixels; Receive returns the oldest
// submitted frame, preserving exact frame order. Call both on the same thread.
// Depth 1, 2 or 3, selected when queue is empty. Normal renderer stays
// synchronous and is unaffected unless this API is explicitly used.
bool enqueueDirectX11Preview(const Scene& s,const ShadowMap& sm,const Camera& cam,
    int w,int h,unsigned depth,std::wstring& error);
bool receiveDirectX11Preview(std::vector<uint32_t>& pixels,Stats& stats,
    std::wstring& error);
std::vector<uint32_t> upscaleBilinear(const std::vector<uint32_t>& src,int sw,int sh,int dw,int dh,unsigned threads);
std::vector<uint32_t> downsample2x(const std::vector<uint32_t>& src,int sw,int sh,unsigned threads);
bool avx2FmaAvailable() noexcept;
void primaryRays8Avx2(const Camera&,int,int,int,int,float,float,Ray*) noexcept;
bool intersectTriangles8Avx2(const Scene&,const Ray&,const uint32_t*,float,float*) noexcept;

V accumulatePointLightsAvx2(const Light*,size_t,V,V,float) noexcept;
V sampleRgb565Avx2(uint16_t,uint16_t,uint16_t,uint16_t,float,float,float,float) noexcept;
std::vector<uint32_t> antialiasEdges4(const std::vector<uint32_t>& src,int w,int h,unsigned threads,bool useAvx2=false);
std::vector<uint32_t> antialiasEdges4Avx2(const std::vector<uint32_t>& src,int w,int h,unsigned threads);
std::vector<uint32_t> bloomGlow(const std::vector<uint32_t>& src,int w,int h,unsigned threads,float strength=0.22f);
std::vector<uint32_t> denoiseBilateral(const std::vector<uint32_t>& src,int w,int h,unsigned threads);
}
