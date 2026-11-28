#include "m3d_engine.h"
#include "m3d_static_geometry_prepare.h"
#include "m3d_gi_bake.h"
#include "m3d_lumatrace_bvh4.h"
#include "m3d_directx11_raster_shader.h"
#include "m3d_directx11_sky_shader.h"
#include "m3d_directx11_composite_shader.h"
#include "m3d_directx11_mesh.h"
#include "m3d_dx11_r001.h"
#include "m3d_dx11_emission.h"
#include "m3d_zmaterial.h"
#include "m3d_zreflect_runtime.h"
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
#include "m3d_escargot_trace_d3d11.h"
#include "m3d_escargot_raster_d3d11.h"
#include "m3d_escargot_projected_d3d11.h"
#include "m3d_escargot_pack_d3d11.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <deque>
#include <cmath>
#include <limits>
#include "m3d_dx11_atmosphere.h"

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
struct EscargotInput {const std::vector<float>* color;const std::vector<float>* meta;int w,h;const std::vector<float>* packet=nullptr;bool resident=false;const EscargotPacketBuild* build=nullptr;EscargotBuildTiming* buildTiming=nullptr;};
thread_local const EscargotInput* escargotInput=nullptr;
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
    uint32_t material,primitiveId,receiverId;
};
static_assert(sizeof(GpuVertexLuma)==36,"LumaTrace packed vertex layout");
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
struct ZReflectTarget {
    ComPtr<ID3D11Texture2D> tex,depth;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11ShaderResourceView> srv;
    int w=0,h=0;
};
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
    float reflectionControls[4];
    uint32_t adaptiveZero[512];
    float zClip[4]; // inward plane, zero outside secondary passes
    float zPlaneIds[4]; // slot IDs + 1, count, secondary flag
    float zEye[2][4],zForward[2][4],zRight[2][4],zUp[2][4];
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



// Explicit benchmark-only synchronous GPU timestamps; production submission
// remains asynchronous. Completion event is independent of timestamp support.
class EscargotFrameProfile {
    using Clock=std::chrono::steady_clock;
    ID3D11DeviceContext* c_=nullptr;
    ComPtr<ID3D11Query> disjoint_,event_;
    std::array<ComPtr<ID3D11Query>,5> ticks_;
    bool timed_=false,open_=false;
public:
    EscargotFrameProfile(ID3D11Device* d,ID3D11DeviceContext* c,bool enabled){
        if(!enabled)return;c_=c;D3D11_QUERY_DESC q{};q.Query=D3D11_QUERY_EVENT;
        if(FAILED(d->CreateQuery(&q,event_.GetAddressOf())))return;
        q.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;
        timed_=SUCCEEDED(d->CreateQuery(&q,disjoint_.GetAddressOf()));
        q.Query=D3D11_QUERY_TIMESTAMP;
        for(auto& t:ticks_)if(timed_&&FAILED(d->CreateQuery(&q,t.GetAddressOf())))timed_=false;
    }
    void start(){if(timed_){c_->Begin(disjoint_.Get());open_=true;mark(0);}}
    void mark(size_t n){if(open_)c_->End(ticks_[n].Get());}
    void finish(EscargotProfile& p){
        if(!event_)return;
        if(open_){mark(4);c_->End(disjoint_.Get());open_=false;}
        c_->End(event_.Get());c_->Flush();const auto start=Clock::now();
        BOOL ready=FALSE;HRESULT hr=S_FALSE;
        while((hr=c_->GetData(event_.Get(),&ready,sizeof(ready),D3D11_ASYNC_GETDATA_DONOTFLUSH))==S_FALSE){
            if(Clock::now()-start>std::chrono::seconds(30))break;Sleep(1);
        }
        p.waitMs=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
        p.completed=hr==S_OK&&ready;
        if(!p.completed||!timed_)return;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};UINT64 t[5]{};
        if(c_->GetData(disjoint_.Get(),&d,sizeof(d),0)!=S_OK||d.Disjoint||!d.Frequency)return;
        for(size_t i=0;i<5;++i)if(c_->GetData(ticks_[i].Get(),&t[i],sizeof(t[i]),0)!=S_OK||(i&&t[i]<t[i-1]))return;
        double k=1000./double(d.Frequency);
        p.computeGpuMs=(t[1]-t[0])*k;p.opticalGpuMs=(t[2]-t[1])*k;p.mainGpuMs=(t[3]-t[2])*k;p.copyGpuMs=(t[4]-t[3])*k;p.gpuTotalMs=(t[4]-t[0])*k;p.gpuValid=true;
    }
    ~EscargotFrameProfile(){if(open_)c_->End(disjoint_.Get());}
};
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
    // Native presentation timing is asynchronous; never stall or flush for queries.
    struct ReflectionTimer {
        ComPtr<ID3D11Query> disjoint,begin,end;
        bool pending=false;
        uint64_t epoch=0;
    };
    std::array<ReflectionTimer,8> reflectionTimers_{};
    uint64_t reflectionTimingEpoch_=0;
    ReflectionTimer* acquireReflectionTimer(double& total,uint32_t& samples){
        ReflectionTimer* available=nullptr;
        for(auto& q:reflectionTimers_){
            if(q.pending){
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d{};UINT64 a=0,b=0;
                const auto hr=context_->GetData(q.disjoint.Get(),&d,sizeof(d),D3D11_ASYNC_GETDATA_DONOTFLUSH);
                if(hr==S_FALSE)continue;
                if(hr==S_OK){
                    const auto ha=context_->GetData(q.begin.Get(),&a,sizeof(a),D3D11_ASYNC_GETDATA_DONOTFLUSH);
                    const auto hb=context_->GetData(q.end.Get(),&b,sizeof(b),D3D11_ASYNC_GETDATA_DONOTFLUSH);
                    if(ha==S_FALSE||hb==S_FALSE)continue;
                    if(ha==S_OK&&hb==S_OK&&q.epoch==reflectionTimingEpoch_&&!d.Disjoint&&d.Frequency&&b>=a){
                        total+=double(b-a)*1000./double(d.Frequency);++samples;
                    }
                }
                q.pending=false;
            }
            if(!available)available=&q;
        }
        if(!available)return nullptr;
        if(!available->disjoint||!available->begin||!available->end){
            D3D11_QUERY_DESC desc{};desc.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;
            if(FAILED(device_->CreateQuery(&desc,available->disjoint.ReleaseAndGetAddressOf())))return nullptr;
            desc.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(device_->CreateQuery(&desc,available->begin.ReleaseAndGetAddressOf()))||
               FAILED(device_->CreateQuery(&desc,available->end.ReleaseAndGetAddressOf())))return nullptr;
        }
        available->epoch=reflectionTimingEpoch_;
        return available;
    }
public:
    bool renderBuiltEscargot(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,EscargotBuildTiming& timing,std::wstring& error){
        if(!renderLumaTrace(scene,bvh,shadows,cam,w,h,pixels,stats,error))return false;
        return (escargotInput&&escargotInput->build&&(escargotInput->build->rasterDirect||escargotInput->build->rasterProjected))||escBuild_.finish(context_.Get(),timing,error);
    }
    bool traceEscargotPaths(const Scene& scene,const LumaTraceBvh4& bvh,const std::vector<EscargotPathQuery>& q,std::vector<EscargotPathEvent>& events,EscargotPathTiming& timing,std::wstring& error){
        if(!ensureDevice(error)||!ensureScene(scene,error,&bvh))return false;
        if(!lumaTraceCompute_.prepare(device_.Get(),bvh,lumaCpuScene_,scene,false,error))return false;
        return escTrace_.runPaths(device_.Get(),context_.Get(),lumaTraceCompute_.bvhSrv(),scene,bvh,q,events,timing,error);
    }
    bool traceEscargot(const Scene& scene,const LumaTraceBvh4& bvh,const std::vector<EscargotRayQuery>& q,std::vector<EscargotRayHit>& hits,double& preparation,double& execution,std::wstring& error){
        if(!ensureDevice(error)||!ensureScene(scene,error,&bvh))return false;
        if(!lumaTraceCompute_.prepare(device_.Get(),bvh,lumaCpuScene_,scene,false,error))return false;
        return escTrace_.run(device_.Get(),context_.Get(),lumaTraceCompute_.bvhSrv(),scene,bvh,q,hits,preparation,execution,error);
    }
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
        if(!hwnd || !IsWindow(hwnd)){error=L"Fenêtre native DirectX 11 indisponible.";return false;}
        return enqueue(scene,shadows,cam,w,h,1,error,hwnd,&stats,lumaBvh);
    }
    void releasePresenter() {
        ++reflectionTimingEpoch_;
        if(context_ && swapchain_)context_->Flush();
        swapchain_.Reset();
        nativeHwnd_=nullptr;
        nativeW_=nativeH_=0;
    }
    bool enqueue(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,
                 unsigned depth,std::wstring& error,
                 HWND nativeHwnd=nullptr,Stats* nativeStats=nullptr,
                 const LumaTraceBvh4* lumaBvh=nullptr) {
        const auto started=Clock::now();
        if(w<=0||h<=0){error=L"Résolution GPU invalide.";return false;}
        if(depth<1||depth>3){error=L"Profondeur des tampons GPU attendue : 1 à 3.";return false;}
        if(pending_.size()>=depth || (!pending_.empty()&&depth!=readbackDepth_)){
            error=L"Lecture GPU en attente : vider la file avant de changer la profondeur.";return false;
        }
        if(!pending_.empty() && (w!=width_||h!=height_)){
            error=L"Changement de résolution avec des images GPU en attente.";return false;
        }
        const uint64_t qualityBits=gpuQualityBits.load(std::memory_order_acquire);
        const bool hybrid=hybridLighting.load(std::memory_order_acquire);
        const bool tone=filmicTonemap.load(std::memory_order_acquire);
        const bool balanced=colorEnergyBalance.load(std::memory_order_acquire);
        const float exposure=clampf(colorExposure.load(std::memory_order_acquire),.65f,1.45f);
        const RasterQualitySettings quality=unpackQuality(qualityBits);
        const char* dxR001Env=std::getenv("M3D_V17_DX11_R001");
        const bool dxR001=!lumaBvh&&!(dxR001Env&&std::strcmp(dxR001Env,"0")==0);
        const dx11::r001::Frustum dxFrustum(cam);
        const int peripheralSetting=peripheralOverride.load(std::memory_order_acquire);
        const char* multiresEnv=peripheralSetting<0
            ?std::getenv("M3D_V17_PERIPHERAL_MULTIREZ"):nullptr;
        const float atmosphereDensity=scene.underwaterEnvironment?0.f:atmosphere::density();
        const bool escEnabled=lumaBvh&&escargotInput!=nullptr;
        // F16's stencil is defined in full-resolution pixel coordinates.
        const bool multires=!escEnabled&&atmosphereDensity<=0.f&&(peripheralSetting==1 ||
            (peripheralSetting<0&&multiresEnv&&std::strcmp(multiresEnv,"1")==0))
            &&w>=320&&h>=240;
        if(!pending_.empty() &&
           (sceneKey_!=scene.obj.data() || objectCount_!=scene.obj.size() ||
            sceneIdentity_!=scene.gpuCacheIdentity ||
            sceneRevision_!=scene.gpuCacheRevision ||
            lastSubmissionQualityBits_!=qualityBits ||
            lastSubmissionHybrid_!=hybrid ||
            lastSubmissionTone_!=tone ||
            lastSubmissionBalanced_!=balanced ||
            lastSubmissionExposure_!=exposure || lastSubmissionMultires_!=multires)){
            error=L"Vider les lectures différées avant de changer de scène ou de qualité.";
            return false;
        }
        if(lumaBvh&&(lumaBvh->nodes.empty()||lumaBvh->primitiveCount==0)){
            error=L"Le BVH4 LumaTrace n’a pas été préparé pour cette scène.";return false;
        }
        if(!ensureDevice(error)||!ensureScene(scene,error,lumaBvh))return false;
        char zValue[16]{};const DWORD zLength=GetEnvironmentVariableA("M3D_V17_ZREFLECT",zValue,DWORD(sizeof(zValue)));
        const char* zOpt=(zLength>0&&zLength<sizeof(zValue))?zValue:nullptr;
        // V17.024: production LumaTrace enables Z Reflect unless explicitly disabled.
        if(escEnabled&&(escargotInput->w!=w||escargotInput->h!=h||multires)){error=L"Escargot incompatible dimensions/multires";return false;}
        const bool zEnabled=lumaBvh&&!escEnabled&&!(zOpt&&std::strcmp(zOpt,"0")==0);
        zActive_.clear();zreflect::QueryStats zStats{};
        const auto zStart=Clock::now();
        // Production "Z Reflect off" intentionally retains water. The profiler
        // needs a distinct, explicit all-optics-off control (including water).
        char profileBaseOff[4]{};
        const DWORD profileBaseLength=GetEnvironmentVariableA("CSE_WARP_ESCARGOT_PROFILE_BASE_OFF",profileBaseOff,DWORD(sizeof(profileBaseOff)));
        const bool allOpticsOff=profileBaseLength==1&&profileBaseOff[0]=='1';
        if(lumaBvh&&!escEnabled&&!allOpticsOff)zStats=zRuntime_.select(scene,cam,zActive_,zEnabled);
        const bool escDirect=escEnabled&&escargotInput->build&&escargotInput->build->rasterDirect;
        const bool escProjected=escEnabled&&escargotInput->build&&escargotInput->build->rasterProjected;
        const bool escBuilt=escEnabled&&escargotInput->build&&!escDirect&&!escProjected;
        const bool escGpu=escEnabled&&(escargotInput->packet||escBuilt);
        EscargotProfile escProfile{};const char* ep=std::getenv("CSE_WARP_ESCARGOT_PROFILE");
        const bool profileEnabled=ep&&std::strcmp(ep,"1")==0;
        if(escProjected){
            const auto mapStart=Clock::now();
            if(!uploadEscargotPacket(escargotInput->build->data,error,true))return false;
            escProfile.mapMs=std::chrono::duration<double,std::milli>(Clock::now()-mapStart).count();
        }else if(escDirect){
            if(!objectPs_){error=objectShaderError_.empty()?L"Object GPU shader not enabled at device startup":objectShaderError_;return false;}
        }else if(escBuilt){
            if(!escargotInput->buildTiming||!escBuild_.build(device_.Get(),context_.Get(),*escargotInput->build,*escargotInput->buildTiming,error))return false;
        }else if(escEnabled&&!(escGpu?uploadEscargotPacket(*escargotInput->packet,error,escargotInput->resident):uploadEscargot(w,h,*escargotInput,error)))return false;
        const double zSelectUs=lumaBvh?std::chrono::duration<double,std::micro>(Clock::now()-zStart).count():0.;
        const uint32_t zTotal=lumaBvh?uint32_t(scene.opticalPlanes.size()):0u;
        // Two shared slots for all optical surfaces, including water.
        // Oversubscription is visible in telemetry; no global descriptor limit.
        const uint32_t zDeferred=uint32_t(zActive_.size()>2?zActive_.size()-2:0);
        if(zActive_.size()>2)zActive_.resize(2);
        for(size_t i=0;i<zActive_.size();++i)
            if(!ensureZTarget(zTargets_[i],std::max(1,w/2),std::max(1,h/2),error))return false;
        const bool planarReflection=std::any_of(zActive_.begin(),zActive_.end(),[&](const auto& a){return zRuntime_.water(a.id);});
        const bool anyReflection=!zActive_.empty();
        if(!ensureShadow(shadows,error)||
           (hybrid&&!lumaBvh&&!ensureProbeVolume(scene,error))||
           (hybrid&&!lumaBvh&&!ensureGiHours(error))||
           !ensureTargets(w,h,quality.msaaSamples,error)||
           (multires&&!ensureMultires(w,h,error))||
           !ensureAnisotropy(quality.anisotropy,error)||
           (nativeHwnd?!ensureSwapchain(nativeHwnd,w,h,error):
                        !ensureReadback(depth,w,h,error)))return false;
        const char* adaptiveEnv=std::getenv("M3D_V17_LUMA_ADAPTIVE");
        const bool adaptiveRequested=lumaBvh&&adaptiveEnv&&std::strcmp(adaptiveEnv,"1")==0;
        if(lumaBvh&&!lumaTraceCompute_.prepare(device_.Get(),*lumaBvh,lumaCpuScene_,scene,adaptiveRequested,error))return false;
        if(escDirect&&!objectOptics_.prepare(device_.Get(),context_.Get(),scene,*lumaBvh,escargotInput->build->rasterReceivers,error))return false;
        const unsigned slotIndex=nextReadbackSlot_;
        ReadbackSlot& slot=readbackSlots_[slotIndex];
        slot.cpuStarted=started;
        FrameConstants cb{};
        cb.eye[0]=cam.eye.x;cb.eye[1]=cam.eye.y;cb.eye[2]=cam.eye.z;cb.tanHalf=cam.tanHalf;
        cb.forward[0]=cam.f.x;cb.forward[1]=cam.f.y;cb.forward[2]=cam.f.z;cb.aspect=cam.aspect;
        cb.right[0]=cam.r.x;cb.right[1]=cam.r.y;cb.right[2]=cam.r.z;cb.nearPlane=.08f;
        cb.up[0]=cam.u.x;cb.up[1]=cam.u.y;cb.up[2]=cam.u.z;cb.farPlane=10000.f;
        cb.light[0]=sun.x;cb.light[1]=sun.y;cb.light[2]=sun.z;cb.ambient=.66f;
        V bg=scene.underwaterEnvironment?V{7,27,39}:skyHorizon*.65f+skyZenith*.35f;
        // Sky authoring is in sRGB, while the target expects linear light.
        auto decodeSrgb=[](float v)->float{
            v=clampf(v,0.f,1.f);
            return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);
        };
        cb.sky[0]=balanced?decodeSrgb(bg.x/255.f):bg.x/255.f;
        cb.sky[1]=balanced?decodeSrgb(bg.y/255.f):bg.y/255.f;
        cb.sky[2]=balanced?decodeSrgb(bg.z/255.f):bg.z/255.f;
        cb.materialCount=uint32_t(std::max<size_t>(1,scene.mat.size()));
        for(size_t m=0;m<256;m++)cb.materialTextureDetail[m][0]=-1.f;
        cb.shadowBase[0]=shadows.base.x;cb.shadowBase[1]=shadows.base.y;cb.shadowBase[2]=shadows.base.z;cb.shadowSpan=shadows.span;
        cb.shadowRight[0]=shadows.right.x;cb.shadowRight[1]=shadows.right.y;cb.shadowRight[2]=shadows.right.z;cb.shadowDist=shadows.dist;
        cb.shadowUp[0]=shadows.up.x;cb.shadowUp[1]=shadows.up.y;cb.shadowUp[2]=shadows.up.z;cb.shadowSize=float(shadows.size);
        if(hybrid){
            const HourlyIrradiance ir=irradianceAt(solarHour);
            cb.indirectSky[0]=ir.sky.x;cb.indirectSky[1]=ir.sky.y;cb.indirectSky[2]=ir.sky.z;
            cb.indirectBounce[0]=ir.bounce.x;cb.indirectBounce[1]=ir.bounce.y;cb.indirectBounce[2]=ir.bounce.z;
            cb.indirectSun[0]=ir.sun.x;cb.indirectSun[1]=ir.sun.y;cb.indirectSun[2]=ir.sun.z;
            const V origin=lumaBvh?lumaTraceCompute_.probeOrigin():probeOrigin_;
            const V invSpan=lumaBvh?lumaTraceCompute_.probeInvSpan():probeInvSpan_;
            cb.probeOrigin[0]=origin.x;cb.probeOrigin[1]=origin.y;cb.probeOrigin[2]=origin.z;
            cb.probeInvSpan[0]=invSpan.x;cb.probeInvSpan[1]=invSpan.y;cb.probeInvSpan[2]=invSpan.z;
            cb.hybridControls[0]=1.f;
            cb.hybridControls[1]=giHourMix_;
            cb.hybridControls[3]=lumaBvh?2.f:(hasBakedGi_?1.f:0.f);
        }
        cb.hybridControls[2]=tone?1.f:0.f;
        cb.colorControls[0]=balanced?1.f:0.f;
        cb.colorControls[1]=exposure;
        cb.peripheralControls[3]=lumaBvh?1.f:0.f;
        cb.reflectionControls[3]=dxR001?1.f:0.f;
        const bool zMaterialOn=zmaterial::enabled();
        for(size_t m=0;m<scene.mat.size()&&m<256;m++){
            const std::string& name=scene.mat[m].name;
            if(preservedTextureLayers_[m]>=0)
                cb.materialTextureDetail[m][0]=float(preservedTextureLayers_[m]);
            const bool carPaint=name.rfind("sports",0)==0||name=="carSilver"||name=="carBlue"||name=="carGreen"||name=="classicBody";
            const bool raptureSurface=name=="raptureStone"||name=="raptureBronze"||name=="raptureGlass";
            float r=1.f,g=1.f,b=1.f,strength=0.f;
            float spec=0.f,power=34.f,sr=178.f/255.f,sg=202.f/255.f;
            float emissionR=0.f,emissionG=0.f,emissionB=0.f,emissionStrength=0.f;
            // Lift the general environment fill without flattening direct sun.
            // The previous .21/.34/.50 mix left shaded concrete too dark.
            float diffuseBase=.30f,diffuseDay=.44f,diffuseDirect=.56f;
            if(name=="foam"){diffuseBase=.60f;diffuseDay=.30f;diffuseDirect=.24f;}
            else if(name=="orange"){diffuseBase=.34f;diffuseDay=.43f;diffuseDirect=.26f;}
            else if(carPaint){diffuseBase=.32f;diffuseDay=.42f;diffuseDirect=.32f;}
            else if(raptureSurface){diffuseBase=.42f;diffuseDay=.28f;diffuseDirect=.50f;}
            else if(name=="prp_tile_ceramic" || name=="prp_plaster_pale"){
                // Neutral, slightly stronger diffuse fill in enclosed spaces.
                diffuseBase=.47f;diffuseDay=.37f;diffuseDirect=.38f;
            }
            else if(name=="prp_concrete_panel"){
                diffuseBase=.38f;diffuseDay=.37f;diffuseDirect=.44f;
            }
            if(name=="cse_pool_strip")strength=.38f; // subdued linear luminaire
            else if(name=="lamp")strength=1.5f*lampFactor;
            else if(name=="led")strength=1.35f*lampFactor;
            else if(name=="taillight")strength=1.05f*lampFactor;
            else if(name=="neon")strength=1.1f;
            else if(name=="windowlit")strength=.88f;
            if(name=="cse_pool_strip"){
                emissionR=.83f;emissionG=.91f;emissionB=1.f;
                emissionStrength=.24f; // do not overexpose concrete
            }
            else if(name=="cse_pool_water"){
                // Negative emission sentinel identifies exactly this custom
                // scene-local water material in the pixel shader.
                emissionStrength=-1.f;
                spec=.85f;power=58.f;
                diffuseBase=.25f;diffuseDay=.26f;diffuseDirect=.18f;
            }
            else if(name=="cse_neon_red"){
                emissionR=1.f;emissionG=.09f;emissionB=.035f;emissionStrength=1.65f;
                strength=.55f;
            }
            else if(name=="cse_neon_cyan"){
                emissionR=.03f;emissionG=.89f;emissionB=1.f;emissionStrength=1.5f;
                strength=.5f;
            }
            else if(name=="cse_neon_gold"){
                emissionR=1.f;emissionG=.72f;emissionB=.24f;emissionStrength=1.35f;
                strength=.45f;
            }
            else if(name=="lamp"){emissionR=1.f;emissionG=166.f/255.f;emissionB=91.f/255.f;emissionStrength=2.65f*lampFactor;}
            else if(name=="led"){emissionR=1.f;emissionG=217.f/255.f;emissionB=165.f/255.f;emissionStrength=1.33f*lampFactor;}
            else if(name=="taillight"){emissionR=1.f;emissionG=28.f/255.f;emissionB=12.f/255.f;emissionStrength=1.25f*lampFactor;}
            if(scene.mat[m].surfaceAuthored && scene.mat[m].waterSurface){
                // Generic authored water matches the legacy DX11 water model.
                emissionStrength=-1.f;
                spec=.85f;power=58.f;
                diffuseBase=.25f;diffuseDay=.26f;diffuseDirect=.18f;
            }
            if(name=="sea"){spec=.46f;power=25.f;}
            else if(name=="puddle"){spec=.64f;power=18.f;}
            else if(name=="glass"||name=="raptureGlass"){spec=.27f;power=34.f;}
            else if(name=="raptureBronze"){spec=.20f;power=34.f;}
            else if(name=="metal"||name=="seatSteel"){spec=.24f;power=34.f;}
            else if(name=="wheelMetal"){spec=.34f;power=42.f;}
            else if(name=="carSilver"){spec=.94f;power=52.f;}
            else if(carPaint){spec=.68f;power=30.f;}
            else if(name=="carGlass"){spec=.52f;power=38.f;}
            else if(name=="taxi"){spec=.20f;power=34.f;}
            else if(name=="mutant"){spec=.23f;power=29.f;}
            else if(name=="scar"){spec=.28f;power=35.f;}
            else if(name=="wound"){spec=.19f;power=20.f;}
            else if(name=="asphalt"){spec=.16f;power=13.f;}
            else if(name=="orange"){spec=.13f;power=42.f;}
            // The Pripyat material IDs carry generic PBR parameters rather
            // than the old hardcoded names (metal, puddle, glass). Honor
            // roughness/metalness/transmission even for unseen material IDs.
            const float metal=clampf(scene.mat[m].metallic,0.f,1.f);
            const float surfaceRough=clampf(scene.mat[m].roughnessScale,.08f,1.f);
            const float transmission=float(scene.mat[m].transmissionPercent)/100.f;
            spec=std::max(spec,.025f+.29f*metal+.095f*(1.f-surfaceRough)
                          +.14f*transmission);
            if(scene.mat[m].roughnessMip.empty())
                power=clampf(2.f/(surfaceRough*surfaceRough)-2.f,8.f,80.f);
            // Generic scene-authored emission, shared by DX11 and LumaTrace.
            dx11::applyAuthoredEmission(scene.mat[m],emissionR,emissionG,emissionB,emissionStrength,strength);
            cb.materialGlow[m][0]=r;cb.materialGlow[m][1]=g;cb.materialGlow[m][2]=b;cb.materialGlow[m][3]=strength;
            const auto zm=zmaterial::prepare(metal,scene.mat[m].roughnessScale,transmission==0.f,
                emissionStrength<0.f,emissionStrength>0.f||strength>0.f,zMaterialOn);
            cb.materialTextureDetail[m][1]=zm.metal;
            cb.materialTextureDetail[m][2]=zm.diffuse;
            cb.materialTextureDetail[m][3]=zm.roughScale;
            cb.materialSpecular[m][0]=spec;cb.materialSpecular[m][1]=power;cb.materialSpecular[m][2]=scene.mat[m].roughnessMip.empty()?0.f:1.f;cb.materialSpecular[m][3]=metal;
            cb.materialOpacity[m][0]=1.f-float(scene.mat[m].transmissionPercent)/100.f;
            cb.materialOpacity[m][1]=scene.mat[m].normalMip.empty()?0.f:1.f;
            cb.materialOpacity[m][2]=zreflect::dielectricF0(scene.mat[m].refractiveIndex);
            cb.materialDiffuse[m][0]=diffuseBase;cb.materialDiffuse[m][1]=diffuseDay;cb.materialDiffuse[m][2]=diffuseDirect;cb.materialDiffuse[m][3]=daylight;
            cb.materialEmission[m][0]=emissionR;cb.materialEmission[m][1]=emissionG;cb.materialEmission[m][2]=emissionB;cb.materialEmission[m][3]=emissionStrength;
        }
        const float sceneLightScale=.42f+.58f*(1.f-daylight);
        uint32_t dxLightReferenceCount=0;
        auto appendLight=[&](const Light& l,float scale){
            // Preserve the original first-256 selection and summation order.
            if(dxLightReferenceCount>=256)return;
            ++dxLightReferenceCount;
            if(dxR001&&!dx11::r001::lightMayContribute(dxFrustum,l.p,l.radius,l.intensity*scale,l.color))return;
            const uint32_t i=cb.lightCount++;
            cb.lightPositionRadius[i][0]=l.p.x;cb.lightPositionRadius[i][1]=l.p.y;cb.lightPositionRadius[i][2]=l.p.z;cb.lightPositionRadius[i][3]=l.radius;
            cb.lightColorIntensity[i][0]=l.color.x/255.f;cb.lightColorIntensity[i][1]=l.color.y/255.f;cb.lightColorIntensity[i][2]=l.color.z/255.f;cb.lightColorIntensity[i][3]=l.intensity*scale;
        };
        for(const Light& l:pointLights)appendLight(l,1.f);
        for(const Light& l:scene.lights){
            // Cornice lights and the five pairs of plaza globes are electric:
            // fade in as dusk approaches.  The other indoor sources keep
            // their existing scene-wide scale and remain functional by day.
            const bool exteriorFixture=(l.p.y>17.f&&l.p.z>-4.f&&l.p.z<-1.f)
                          ||(l.p.y>6.f&&l.p.y<9.f&&l.p.z>18.f);
            const float factor=(l.exterior||exteriorFixture)?lampFactor:sceneLightScale;
            appendLight(l,factor);
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr=context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
        if(FAILED(hr)){error=L"Impossible de transmettre les paramètres à DirectX 11.";return false;}
        std::memcpy(mapped.pData,&cb,sizeof(cb));context_->Unmap(constants_.Get(),0);
        // Animate wind on the sky hemisphere; geometry and PBR are untouched.
        CloudConstants cloudCB{};
        cloudCB.forward[0]=cam.f.x;cloudCB.forward[1]=cam.f.y;cloudCB.forward[2]=cam.f.z;
        cloudCB.tanHalf=cam.tanHalf;
        cloudCB.right[0]=cam.r.x;cloudCB.right[1]=cam.r.y;cloudCB.right[2]=cam.r.z;
        cloudCB.aspect=cam.aspect;
        cloudCB.up[0]=cam.u.x;cloudCB.up[1]=cam.u.y;cloudCB.up[2]=cam.u.z;
        cloudCB.sun[0]=sun.x;cloudCB.sun[1]=sun.y;cloudCB.sun[2]=sun.z;
        cloudCB.day=daylight;
        const V zenith=skyZenith,horizon=skyHorizon;
        const float zen[3]{zenith.x,zenith.y,zenith.z};
        const float hor[3]{horizon.x,horizon.y,horizon.z};
        for(int c=0;c<3;++c){
            cloudCB.zenith[c]=balanced?decodeSrgb(zen[c]/255.f):zen[c]/255.f;
            cloudCB.horizon[c]=balanced?decodeSrgb(hor[c]/255.f):hor[c]/255.f;
        }
        cse::weather::CloudState cloudSettings;
        cloudSettings.coverage=.28f;
        cloudSettings.opacity=.65f;
        cloudSettings.windEastMetersPerSecond=12.f;
        cloudSettings.windNorthMetersPerSecond=3.5f;
        static const auto cloudEpoch=std::chrono::steady_clock::now();
        // Deterministic bench renders remain pixel-exact across readback modes;
        // only the interactive GUI opts into real-time cloud motion.
        const char* movingSky=std::getenv("M3D_V17_ANIMATE_CLOUDS");
        const bool animateSky=movingSky && std::strcmp(movingSky,"1")==0;
        const double cloudSeconds=animateSky ? std::chrono::duration<double>(
            std::chrono::steady_clock::now()-cloudEpoch).count()*
            double(cloudTimeScale.load(std::memory_order_acquire)) : 0.0;
        const auto clouds=cse::weather::cirrusAt(cloudSettings,cloudSeconds,
            std::asin(clampf(sun.y,-1.f,1.f))*180.f/PI);
        cloudCB.wind[0]=clouds.uOffset;cloudCB.wind[1]=clouds.vOffset;
        cloudCB.wind[2]=solarHour; // Night stars rotate with the time of day.
        const char* splitSky=std::getenv("M3D_V17_SPLIT_SKY");
        // Native flip presentation needs its GPU sky; only QImage uses the split alpha sky.
        cloudCB.wind[3]=(!nativeHwnd && splitSky && std::strcmp(splitSky,"1")==0)?1.f:0.f;
        const bool showClouds=rasterCloudsEnabled.load(std::memory_order_acquire);
        cloudCB.coverage=showClouds?clouds.coverage:0.f;
        cloudCB.opacity=showClouds?clouds.opacity:0.f;
        D3D11_MAPPED_SUBRESOURCE cloudMapped{};
        if(FAILED(context_->Map(skyConstants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&cloudMapped))){
            error=L"Upload paramètres nuages impossible.";return false;
        }
        std::memcpy(cloudMapped.pData,&cloudCB,sizeof(cloudCB));
        context_->Unmap(skyConstants_.Get(),0);
        const float clear[4]={cb.sky[0],cb.sky[1],cb.sky[2],1.f};
        EscargotFrameProfile frameProfile(device_.Get(),context_.Get(),profileEnabled);frameProfile.start();
        if(slot.queryReady){context_->Begin(slot.disjoint.Get());context_->End(slot.start.Get());}
        if(lumaBvh&&!lumaTraceCompute_.dispatch(context_.Get(),verticesSrv_.Get(),indicesSrv_.Get(),scene,
            albedo_.Get(),preservedAlbedo_.Get(),sampler_.Get(),preservedTextureLayers_,hybrid,adaptiveRequested,error))return false;
        const bool adaptiveEnabled=lumaBvh&&lumaTraceCompute_.adaptiveEnabled();
        if(adaptiveEnabled){
            cb.padding[0]=1.f;
            std::memcpy(cb.adaptiveZero,lumaTraceCompute_.zeroMask().data(),sizeof(cb.adaptiveZero));
        }
        UINT submittedVertices=0,submittedBoxInstances=0;
        uint64_t drawCalls=0,submittedEllipseTriangles=0,submittedEllipseInstances=0;
        uint64_t submittedFoliageTriangles=0,submittedFoliageInstances=0;
        Camera activeCam=cam;
        ZReflectPassStats reflectPasses[2]{};
        size_t reflectSlot=0;
        auto drawScene=[&](ID3D11RenderTargetView* rt,ID3D11DepthStencilView* ds,
                           int pw,int ph,bool scissor,bool skipCore)->bool{
            if(multires){
                cb.peripheralControls[0]=skipCore?1.f:(scissor?2.f:0.f);
                cb.peripheralControls[1]=1.f/float(pw);
                cb.peripheralControls[2]=1.f/float(ph);
            }
            cb.eye[0]=activeCam.eye.x;cb.eye[1]=activeCam.eye.y;cb.eye[2]=activeCam.eye.z;
            cb.forward[0]=activeCam.f.x;cb.forward[1]=activeCam.f.y;cb.forward[2]=activeCam.f.z;
            cb.right[0]=activeCam.r.x;cb.right[1]=activeCam.r.y;cb.right[2]=activeCam.r.z;
            cb.up[0]=activeCam.u.x;cb.up[1]=activeCam.u.y;cb.up[2]=activeCam.u.z;
            cb.tanHalf=activeCam.tanHalf;cb.aspect=activeCam.aspect;
            cb.reflectionControls[1]=1.f/float(pw);cb.reflectionControls[2]=1.f/float(ph);
            D3D11_MAPPED_SUBRESOURCE mp{};
            if(FAILED(context_->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mp))){
                error=L"Upload constantes de rendu impossible.";return false;
            }
            std::memcpy(mp.pData,&cb,sizeof(cb));context_->Unmap(constants_.Get(),0);
            if(anyReflection){
                cloudCB.forward[0]=activeCam.f.x;cloudCB.forward[1]=activeCam.f.y;cloudCB.forward[2]=activeCam.f.z;
                cloudCB.right[0]=activeCam.r.x;cloudCB.right[1]=activeCam.r.y;cloudCB.right[2]=activeCam.r.z;
                cloudCB.up[0]=activeCam.u.x;cloudCB.up[1]=activeCam.u.y;cloudCB.up[2]=activeCam.u.z;
                cloudCB.aspect=activeCam.aspect;cloudCB.tanHalf=activeCam.tanHalf;
                D3D11_MAPPED_SUBRESOURCE skyMap{};
                if(FAILED(context_->Map(skyConstants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&skyMap))){
                    error=L"Upload constantes du ciel réfléchi impossible.";return false;}
                std::memcpy(skyMap.pData,&cloudCB,sizeof(cloudCB));context_->Unmap(skyConstants_.Get(),0);
            }
            context_->OMSetRenderTargets(1,&rt,ds);
            context_->ClearRenderTargetView(rt,clear);
            context_->ClearDepthStencilView(ds,D3D11_CLEAR_DEPTH,1.f,0);
            D3D11_VIEWPORT vp{};vp.Width=float(pw);vp.Height=float(ph);
            vp.MinDepth=0;vp.MaxDepth=1;context_->RSSetViewports(1,&vp);
            // Draw the clouded sky first, with depth completely disabled.
            context_->RSSetState(raster_.Get());
            context_->OMSetDepthStencilState(skyDepth_.Get(),0);
            context_->OMSetBlendState(nullptr,nullptr,0xffffffffu);
            context_->IASetInputLayout(nullptr);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(skyVs_.Get(),nullptr,0);
            context_->PSSetShader(skyPs_.Get(),nullptr,0);
            ID3D11Buffer* skyCb=skyConstants_.Get();
            context_->PSSetConstantBuffers(1,1,&skyCb);
            context_->Draw(3,0);
            if(scissor){
                const D3D11_RECT rect={LONG(std::floor(pw*.03f)),LONG(std::floor(ph*.02f)),
                    LONG(std::ceil(pw*.97f)),LONG(std::ceil(ph*.98f))};
                context_->RSSetScissorRects(1,&rect);
            }
            UINT stride=lumaBvh?UINT(sizeof(GpuVertexLuma)):UINT(sizeof(GpuVertex)),offset=0;
            ID3D11Buffer* vb=vertices_.Get();
            context_->IASetIndexBuffer(indexBuffer_.Get(),DXGI_FORMAT_R32_UINT,0);
            context_->IASetInputLayout(lumaBvh?lumaLayout_.Get():layout_.Get());
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->RSSetState(scissor?scissorRaster_.Get():raster_.Get());
            context_->OMSetDepthStencilState(depth_.Get(),0);
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->PSSetShader(escDirect?objectPs_.Get():ps_.Get(),nullptr,0);
            ID3D11Buffer* cbuff=constants_.Get();
            context_->VSSetConstantBuffers(0,1,&cbuff);
            context_->PSSetConstantBuffers(0,1,&cbuff);
            ID3D11ShaderResourceView* srvs[]={albedo_.Get(),shadowSrv_.Get(),
                roughness_.Get(),normal_.Get(),hybrid?(lumaBvh?lumaTraceCompute_.probeSrv():probeSrv_.Get()):nullptr,
                hybrid?(lumaBvh?lumaTraceCompute_.probeSrv():(hasBakedGi_?probeNextSrv_.Get():probeSrv_.Get())):nullptr,
                preservedAlbedo_?preservedAlbedo_.Get():albedo_.Get(),
                lumaBvh?lumaTraceCompute_.radianceSrv():nullptr,
                escProjected?projectedOptics_.view():nullptr, // Object005 projected layers
                cb.zPlaneIds[3]<.5f&&cb.zPlaneIds[2]>.5f?zTargets_[0].srv.Get():nullptr,
                cb.zPlaneIds[3]<.5f&&cb.zPlaneIds[2]>1.5f?zTargets_[1].srv.Get():nullptr,
                escEnabled&&!escGpu&&!escDirect&&!escProjected?escColorSrv_.Get():nullptr,escEnabled&&!escGpu&&!escDirect&&!escProjected?escMetaSrv_.Get():nullptr,
                (escGpu||escProjected)?(escBuilt?escBuild_.view():escPacketSrv_.Get()):nullptr};
            context_->PSSetShaderResources(0,14,srvs);
            if(escDirect)objectOptics_.bind(context_.Get(),lumaTraceCompute_.bvhSrv());else escargot_gpu::RasterOptics::unbind(context_.Get());
            ID3D11SamplerState* samplers[]={sampler_.Get(),probeSampler_.Get()};
            context_->PSSetSamplers(0,2,samplers);
        
        
        const char* noCull=std::getenv("M3D_V17_DISABLE_FRUSTUM_CULLING");
        const bool cullEnabled=quality.frustumCulling
            &&!(noCull && std::strcmp(noCull,"1")==0);
        // Reuse existing opaque geometry for the depth-only pass. This avoids
        // repeating expensive pixel-light loops on hidden fragments; it does
        // not add CPU ray casting, material work, or per-triangle sorting.
        const char* zPrepassEnv=std::getenv("M3D_V17_EXPERIMENT_Z_PREPASS");
        const char* disableZPrepass=std::getenv("M3D_V17_DISABLE_Z_PREPASS");
        const bool secondary=cb.zPlaneIds[3]>.5f;
        const char* disableReflectOpt=std::getenv("M3D_V17_DISABLE_ZREFLECT_DEPTH_OPT");
        const bool legacyDisabled=disableReflectOpt&&std::strcmp(disableReflectOpt,"1")==0;
        // V17.023: measured configuration C. Explicit independent overrides
        // retain all four benchmark modes; legacy disable=1 still disables both.
        auto reflectOption=[&](const char* key,bool defaultEnabled){
            const char* v=std::getenv(key);
            return v&&std::strcmp(v,"0")==0?false:v&&std::strcmp(v,"1")==0?true:(!legacyDisabled&&defaultEnabled);
        };
        const bool reflectionCull=secondary&&reflectOption("M3D_V17_ZREFLECT_CPU_CULL",true);
        const bool reflectionDepth=secondary&&reflectOption("M3D_V17_ZREFLECT_PREPASS",false);
        uint64_t reflectColorTriangles=0,reflectDepthTriangles=0;
        // Conservative support point of the AABB against the mirror clip plane.
        // A margin accommodates float interpolation; intersecting chunks survive.
        auto visibleForPass=[&](const DrawChunk& chunk){
            if(dxR001){if(!dxFrustum.visible(chunk.mn,chunk.mx))return false;}
            else if(!visibleChunk(chunk,activeCam))return false;
            if(!reflectionCull)return true;
            const auto& n=cb.zClip;
            const double x=double(n[0])*(n[0]>=0?chunk.mx.x:chunk.mn.x);
            const double y=double(n[1])*(n[1]>=0?chunk.mx.y:chunk.mn.y);
            const double z=double(n[2])*(n[2]>=0?chunk.mx.z:chunk.mn.z);
            const double d=n[3],margin=.01+1.e-5*(std::abs(x)+std::abs(y)+std::abs(z)+std::abs(d));
            return !(x+y+z+d < -margin);
        };
        const bool depthPrepass=(!secondary||reflectionDepth) && !(disableZPrepass&&std::strcmp(disableZPrepass,"1")==0)
            &&!(zPrepassEnv&&std::strcmp(zPrepassEnv,"0")==0);
        const bool reuseVisibility=dxR001&&cullEnabled&&depthPrepass;
        if(reuseVisibility){
            dx11::r001::fillVisibilityMask(chunks_,dxChunkMask_,visibleForPass);
            dx11::r001::fillVisibilityMask(boxChunks_,dxBoxMask_,visibleForPass);
        }
        auto chunkVisible=[&](const DrawChunk& chunk,const std::vector<DrawChunk>& source,const std::vector<uint8_t>& mask){
            return reuseVisibility?mask[size_t(&chunk-source.data())]!=0:visibleForPass(chunk);
        };
        if(depthPrepass){
            context_->OMSetRenderTargets(0,nullptr,ds);
            context_->PSSetShader(secondary?reflectionDepthPs_.Get():nullptr,nullptr,0);
            // Static opaque walls, floors and other architectural meshes.
            if(vertexCount_){
                if(!cullEnabled){if(staticGeometryPreparationEnabled_)context_->DrawIndexed(indexCount_,0,0);else context_->Draw(vertexCount_,0);reflectDepthTriangles+=(staticGeometryPreparationEnabled_?indexCount_:vertexCount_)/3;}
                else{
                    UINT start=0,count=0;
                    auto flush=[&]{if(count){if(staticGeometryPreparationEnabled_)context_->DrawIndexed(count,start,0);else context_->Draw(count,start);reflectDepthTriangles+=count/3;count=0;}};
                    for(const DrawChunk& chunk:chunks_){
                        if(!chunkVisible(chunk,chunks_,dxChunkMask_)){flush();continue;}
                        if(count&&start+count!=chunk.first)flush();
                        if(!count)start=chunk.first;
                        count+=chunk.count;
                    }
                    flush();
                }
            }
            // Instanced opaque boxes: buildings/large occluders.
            if(boxInstanceCount_){
                ID3D11Buffer* buffers[]={boxVertices_.Get(),boxInstances_.Get()};
                UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuBoxInstance))};
                UINT offsets[]={0,0};
                context_->IASetInputLayout(boxLayout_.Get());
                context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
                context_->VSSetShader(boxVs_.Get(),nullptr,0);
                if(!cullEnabled){context_->DrawInstanced(36,boxInstanceCount_,0,0);reflectDepthTriangles+=uint64_t(boxInstanceCount_)*12;}
                else{
                    UINT first=0,count=0;
                    auto flush=[&]{
                        if(count){context_->DrawInstanced(36,count,0,first);reflectDepthTriangles+=uint64_t(count)*12;count=0;}
                    };
                    for(const DrawChunk& chunk:boxChunks_){
                        if(!chunkVisible(chunk,boxChunks_,dxBoxMask_)){flush();continue;}
                        if(count&&first+count!=chunk.first)flush();
                        if(!count)first=chunk.first;
                        count+=chunk.count;
                    }
                    flush();
                }
                context_->IASetInputLayout(layout_.Get());
                context_->VSSetShader(vs_.Get(),nullptr,0);
                context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
            }
            // Restore the selected color shader, including Object004, after Z-prepass.
            context_->OMSetRenderTargets(1,&rt,ds);
            context_->PSSetShader(escDirect?objectPs_.Get():ps_.Get(),nullptr,0);
        }
        if(vertexCount_){
            if(!cullEnabled){
                if(staticGeometryPreparationEnabled_)context_->DrawIndexed(indexCount_,0,0);
                else context_->Draw(vertexCount_,0);
                ++drawCalls;
                submittedVertices=staticGeometryPreparationEnabled_?indexCount_:vertexCount_;
                reflectColorTriangles+=submittedVertices/3;
            }else{
                // Merge adjacent visible batches to avoid thousands of draw
                // calls. A scene with every batch visible stays one Draw().
                UINT start=0,count=0;
                auto flush=[&]{
                    if(!count)return;
                    if(staticGeometryPreparationEnabled_)context_->DrawIndexed(count,start,0);else context_->Draw(count,start);
                    ++drawCalls;
                    submittedVertices+=count;
                    reflectColorTriangles+=count/3;
                    count=0;
                };
                for(const DrawChunk& chunk:chunks_){
                    if(!chunkVisible(chunk,chunks_,dxChunkMask_)){flush();continue;}
                    if(count && start+count!=chunk.first)flush();
                    if(!count)start=chunk.first;
                    count+=chunk.count;
                }
                flush();
            }
        }
        
        if(boxInstanceCount_){
            ID3D11Buffer* buffers[]={boxVertices_.Get(),boxInstances_.Get()};
            UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuBoxInstance))};
            UINT offsets[]={0,0};
            context_->IASetInputLayout(boxLayout_.Get());
            context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
            context_->VSSetShader(boxVs_.Get(),nullptr,0);
            auto drawBoxes=[&](UINT first,UINT count){
                if(!count)return;
                context_->DrawInstanced(36,count,0,first);
                ++drawCalls;
                submittedBoxInstances+=count;
                reflectColorTriangles+=uint64_t(count)*12;
            };
            if(!cullEnabled){
                drawBoxes(0,boxInstanceCount_);
            }else{
                UINT first=0,count=0;
                auto flush=[&]{drawBoxes(first,count);count=0;};
                for(const DrawChunk& chunk:boxChunks_){
                    if(!chunkVisible(chunk,boxChunks_,dxBoxMask_)){flush();continue;}
                    if(count && first+count!=chunk.first)flush();
                    if(!count)first=chunk.first;
                    count+=chunk.count;
                }
                flush();
            }
            context_->IASetInputLayout(layout_.Get());
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        
        
        if(ellipseInstanceCount_){
            std::array<std::vector<GpuEllipseInstance>,3> level;
            for(auto& bucket:level)bucket.reserve(ellipseInstanceCount_/3+1);
            const char* disableLod=std::getenv("M3D_V17_DISABLE_LOD");
            const bool lodEnabled=quality.dynamicLod
                &&!(disableLod && std::strcmp(disableLod,"1")==0);
            for(const auto& sphere:ellipses_){
                const V center{sphere.center[0],sphere.center[1],sphere.center[2]};
                const float radius=std::max({sphere.radii[0],sphere.radii[1],sphere.radii[2]});
                const DrawChunk bounds{0,0,{center.x-radius,center.y-radius,center.z-radius},
                                           {center.x+radius,center.y+radius,center.z+radius}};
                if(cullEnabled&&!visibleForPass(bounds))continue;
                const float z=std::max(.10f,dot(center-activeCam.eye,activeCam.f));
                const float projectedRadius=radius*float(pw)/(2.f*std::max(.01f,activeCam.tanHalf)*z);
                const float lodScale=100.f/float(quality.lodQualityPercent);
                const int lod=(!lodEnabled||projectedRadius>=48.f*lodScale)?2
                    :(projectedRadius>=14.f*lodScale?1:0);
                level[lod].push_back(sphere);
            }
            // One small streaming allocation/update for all visible
            // ellipsoids, not a per-object Draw() or a CPU rebuild.
            D3D11_MAPPED_SUBRESOURCE ellipseMap{};
            hr=context_->Map(ellipseInstances_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&ellipseMap);
            if(FAILED(hr)){error=L"Mise à jour des LOD ellipsoïdes impossible.";return false;}
            GpuEllipseInstance* dest=static_cast<GpuEllipseInstance*>(ellipseMap.pData);
            for(const auto& bucket:level){
                if(bucket.empty())continue;
                std::memcpy(dest,bucket.data(),bucket.size()*sizeof(GpuEllipseInstance));
                dest+=bucket.size();
            }
            context_->Unmap(ellipseInstances_.Get(),0);
            context_->IASetInputLayout(ellipseLayout_.Get());
            context_->VSSetShader(ellipseVs_.Get(),nullptr,0);
            UINT first=0;
            for(size_t lod=0;lod<level.size();++lod){
                const UINT count=UINT(level[lod].size());
                if(!count)continue;
                ID3D11Buffer* buffers[]={ellipseVertices_[lod].Get(),ellipseInstances_.Get()};
                UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuEllipseInstance))};
                UINT offsets[]={0,0};
                context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
                context_->DrawInstanced(ellipseVertexCounts_[lod],count,0,first);
                ++drawCalls;
                submittedEllipseInstances+=count;
                submittedEllipseTriangles+=uint64_t(ellipseVertexCounts_[lod]/3)*count;
                reflectColorTriangles+=uint64_t(ellipseVertexCounts_[lod]/3)*count;
                first+=count;
            }
            context_->IASetInputLayout(layout_.Get());
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        
        
        if(foliageInstanceCount_){
            std::array<std::vector<GpuEllipseInstance>,3> buckets;
            for(auto& bucket:buckets)bucket.reserve(foliageInstanceCount_/3+1);
            const char* disabled=std::getenv("M3D_V17_DISABLE_LOD");
            const bool lodEnabled=quality.dynamicLod
                &&!(disabled&&std::strcmp(disabled,"1")==0);
            for(const auto& plant:foliage_){
                const V center{plant.center[0],plant.center[1],plant.center[2]};
                const float radius=std::max({plant.radii[0],plant.radii[1],plant.radii[2]});
                const DrawChunk bounds{0,0,{center.x-radius,center.y-radius,center.z-radius},
                    {center.x+radius,center.y+radius,center.z+radius}};
                if(cullEnabled&&!visibleForPass(bounds))continue;
                const float z=std::max(.10f,dot(center-activeCam.eye,activeCam.f));
                const float projectedRadius=radius*float(pw)/(2.f*std::max(.01f,activeCam.tanHalf)*z);
                const float lodScale=100.f/float(quality.lodQualityPercent);
                const int lod=(!lodEnabled||projectedRadius>=40.f*lodScale)?2
                    :(projectedRadius>=12.f*lodScale?1:0);
                buckets[lod].push_back(plant);
            }
            D3D11_MAPPED_SUBRESOURCE foliageMap{};
            hr=context_->Map(foliageInstances_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&foliageMap);
            if(FAILED(hr)){error=L"Mise à jour des LOD du feuillage impossible.";return false;}
            GpuEllipseInstance* dest=static_cast<GpuEllipseInstance*>(foliageMap.pData);
            for(const auto& bucket:buckets){
                if(bucket.empty())continue;
                std::memcpy(dest,bucket.data(),bucket.size()*sizeof(GpuEllipseInstance));
                dest+=bucket.size();
            }
            context_->Unmap(foliageInstances_.Get(),0);
            context_->IASetInputLayout(ellipseLayout_.Get());
            context_->VSSetShader(ellipseVs_.Get(),nullptr,0);
            UINT first=0;
            for(size_t lod=0;lod<buckets.size();++lod){
                const UINT count=UINT(buckets[lod].size());
                if(!count)continue;
                ID3D11Buffer* buffers[]={foliageVertices_[lod].Get(),foliageInstances_.Get()};
                UINT strides[]={UINT(sizeof(GpuVertex)),UINT(sizeof(GpuEllipseInstance))};
                UINT offsets[]={0,0};
                context_->IASetVertexBuffers(0,2,buffers,strides,offsets);
                context_->DrawInstanced(foliageVertexCounts_[lod],count,0,first);
                ++drawCalls;
                submittedFoliageTriangles+=uint64_t(foliageVertexCounts_[lod]/3)*count;
                reflectColorTriangles+=uint64_t(foliageVertexCounts_[lod]/3)*count;
                submittedFoliageInstances+=count;
                first+=count;
            }
            context_->IASetInputLayout(layout_.Get());
            context_->VSSetShader(vs_.Get(),nullptr,0);
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
        if(!transparent_.empty()&&cb.zPlaneIds[3]<.5f){
            std::sort(transparent_.begin(),transparent_.end(),[&](const TransparentTriangle& a,const TransparentTriangle& b){return dot(a.center-activeCam.eye,activeCam.f)>dot(b.center-activeCam.eye,activeCam.f);});
            D3D11_MAPPED_SUBRESOURCE transparentMap{};hr=context_->Map(transparentVertices_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&transparentMap);
            if(FAILED(hr)){error=L"Impossible de préparer les surfaces transparentes DirectX 11.";return false;}
            auto* out=static_cast<GpuVertex*>(transparentMap.pData);for(const auto& t:transparent_)for(const auto& v:t.v)*out++=v;context_->Unmap(transparentVertices_.Get(),0);
            context_->IASetInputLayout(layout_.Get());
            UINT transparentStride=sizeof(GpuVertex);ID3D11Buffer* transparentBuffer=transparentVertices_.Get();context_->IASetVertexBuffers(0,1,&transparentBuffer,&transparentStride,&offset);
            context_->OMSetDepthStencilState(transparentDepth_.Get(),0);context_->OMSetBlendState(blend_.Get(),nullptr,0xffffffffu);context_->Draw(UINT(transparent_.size()*3),0);++drawCalls;
            context_->OMSetBlendState(nullptr,nullptr,0xffffffffu);context_->OMSetDepthStencilState(depth_.Get(),0);
            context_->IASetInputLayout(lumaBvh?lumaLayout_.Get():layout_.Get());
            context_->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        }
            if(secondary&&reflectSlot<2){
                auto& p=reflectPasses[reflectSlot];
                p.colorTriangles=reflectColorTriangles;p.depthTriangles=reflectDepthTriangles;
                p.cpuCull=reflectionCull&&cullEnabled;p.depthPrepass=depthPrepass;
            }
            return true;
        };
        // Secondary passes only render opaque geometry. No recursive reflections.
        ID3D11ShaderResourceView* noOptics[3]={nullptr,nullptr,nullptr};
        context_->PSSetShaderResources(8,3,noOptics);
        double reflectionGpuSum=0.;uint32_t reflectionGpuSamples=0;
        const char* timingEnv=std::getenv("M3D_V17_ZREFLECT_GPU_TIMING");
        ReflectionTimer* timing=nativeHwnd&&anyReflection&&timingEnv&&std::strcmp(timingEnv,"1")==0
            ?acquireReflectionTimer(reflectionGpuSum,reflectionGpuSamples):nullptr;
        if(timing){context_->Begin(timing->disjoint.Get());context_->End(timing->begin.Get());}
        frameProfile.mark(1);const auto reflectSubmitStart=Clock::now();
        if(escProjected&&!projectedOptics_.render(device_.Get(),context_.Get(),scene,cam,shadows,shadowRevision_,shadowSrv_.Get(),*escargotInput->build,w,h,clear,constants_.Get(),albedo_.Get(),preservedAlbedo_?preservedAlbedo_.Get():albedo_.Get(),sampler_.Get(),probeSampler_.Get(),scissorRaster_.Get(),depth_.Get(),transparentDepth_.Get(),blend_.Get(),escProfile,error))return false;
        for(size_t i=0;i<zActive_.size();++i){
            const auto id=zActive_[i].id;const auto& p=zRuntime_.patch(id);
            reflectSlot=i;reflectPasses[i].planeId=id;
            const auto m=zreflect::mirrorCamera(cam,p.normal,p.offset);
            const float sign=zreflect::planeDistance(cam.eye,p)>=0?1.f:-1.f;
            cb.zClip[0]=sign*p.normal.x;cb.zClip[1]=sign*p.normal.y;cb.zClip[2]=sign*p.normal.z;
            cb.zClip[3]=-sign*float(p.offset)-(zRuntime_.water(id)?0.f:.001f);
            cb.zPlaneIds[3]=1.f;activeCam=m;
            cb.reflectionControls[0]=zRuntime_.water(id)?1.f:0.f; // water-specific clipping only
            if(!drawScene(zTargets_[i].rtv.Get(),zTargets_[i].dsv.Get(),zTargets_[i].w,zTargets_[i].h,false,false)){
                if(timing){context_->End(timing->end.Get());context_->End(timing->disjoint.Get());timing->pending=true;++reflectionTimingEpoch_;}
                return false;
            }
            context_->OMSetRenderTargets(0,nullptr,nullptr);
            cb.zPlaneIds[i]=float(id+1u);
            auto put4=[](float* dest,V v,float w){dest[0]=v.x;dest[1]=v.y;dest[2]=v.z;dest[3]=w;};
            put4(cb.zEye[i],m.eye,m.tanHalf);put4(cb.zForward[i],m.f,m.aspect);
            put4(cb.zRight[i],m.r,0);put4(cb.zUp[i],m.u,0);
        }
        if(timing){context_->End(timing->end.Get());context_->End(timing->disjoint.Get());timing->pending=true;}
        frameProfile.mark(2);escProfile.opticalSubmitMs=std::chrono::duration<double,std::milli>(Clock::now()-reflectSubmitStart).count();
        const double reflectSubmitMs=zActive_.empty()?0.:std::chrono::duration<double,std::milli>(Clock::now()-reflectSubmitStart).count();
        cb.zPlaneIds[3]=0.f;activeCam=cam;
        cb.reflectionControls[0]=0.f;
        cb.zPlaneIds[2]=escProjected?-4.f:escDirect?-3.f:(escGpu?-2.f:(escEnabled?-1.f:float(zActive_.size())));
        if(multires){
            if(!drawScene(lowRtv_.Get(),lowDsv_.Get(),lowW_,lowH_,false,true) ||
               !drawScene(rtv_.Get(),dsv_.Get(),w,h,true,false))return false;
            if(sampleCount_>1)
                context_->ResolveSubresource(resolve_.Get(),0,target_.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
            context_->OMSetRenderTargets(1,mergeRtv_.GetAddressOf(),nullptr);
            D3D11_VIEWPORT vp{};vp.Width=float(w);vp.Height=float(h);vp.MinDepth=0;vp.MaxDepth=1;
            context_->RSSetViewports(1,&vp);
            context_->RSSetState(raster_.Get());
            context_->OMSetDepthStencilState(nullptr,0);
            context_->OMSetBlendState(nullptr,nullptr,0xffffffffu);
            context_->IASetInputLayout(nullptr);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(mergeVs_.Get(),nullptr,0);
            context_->PSSetShader(mergePs_.Get(),nullptr,0);
            ID3D11ShaderResourceView* srcs[]={lowSrv_.Get(),
                sampleCount_>1?resolveSrv_.Get():targetSrv_.Get()};
            context_->PSSetShaderResources(0,2,srcs);
            ID3D11SamplerState* spl=probeSampler_.Get();
            context_->PSSetSamplers(0,1,&spl);
            context_->Draw(3,0);++drawCalls;
            ID3D11ShaderResourceView* empty[]={nullptr,nullptr};
            context_->PSSetShaderResources(0,2,empty);
        }else if(!drawScene(rtv_.Get(),dsv_.Get(),w,h,false,false))return false;
        // Apply atmosphere to primary camera only.
        const bool atmosphereOn=atmosphereDensity>0.f;
        if(atmosphereOn){
            context_->OMSetRenderTargets(0,nullptr,nullptr);
            if(sampleCount_>1)context_->ResolveSubresource(resolve_.Get(),0,target_.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
            if(!atmospherePass_.draw(device_.Get(),context_.Get(),atmosphereDepthSrv_.Get(),shadowSrv_.Get(),
                sampleCount_>1?atmosphereResolveRtv_.Get():rtv_.Get(),raster_.Get(),scene,cam,shadows,
                sampleCount_,unsigned(w),unsigned(h),atmosphereDensity,error))return false;
            ++drawCalls;
        }
        frameProfile.mark(3);
        // Native DXGI presentation: all transfers remain on the GPU. The
        // offscreen raster and MSAA targets are reused; no CPU staging,
        // Map, swizzle, QImage or Qt painter is touched on this path.
        if(nativeHwnd){
            context_->OMSetRenderTargets(0,nullptr,nullptr);
            // Validate the small packet status before presenting. No image readback.
            // Native callers never request the diagnostic full packet capture.
            if(escBuilt&&!escBuild_.finish(context_.Get(),*escargotInput->buildTiming,error)){
                if(slot.queryReady){context_->End(slot.afterDraw.Get());context_->End(slot.afterCopy.Get());context_->End(slot.disjoint.Get());}
                return false;
            }
            if(sampleCount_>1&&!multires&&!atmosphereOn)
                context_->ResolveSubresource(resolve_.Get(),0,target_.Get(),0,
                                              DXGI_FORMAT_R8G8B8A8_UNORM);
            ID3D11Texture2D* gpuSource=multires?mergeTex_.Get():
                (sampleCount_>1?resolve_.Get():target_.Get());
            ComPtr<ID3D11Texture2D> backBuffer;
            HRESULT hr=swapchain_->GetBuffer(0,IID_PPV_ARGS(&backBuffer));
            if(FAILED(hr)){
                error=L"DirectX 11 : tampon DXGI inaccessible.";
                releasePresenter();return false;
            }
            context_->CopyResource(backBuffer.Get(),gpuSource);
            backBuffer.Reset(); // flip-model swapchain must not retain the backbuffer
            frameProfile.finish(escProfile);
            if(slot.queryReady){
                context_->End(slot.afterDraw.Get());
                context_->End(slot.afterCopy.Get());
                context_->End(slot.disjoint.Get());
            }
            static const UINT syncInterval=[]() -> UINT {
                const char* value=std::getenv("CSE_WARP_DX_BENCH_PRESENT_INTERVAL");
                return value && std::strcmp(value,"1")==0 ? 1u : 0u;
            }();
            hr=swapchain_->Present(syncInterval,0);
            if(syncInterval && hr==DXGI_STATUS_OCCLUDED){
                error=L"DXGI presentation window is occluded.";
                return false;
            }
            if(FAILED(hr)){
                error=L"DirectX 11 : échec de présentation DXGI.";
                releasePresenter();return false;
            }
            if(nativeStats){
                *nativeStats={};nativeStats->escProfile=escProfile;
                nativeStats->gpu=true;
                nativeStats->threads=0;
                nativeStats->readbackBufferCount=0;
                nativeStats->gpuDrawCalls=drawCalls;
                nativeStats->dxR001Enabled=dxR001;
                nativeStats->dxLightReferenceCount=dxLightReferenceCount;
                nativeStats->dxLightSubmittedCount=cb.lightCount;
                nativeStats->planarReflectionRendered=planarReflection;
                for(size_t i=0;i<2;++i)nativeStats->zReflectPasses[i]=reflectPasses[i];
                nativeStats->zReflectActive=uint32_t(zActive_.size());nativeStats->zReflectCandidates=uint32_t(zStats.candidates);
                nativeStats->zReflectNodeVisits=uint32_t(zStats.nodeVisits);nativeStats->zReflectDeferred=zDeferred;
                nativeStats->zReflectTotal=zTotal;nativeStats->zReflectSelectUs=zSelectUs;nativeStats->zReflectSubmitMs=reflectSubmitMs;nativeStats->zReflectGpuMs=reflectionGpuSum;nativeStats->zReflectGpuSamples=reflectionGpuSamples;nativeStats->zReflectEnabled=zEnabled;
                nativeStats->lumaProbeLightingEnabled=lumaBvh&&hybrid;
                nativeStats->lumaAdaptiveEnabled=adaptiveEnabled;
                nativeStats->lumaAdaptiveZeroTriangles=adaptiveEnabled?lumaTraceCompute_.zeroTriangles():0;
                nativeStats->lumaAdaptiveTriangleCount=lumaBvh?lumaTraceCompute_.triangleCount():0;
                nativeStats->geometryPreparationEnabled=staticGeometryPreparationEnabled_;
                nativeStats->geometrySourceVertices=sourceVertexCount_;nativeStats->geometryUniqueVertices=uniqueVertexCount_;
                nativeStats->geometryIndexCount=indexCount_;nativeStats->geometryVertexBytes=geometryVertexBytes_;
                nativeStats->geometryIndexBytes=geometryIndexBytes_;nativeStats->geometryPrepareMs=geometryPrepareMs_;
                nativeStats->geometryUploadMs=geometryUploadMs_;
                nativeStats->gpuBoxInstances=submittedBoxInstances;
                nativeStats->gpuEllipseInstances=submittedEllipseInstances;
                nativeStats->gpuFoliageInstances=submittedFoliageInstances;
                nativeStats->tests=submittedVertices/3+uint64_t(submittedBoxInstances)*12+
                    submittedEllipseTriangles+submittedFoliageTriangles+transparent_.size();
                nativeStats->ms=std::chrono::duration<double,std::milli>(
                    Clock::now()-started).count();
                nativeStats->cpuSubmitMs=nativeStats->ms;
            }
            return true;
        }
        // GPU timestamps surround the actual draw commands and the queued
        // GPU copy. They do not include any CPU-side GetData/Map blocking.
        if(slot.queryReady)context_->End(slot.afterDraw.Get());
        if(multires){
            context_->OMSetRenderTargets(0,nullptr,nullptr);
            context_->CopyResource(slot.staging.Get(),mergeTex_.Get());
        }else{
        // V17 WARP lossless fast-path: no MSAA resolve is needed when
        // sampleCount_ == 1. Copy the render target directly into staging,
        // eliminating an entire full-frame GPU copy. Both textures are
        // DXGI R8G8B8A8 compatible; no shading/LOD/pixel changes.
        // Keep the old two-copy path selectable for strict SHA-256 A/B.
        const char* legacyReadback=std::getenv("M3D_V17_LEGACY_DOUBLE_COPY");
        if(sampleCount_>1){
            if(!atmosphereOn)context_->ResolveSubresource(resolve_.Get(),0,target_.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
            context_->CopyResource(slot.staging.Get(),resolve_.Get());
        }else if(legacyReadback&&std::strcmp(legacyReadback,"1")==0){
            context_->CopyResource(resolve_.Get(),target_.Get());
            context_->CopyResource(slot.staging.Get(),resolve_.Get());
        }else{
            context_->CopyResource(slot.staging.Get(),target_.Get());
        }
        }
        if(slot.queryReady){
            context_->End(slot.afterCopy.Get());
            context_->End(slot.disjoint.Get());
        }
        frameProfile.finish(escProfile);
        slot.submitted={};slot.submitted.escProfile=escProfile;
        slot.submitted.gpu=true;slot.submitted.threads=0;
        slot.submitted.readbackBufferCount=depth;
        slot.submitted.gpuDrawCalls=drawCalls;
        slot.submitted.dxR001Enabled=dxR001;
        slot.submitted.dxLightReferenceCount=dxLightReferenceCount;
        slot.submitted.dxLightSubmittedCount=cb.lightCount;
        slot.submitted.planarReflectionRendered=planarReflection;
        for(size_t i=0;i<2;++i)slot.submitted.zReflectPasses[i]=reflectPasses[i];
        slot.submitted.zReflectActive=uint32_t(zActive_.size());slot.submitted.zReflectCandidates=uint32_t(zStats.candidates);
        slot.submitted.zReflectNodeVisits=uint32_t(zStats.nodeVisits);slot.submitted.zReflectDeferred=zDeferred;
                slot.submitted.zReflectTotal=zTotal;slot.submitted.zReflectSelectUs=zSelectUs;slot.submitted.zReflectSubmitMs=reflectSubmitMs;slot.submitted.zReflectEnabled=zEnabled;
        slot.submitted.lumaProbeLightingEnabled=lumaBvh&&hybrid;
        slot.submitted.lumaAdaptiveEnabled=adaptiveEnabled;
        slot.submitted.lumaAdaptiveZeroTriangles=adaptiveEnabled?lumaTraceCompute_.zeroTriangles():0;
        slot.submitted.lumaAdaptiveTriangleCount=lumaBvh?lumaTraceCompute_.triangleCount():0;
        slot.submitted.geometryPreparationEnabled=staticGeometryPreparationEnabled_;
        slot.submitted.geometrySourceVertices=sourceVertexCount_;slot.submitted.geometryUniqueVertices=uniqueVertexCount_;
        slot.submitted.geometryIndexCount=indexCount_;slot.submitted.geometryVertexBytes=geometryVertexBytes_;
        slot.submitted.geometryIndexBytes=geometryIndexBytes_;slot.submitted.geometryPrepareMs=geometryPrepareMs_;
        slot.submitted.geometryUploadMs=geometryUploadMs_;
        slot.submitted.gpuBoxInstances=submittedBoxInstances;
        slot.submitted.gpuEllipseInstances=submittedEllipseInstances;
        slot.submitted.gpuFoliageInstances=submittedFoliageInstances;
        slot.submitted.tests=submittedVertices/3+uint64_t(submittedBoxInstances)*12
            +submittedEllipseTriangles+submittedFoliageTriangles+transparent_.size();
        slot.copyQueued=Clock::now();
        slot.submitted.cpuSubmitMs=std::chrono::duration<double,std::milli>(
            slot.copyQueued-started).count();
        pending_.push_back(slotIndex);
        lastSubmissionQualityBits_=qualityBits;
        lastSubmissionHybrid_=hybrid;
        lastSubmissionTone_=tone;
        lastSubmissionBalanced_=balanced;
        lastSubmissionExposure_=exposure;
        lastSubmissionMultires_=multires;
        nextReadbackSlot_=(slotIndex+1u)%readbackDepth_;
        return true;
    }
    bool receive(std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
        if(pending_.empty()){error=L"Aucune image GPU en attente.";return false;}
        ReadbackSlot& slot=readbackSlots_[pending_.front()];
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const auto mapStarted=Clock::now();
        const HRESULT hr=context_->Map(slot.staging.Get(),0,D3D11_MAP_READ,0,&mapped);
        if(FAILED(hr)){error=L"Lecture différée DirectX 11 impossible.";return false;}
        const auto mapDone=Clock::now();
        pixels.resize(size_t(slot.width)*size_t(slot.height));
        const char* disableFastSwizzle=std::getenv("M3D_V17_DISABLE_FAST_SWIZZLE");
        const bool fastSwizzle=!(disableFastSwizzle&&std::strcmp(disableFastSwizzle,"1")==0);
        const char* skyCompositor=std::getenv("M3D_V17_SPLIT_SKY");
        const bool preserveAlpha=skyCompositor&&std::strcmp(skyCompositor,"1")==0;
        auto packPixel=[preserveAlpha](uint32_t rgba)->uint32_t {
            uint32_t r=rgba&255u,g=(rgba>>8)&255u,b=(rgba>>16)&255u;
            if(!preserveAlpha)return 0xff000000u|(r<<16)|(g<<8)|b;
            const uint32_t a=(rgba>>24)&255u;
            if(a==0)return 0;
            if(a<255){
                // GPU transparency was blended over transparent black.
                // Convert the premultiplied color to straight ARGB32 for Qt.
                r=std::min(255u,(r*255u+a/2u)/a);
                g=std::min(255u,(g*255u+a/2u)/a);
                b=std::min(255u,(b*255u+a/2u)/a);
            }
            return (a<<24)|(r<<16)|(g<<8)|b;
        };
        for(int y=0;y<slot.height;++y){
            const uint8_t* row=static_cast<const uint8_t*>(mapped.pData)
                +size_t(y)*mapped.RowPitch;
            uint32_t* dest=pixels.data()+size_t(y)*slot.width;
            if(fastSwizzle){
                // Windows x64: R8G8B8A8_UNORM is a little-endian DWORD
                // 0xAABBGGRR. QImage::Format_ARGB32 expects 0xFFRRGGBB.
                // One aligned 32-bit load replaces three byte loads.
                const uint32_t* src=reinterpret_cast<const uint32_t*>(row);
                for(int x=0;x<slot.width;++x){
                    const uint32_t rgba=src[x];
                    dest[x]=packPixel(rgba);
                }
            }else{
                // Unmodified reference path for pixel-exact A/B.
                for(int x=0;x<slot.width;++x){
                    const uint8_t* p=row+size_t(x)*4;
                    const uint32_t rgba=uint32_t(p[0])|(uint32_t(p[1])<<8)
                        |(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);
                    dest[x]=packPixel(rgba);
                }
            }
        }
        context_->Unmap(slot.staging.Get(),0);
        const auto finished=Clock::now();
        stats=slot.submitted;
        stats.ms=std::chrono::duration<double,std::milli>(finished-slot.cpuStarted).count();
        stats.postMs=std::chrono::duration<double,std::milli>(finished-slot.copyQueued).count();
        stats.cpuMapWaitMs=std::chrono::duration<double,std::milli>(mapDone-mapStarted).count();
        stats.cpuPixelConvertMs=std::chrono::duration<double,std::milli>(finished-mapDone).count();
        if(slot.queryReady){
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info{};
            UINT64 begin=0,draw=0,copy=0;
            const bool valid=context_->GetData(slot.disjoint.Get(),&info,sizeof(info),0)==S_OK
                &&context_->GetData(slot.start.Get(),&begin,sizeof(begin),0)==S_OK
                &&context_->GetData(slot.afterDraw.Get(),&draw,sizeof(draw),0)==S_OK
                &&context_->GetData(slot.afterCopy.Get(),&copy,sizeof(copy),0)==S_OK;
            if(valid&&!info.Disjoint&&info.Frequency>0&&begin<=draw&&draw<=copy){
                const double ticksToMs=1000.0/double(info.Frequency);
                stats.gpuDrawMs=double(draw-begin)*ticksToMs;
                stats.gpuTransferMs=double(copy-draw)*ticksToMs;
                stats.gpuTotalMs=double(copy-begin)*ticksToMs;
                stats.gpuTimingValid=true;
            }
        }
        pending_.pop_front();
        return true;
    }
private:
    bool ensureProbeVolume(const Scene& scene,std::wstring& error){
        const uint64_t version=giFileRevision.load(std::memory_order_acquire);
        if(probeSrv_&&loadedGiFileRevision_==version)return true;
        probeSrv_.Reset();probeTexture_.Reset();
        probeNextSrv_.Reset();probeNextTexture_.Reset();
        bakedGi_=gibake::Table{};hasBakedGi_=false;
        lowFrame_=-1;highFrame_=-1;loadedGiFileRevision_=version;
        std::filesystem::path cachePath;
        {std::lock_guard<std::mutex> lock(giPathMutex);cachePath=giFilePath;}
        if(!cachePath.empty()){
            gibake::Table candidate;std::string reason;
            if(gibake::readFile(cachePath,candidate,reason)&&
               candidate.sceneHash==gibake::signature(scene)){
                bakedGi_=std::move(candidate);
                hasBakedGi_=true;
                probeOrigin_=bakedGi_.origin;
                probeInvSpan_={1.f/std::max(.001f,bakedGi_.span.x),
                               1.f/std::max(.001f,bakedGi_.span.y),
                               1.f/std::max(.001f,bakedGi_.span.z)};
                giTableLoaded.store(true,std::memory_order_release);
                return true;
            }
        }
        giTableLoaded.store(false,std::memory_order_release);
        // Compatibility fallback if a scene has not yet been baked. This
        // retains the V17.310 reference path exactly and is NOT real GI.
        // The coarse fallback exposed probe-cell patches on large flat walls.
        // Match the denser offline GI defaults while keeping render cost fixed.
        constexpr UINT nx=24,ny=10,nz=24;
        V lo{INF,INF,INF},hi{-INF,-INF,-INF};
        for(const Obj& o:scene.obj){
            const V a=o.mn,b=o.mx;
            if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)||
               !std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||
               a.x>b.x||a.y>b.y||a.z>b.z)continue;
            lo={std::min(lo.x,a.x),std::min(lo.y,a.y),std::min(lo.z,a.z)};
            hi={std::max(hi.x,b.x),std::max(hi.y,b.y),std::max(hi.z,b.z)};
        }
        if(!std::isfinite(lo.x))lo={-5.f,-2.f,-5.f},hi={5.f,6.f,5.f};
        const V dimensions=hi-lo;
        const V padding={std::max(1.f,dimensions.x*.01f),
                         std::max(1.f,dimensions.y*.03f),
                         std::max(1.f,dimensions.z*.01f)};
        probeOrigin_=lo-padding;
        const V span=hi-lo+padding*2.f;
        probeInvSpan_={1.f/std::max(span.x,.001f),
                       1.f/std::max(span.y,.001f),
                       1.f/std::max(span.z,.001f)};
        const float maxRay=clampf(std::max(span.x,std::max(span.y,span.z))/12.f,6.f,25.f);
        const std::array<V,8> directions={{
            norm({0.f,1.f,0.f}),norm({.6f,.8f,0.f}),
            norm({-.6f,.8f,0.f}),norm({0.f,.8f,.6f}),
            norm({1.f,.2f,0.f}),norm({-1.f,.2f,0.f}),
            norm({0.f,.2f,-1.f}),norm({0.f,-1.f,0.f})
        }};
        auto pigment=[&](int mi)->V{
            if(mi<0||size_t(mi)>=scene.mat.size())return {.52f,.49f,.43f};
            const Material& m=scene.mat[size_t(mi)];
            if(!m.colorMip.empty()&&!m.colorMip.back().p.empty()){
                const uint32_t c=m.colorMip.back().p.front();
                return {float((c>>16)&255)/255.f,float((c>>8)&255)/255.f,float(c&255)/255.f};
            }
            if(!m.mip.empty()&&!m.mip.back().p.empty()){
                const uint16_t c=m.mip.back().p.front();
                return {float(c>>11)/31.f,float((c>>5)&63)/63.f,float(c&31)/31.f};
            }
            return {.52f,.49f,.43f};
        };
        std::vector<float> voxels(size_t(nx)*ny*nz*4u,0.f);
        for(UINT z=0;z<nz;++z)for(UINT y=0;y<ny;++y)for(UINT x=0;x<nx;++x){
            const V origin=probeOrigin_+V{span.x*(float(x)+.5f)/float(nx),
                                           span.y*(float(y)+.5f)/float(ny),
                                           span.z*(float(z)+.5f)/float(nz)};
            V colors{};float reflectWeight=0.f,open=0.f;
            for(size_t d=0;d<directions.size();++d){
                Hit hit;
                closest(scene,Ray{origin,directions[d]},hit,nullptr,false,-1,true);
                if(hit.id<0||hit.t>maxRay){if(d<4)open+=1.f;continue;}
                const float w=(1.f-hit.t/maxRay)*(d==7?1.15f:1.f);
                colors=colors+pigment(hit.mat)*w;
                reflectWeight+=w;
            }
            const V rgb=reflectWeight>.0001f?colors/reflectWeight:V{.38f,.39f,.42f};
            const float impact=clampf(reflectWeight/4.f,0.f,1.f);
            const size_t p=((size_t(z)*ny+y)*nx+x)*4u;
            voxels[p+0]=rgb.x*impact;voxels[p+1]=rgb.y*impact;
            voxels[p+2]=rgb.z*impact;voxels[p+3]=open*.25f;
        }
        D3D11_TEXTURE3D_DESC td{};
        td.Width=nx;td.Height=ny;td.Depth=nz;td.MipLevels=1;
        td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
        td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem=voxels.data();data.SysMemPitch=nx*4u*sizeof(float);
        data.SysMemSlicePitch=nx*ny*4u*sizeof(float);
        if(FAILED(device_->CreateTexture3D(&td,&data,&probeTexture_))||
           FAILED(device_->CreateShaderResourceView(probeTexture_.Get(),nullptr,&probeSrv_))){
            error=L"Échec de création des sondes d'éclairage indirect sur GPU.";
            probeTexture_.Reset();probeSrv_.Reset();return false;
        }
        return true;
    }
    bool ensureGiHours(std::wstring& error){
        if(!hasBakedGi_)return true;
        unsigned lo=0,hi=0;float weight=0.f;
        gibake::hourIndices(bakedGi_,solarHour,lo,hi,weight);
        giHourMix_=weight;
        if(lowFrame_==int(lo)&&highFrame_==int(hi)&&probeSrv_&&probeNextSrv_)
            return true;
        const UINT nx=bakedGi_.nx,ny=bakedGi_.ny,nz=bakedGi_.nz;
        D3D11_TEXTURE3D_DESC td{};
        td.Width=nx;td.Height=ny;td.Depth=nz;td.MipLevels=1;
        td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
        td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        const size_t frameLength=bakedGi_.voxelCount()*4u;
        auto build=[&](unsigned index,ComPtr<ID3D11Texture3D>& texture,
                       ComPtr<ID3D11ShaderResourceView>& srv)->bool{
            texture.Reset();srv.Reset();
            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem=bakedGi_.rgba.data()+frameLength*index;
            data.SysMemPitch=nx*4u*sizeof(float);
            data.SysMemSlicePitch=nx*ny*4u*sizeof(float);
            return SUCCEEDED(device_->CreateTexture3D(&td,&data,&texture))&&
                SUCCEEDED(device_->CreateShaderResourceView(texture.Get(),nullptr,&srv));
        };
        if(!build(lo,probeTexture_,probeSrv_)||
           !build(hi,probeNextTexture_,probeNextSrv_)){
            error=L"Échec des textures de table GI précalculée";
            return false;
        }
        lowFrame_=int(lo);highFrame_=int(hi);
        return true;
    }
    bool ensureShadow(const ShadowMap& shadows,std::wstring& error) {
        if(shadows.size<=0||shadows.d.size()<size_t(shadows.size)*shadows.size){error=L"Carte d’ombres invalide pour DirectX 11.";return false;}
        if(!shadowTexture_||shadowSize_!=shadows.size){
            shadowTexture_.Reset();shadowSrv_.Reset();
            shadowUploadCache_.clear();
            D3D11_TEXTURE2D_DESC td{};td.Width=UINT(shadows.size);td.Height=UINT(shadows.size);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R32_FLOAT;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            if(FAILED(device_->CreateTexture2D(&td,nullptr,&shadowTexture_))||FAILED(device_->CreateShaderResourceView(shadowTexture_.Get(),nullptr,&shadowSrv_))){error=L"Création de la carte d’ombres DirectX 11 impossible.";return false;}
            shadowSize_=shadows.size;
        }
        // Bitwise comparison, not an address or probabilistic hash: in-place
        // edits and allocator address reuse must never yield stale shadows.
        // The transform is uploaded separately through FrameConstants.
        const size_t count=size_t(shadows.size)*shadows.size;
        const char* bypass=std::getenv("M3D_V17_DISABLE_SHADOW_UPLOAD_CACHE");
        const bool cacheEnabled=!(bypass&&std::strcmp(bypass,"1")==0);
        if(cacheEnabled&&shadowUploadCache_.size()==count &&
           std::memcmp(shadowUploadCache_.data(),shadows.d.data(),
                       count*sizeof(float))==0)return true;
        context_->UpdateSubresource(shadowTexture_.Get(),0,nullptr,
                                    shadows.d.data(),UINT(shadows.size*sizeof(float)),0);
        ++shadowRevision_;
        if(cacheEnabled)shadowUploadCache_.assign(shadows.d.begin(),shadows.d.begin()+count);
        else shadowUploadCache_.clear();
        return true;
    }
    bool ensureDevice(std::wstring& error) {
        if(device_)return true;
        D3D_FEATURE_LEVEL feature{};
        // Hardware rendering remains the default. CI can explicitly use the
        // Microsoft WARP software renderer to test this raster backend.
        const char* warp=std::getenv("M3D_V16_FORCE_WARP");
        const D3D_DRIVER_TYPE driver=(warp&&std::strcmp(warp,"1")==0)
            ?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
        HRESULT hr=D3D11CreateDevice(nullptr,driver,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&device_,&feature,&context_);
        if(FAILED(hr)){error=L"Aucun périphérique Direct3D 11 matériel n’est disponible.";return false;}
        // Keep each embedded HLSL literal below MSVC C2026 limits, then compile the unchanged source.
        const std::string shaderSource = std::string(dx11::shaders::raster::kShader) +
            dx11::shaders::raster::kEscargotGpuShader +
            dx11::shaders::raster::kShaderAfterEscargot + dx11::shaders::raster::kShaderPart2;
        ComPtr<ID3DBlob> vsCode,boxVsCode,ellipseVsCode,psCode,errors;
        auto shaderError=[&](const wchar_t* prefix){
            std::wstring result(prefix);
            if(errors&&errors->GetBufferPointer()){
                result+=L": ";
                const char* p=static_cast<const char*>(errors->GetBufferPointer());
                const size_t n=std::min<size_t>(errors->GetBufferSize(),1024);
                for(size_t i=0;i<n;++i)
                    result.push_back(p[i]>=32&&p[i]<127?wchar_t(p[i]):L' ');
            }
            return result;
        };
        hr=D3DCompile(shaderSource.c_str(),shaderSource.size(),"m3d_directx11_preview",nullptr,nullptr,"VSMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du vertex shader DirectX 11 impossible");return false;}
        hr=D3DCompile(shaderSource.c_str(),shaderSource.size(),"m3d_directx11_preview",nullptr,nullptr,"VSBoxMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&boxVsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du shader d'instancing V17 impossible");return false;}
        hr=D3DCompile(shaderSource.c_str(),shaderSource.size(),"m3d_directx11_preview",nullptr,nullptr,
                      "VSEllipseMain","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&ellipseVsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du shader LOD ellipsoïdes impossible");return false;}
        // The exact old shader can be selected to A/B test visual fidelity.
        // HLSL is compiled once at device creation, never per frame.
        const char* disableShaderOpt=std::getenv("M3D_V17_DISABLE_SHADER_OPT");
        const bool optimizePixels=!(disableShaderOpt&&std::strcmp(disableShaderOpt,"1")==0);
        // The cheap three-axis rejection is safe for every local light and is
        // part of the normal path. Keep the old opt-in variable compatible
        // with benchmark launchers; the explicit disable flag wins.
        const char* lightCullEnv=std::getenv("M3D_V17_EXPERIMENT_LIGHT_AABB_CULL");
        const char* disableLightCull=std::getenv("M3D_V17_DISABLE_LIGHT_AABB_CULL");
        const bool lightAabbCull=!(disableLightCull&&std::strcmp(disableLightCull,"1")==0)
            &&!(lightCullEnv&&std::strcmp(lightCullEnv,"0")==0);
        const D3D_SHADER_MACRO pixelMacros[]={
            {"M3D_V17_OPTIMIZE_PIXELS","1"},
            {lightAabbCull?"M3D_V17_LIGHT_AABB_CULL":nullptr,lightAabbCull?"1":nullptr},
            {nullptr,nullptr}
        };
        const D3D_SHADER_MACRO lightCullOnlyMacros[]={
            {"M3D_V17_LIGHT_AABB_CULL","1"},{nullptr,nullptr}
        };
        // Old reference shader, optimized shader, and the experimental
        // conservative local-light rejection are independent A/B paths.
        const D3D_SHADER_MACRO* activeMacros=
            optimizePixels?pixelMacros:(lightAabbCull?lightCullOnlyMacros:nullptr);
        hr=D3DCompile(shaderSource.c_str(),shaderSource.size(),"m3d_directx11_preview",
            activeMacros,nullptr,"PSMain","ps_4_0",
            D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&psCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du pixel shader DirectX 11 impossible");return false;}
        const char* objectEnv=std::getenv("CSE_WARP_ESCARGOT_OBJECT_GPU");
        if(objectEnv&&std::strcmp(objectEnv,"1")==0){
            try{
                const std::string objectSource=std::string("#define CSE_ESCARGOT_RASTER_DIRECT 1\n")+dx11::shaders::raster::kShader+dx11::shaders::raster::kEscargotGpuShader+escargot_gpu::rasterTraceSource()+escargot_gpu::kRasterOptics+dx11::shaders::raster::kShaderAfterEscargot+dx11::shaders::raster::kShaderPart2;
                ComPtr<ID3DBlob> objectCode;errors.Reset();
                HRESULT objectHr=D3DCompile(objectSource.data(),objectSource.size(),"EscargotObjectRaster004",activeMacros,nullptr,"PSMain","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&objectCode,&errors);
                if(FAILED(objectHr))objectShaderError_=shaderError(L"Object GPU pixel shader compilation");
                else if(FAILED(device_->CreatePixelShader(objectCode->GetBufferPointer(),objectCode->GetBufferSize(),nullptr,&objectPs_)))objectShaderError_=L"Object GPU pixel shader creation";
            }catch(const std::exception&){objectShaderError_=L"Object GPU shader source assembly";}
        }
        ComPtr<ID3DBlob> reflectionDepthCode;
        errors.Reset();hr=D3DCompile(shaderSource.c_str(),shaderSource.size(),"m3d_directx11_preview",nullptr,nullptr,"PSReflectionDepth","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&reflectionDepthCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation du Z Prepass miroir impossible");return false;}
        if(FAILED(device_->CreatePixelShader(reflectionDepthCode->GetBufferPointer(),reflectionDepthCode->GetBufferSize(),nullptr,&reflectionDepthPs_))){error=L"Creation du Z Prepass miroir impossible";return false;}
        ComPtr<ID3DBlob> skyVsCode,skyPsCode;
        errors.Reset();
        hr=D3DCompile(dx11::shaders::sky::kCloudSkyShader,sizeof(dx11::shaders::sky::kCloudSkyShader)-1,"CSE_WARP_CloudSky",
            nullptr,nullptr,"VSSky","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&skyVsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation sommet ciel impossible");return false;}
        errors.Reset();
        hr=D3DCompile(dx11::shaders::sky::kCloudSkyShader,sizeof(dx11::shaders::sky::kCloudSkyShader)-1,"CSE_WARP_CloudSky",
            nullptr,nullptr,"PSSky","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&skyPsCode,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation nuages DirectX 11 impossible");return false;}
        if(FAILED(device_->CreateVertexShader(skyVsCode->GetBufferPointer(),
               skyVsCode->GetBufferSize(),nullptr,&skyVs_)) ||
           FAILED(device_->CreatePixelShader(skyPsCode->GetBufferPointer(),
               skyPsCode->GetBufferSize(),nullptr,&skyPs_))){
            error=L"Impossible de créer les shaders de nuages.";return false;
        }
        ComPtr<ID3DBlob> mergeVS,mergePS;
        errors.Reset();hr=D3DCompile(dx11::shaders::composite::kPeripheralComposite,sizeof(dx11::shaders::composite::kPeripheralComposite)-1,
            "v17_multires",nullptr,nullptr,"VSC","vs_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&mergeVS,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation VS multires impossible");return false;}
        errors.Reset();hr=D3DCompile(dx11::shaders::composite::kPeripheralComposite,sizeof(dx11::shaders::composite::kPeripheralComposite)-1,
            "v17_multires",nullptr,nullptr,"PSC","ps_4_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&mergePS,&errors);
        if(FAILED(hr)){error=shaderError(L"Compilation PS multires impossible");return false;}
        if(FAILED(device_->CreateVertexShader(mergeVS->GetBufferPointer(),mergeVS->GetBufferSize(),nullptr,&mergeVs_)) ||
           FAILED(device_->CreatePixelShader(mergePS->GetBufferPointer(),mergePS->GetBufferSize(),nullptr,&mergePs_))){
           error=L"Création shaders multires impossible.";return false;
        }
        if(FAILED(device_->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,&vs_))||FAILED(device_->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,&ps_))){error=L"Création des shaders DirectX 11 impossible.";return false;}
        const D3D11_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertex,p),D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertex,n),D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(GpuVertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},{"MATERIAL",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertex,material),D3D11_INPUT_PER_VERTEX_DATA,0},{"TRIID",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertex,primitiveId),D3D11_INPUT_PER_VERTEX_DATA,0},{"RECEIVERID",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertex,receiverId),D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(device_->CreateInputLayout(input,6,vsCode->GetBufferPointer(),vsCode->GetBufferSize(),&layout_))){error=L"Création du format de sommet DirectX 11 impossible.";return false;}
        const D3D11_INPUT_ELEMENT_DESC lumaInput[]={
            {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(GpuVertexLuma,p),D3D11_INPUT_PER_VERTEX_DATA,0},
            {"NORMAL",0,DXGI_FORMAT_R10G10B10A2_UNORM,0,offsetof(GpuVertexLuma,normalPacked),D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(GpuVertexLuma,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
            {"MATERIAL",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertexLuma,material),D3D11_INPUT_PER_VERTEX_DATA,0},
            {"TRIID",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertexLuma,primitiveId),D3D11_INPUT_PER_VERTEX_DATA,0},
            {"RECEIVERID",0,DXGI_FORMAT_R32_UINT,0,offsetof(GpuVertexLuma,receiverId),D3D11_INPUT_PER_VERTEX_DATA,0}
        };
        if(FAILED(device_->CreateInputLayout(lumaInput,6,vsCode->GetBufferPointer(),vsCode->GetBufferSize(),&lumaLayout_))){
            error=L"Création du format de sommet compact LumaTrace impossible.";return false;
        }
        if(FAILED(device_->CreateVertexShader(boxVsCode->GetBufferPointer(),boxVsCode->GetBufferSize(),nullptr,&boxVs_))){
            error=L"Création du vertex shader d'instancing V17 impossible.";return false;
        }
        const D3D11_INPUT_ELEMENT_DESC boxInput[]={
            input[0],input[1],input[2],input[3],input[4],
            {"BOXMIN",0,DXGI_FORMAT_R32G32B32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXSCALE",0,DXGI_FORMAT_R32_FLOAT,1,12,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXMAX",0,DXGI_FORMAT_R32G32B32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXMAT",0,DXGI_FORMAT_R32_UINT,1,28,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXSIZE",0,DXGI_FORMAT_R32G32B32_FLOAT,1,32,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"BOXUVMODE",0,DXGI_FORMAT_R32_UINT,1,44,D3D11_INPUT_PER_INSTANCE_DATA,1}
        };
        const HRESULT boxLayoutResult=device_->CreateInputLayout(
            boxInput,11,boxVsCode->GetBufferPointer(),boxVsCode->GetBufferSize(),&boxLayout_);
        if(FAILED(boxLayoutResult)){
            error=L"Création du format des instances GPU impossible. HRESULT "
                  +std::to_wstring(uint32_t(boxLayoutResult));return false;
        }
        if(FAILED(device_->CreateVertexShader(ellipseVsCode->GetBufferPointer(),
            ellipseVsCode->GetBufferSize(),nullptr,&ellipseVs_))){
            error=L"Création du shader GPU LOD impossible.";return false;
        }
        const D3D11_INPUT_ELEMENT_DESC ellipseInput[]={
            input[0],input[1],input[2],input[3],input[4],
            {"ELLIPSECENTER",0,DXGI_FORMAT_R32G32B32_FLOAT,1,0,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"ELLIPSEMAT",0,DXGI_FORMAT_R32_UINT,1,12,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"ELLIPSERADII",0,DXGI_FORMAT_R32G32B32_FLOAT,1,16,D3D11_INPUT_PER_INSTANCE_DATA,1},
            {"ELLIPSEPAD",0,DXGI_FORMAT_R32_FLOAT,1,28,D3D11_INPUT_PER_INSTANCE_DATA,1}
        };
        const HRESULT ellipseLayoutResult=device_->CreateInputLayout(
            ellipseInput,9,ellipseVsCode->GetBufferPointer(),
            ellipseVsCode->GetBufferSize(),&ellipseLayout_);
        if(FAILED(ellipseLayoutResult)){
            error=L"Format des instances ellipsoïdes incorrect. HRESULT "
                  +std::to_wstring(uint32_t(ellipseLayoutResult));return false;
        }
        D3D11_BUFFER_DESC cbd{};cbd.ByteWidth=sizeof(FrameConstants);cbd.Usage=D3D11_USAGE_DYNAMIC;cbd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;cbd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(FAILED(device_->CreateBuffer(&cbd,nullptr,&constants_))){error=L"Création du buffer de constantes DirectX 11 impossible.";return false;}
        cbd.ByteWidth=sizeof(CloudConstants);
        if(FAILED(device_->CreateBuffer(&cbd,nullptr,&skyConstants_))){error=L"Création du buffer de nuages impossible.";return false;}
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.MultisampleEnable=TRUE;
        if(FAILED(device_->CreateRasterizerState(&rd,&raster_))){error=L"Création de l’état raster DirectX 11 impossible.";return false;}
        rd.ScissorEnable=TRUE;
        if(FAILED(device_->CreateRasterizerState(&rd,&scissorRaster_))){
            error=L"Scissor multires impossible.";return false;
        }
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        if(FAILED(device_->CreateDepthStencilState(&dd,&depth_))){error=L"Création du depth buffer DirectX 11 impossible.";return false;}
        dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
        if(FAILED(device_->CreateDepthStencilState(&dd,&transparentDepth_))){error=L"Création de l’état de profondeur transparent DirectX 11 impossible.";return false;}
        dd.DepthEnable=FALSE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
        if(FAILED(device_->CreateDepthStencilState(&dd,&skyDepth_))){error=L"Création du ciel sans profondeur impossible.";return false;}
        D3D11_BLEND_DESC blendDesc{};blendDesc.RenderTarget[0].BlendEnable=TRUE;blendDesc.RenderTarget[0].SrcBlend=D3D11_BLEND_SRC_ALPHA;blendDesc.RenderTarget[0].DestBlend=D3D11_BLEND_INV_SRC_ALPHA;blendDesc.RenderTarget[0].BlendOp=D3D11_BLEND_OP_ADD;blendDesc.RenderTarget[0].SrcBlendAlpha=D3D11_BLEND_ONE;blendDesc.RenderTarget[0].DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;blendDesc.RenderTarget[0].BlendOpAlpha=D3D11_BLEND_OP_ADD;blendDesc.RenderTarget[0].RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        if(FAILED(device_->CreateBlendState(&blendDesc,&blend_))){error=L"Création de l’état de transparence DirectX 11 impossible.";return false;}
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_ANISOTROPIC;sd.MaxAnisotropy=16;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device_->CreateSamplerState(&sd,&sampler_))){error=L"Création du sampler DirectX 11 impossible.";return false;}
        sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.MaxAnisotropy=1;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        if(FAILED(device_->CreateSamplerState(&sd,&probeSampler_))){error=L"Création du sampler des sondes indirectes impossible.";return false;}
        return true;
    }
    bool ensureScene(const Scene& scene,std::wstring& error,const LumaTraceBvh4* lumaBvh) {
        const bool lumaTrace=lumaBvh!=nullptr;
        if(scene.mat.empty()||scene.mat.size()>256u){
            error=L"DirectX 11 : nombre de matériaux hors limite (1 à 256).";return false;
        }
        const char* disableInstancing=std::getenv("M3D_V17_DISABLE_GPU_INSTANCING");
        const bool useInstancing=!lumaTrace &&
            !(disableInstancing && std::strcmp(disableInstancing,"1")==0);
        const char* disableBoxes=std::getenv("M3D_V17_DISABLE_BOX_INSTANCING");
        const char* disableEllipses=std::getenv("M3D_V17_DISABLE_ELLIPSE_INSTANCING");
        const bool useBoxInstancing=useInstancing &&
            !(disableBoxes && std::strcmp(disableBoxes,"1")==0);
        const bool useEllipseInstancing=useInstancing &&
            !(disableEllipses && std::strcmp(disableEllipses,"1")==0);
        const char* disableFoliage=std::getenv("M3D_V17_DISABLE_FOLIAGE_INSTANCING");
        const bool useFoliageInstancing=useInstancing &&
            !(disableFoliage && std::strcmp(disableFoliage,"1")==0);
        const char* staticPrepEnv=std::getenv("M3D_V17_STATIC_GEOMETRY_PREP");
        const bool useStaticGeometry=staticPrepEnv&&std::strcmp(staticPrepEnv,"1")==0;
        if(sceneKey_==scene.obj.data()&&objectCount_==scene.obj.size()&&
           (vertices_||boxInstances_||ellipseInstances_) &&
           boxInstancingEnabled_==useBoxInstancing &&
           ellipseInstancingEnabled_==useEllipseInstancing &&
           foliageInstancingEnabled_==useFoliageInstancing &&
           staticGeometryPreparationEnabled_==useStaticGeometry &&
           lumaTraceGeometryEnabled_==lumaTrace &&
           sceneIdentity_==scene.gpuCacheIdentity &&
           sceneRevision_==scene.gpuCacheRevision)return true;
        // An edited scene may retain the same std::vector address: revision
        // tracking is required to refresh its GPU PBR/mesh resources.
        vertices_.Reset();indexBuffer_.Reset();transparentVertices_.Reset();albedo_.Reset();
        verticesSrv_.Reset();indicesSrv_.Reset();
        preservedAlbedo_.Reset();preservedTextureLayers_.fill(-1);
        probeSrv_.Reset();probeTexture_.Reset();
        probeNextSrv_.Reset();probeNextTexture_.Reset();
        hasBakedGi_=false;bakedGi_=gibake::Table{};
        lowFrame_=-1;highFrame_=-1; // new scene/revision = reload GI cache
        boxVertices_.Reset();boxInstances_.Reset();boxInstanceCount_=0;
        roughness_.Reset();normal_.Reset();transparent_.clear();vertexCount_=0;indexCount_=0;
        chunks_.clear();boxChunks_.clear();
        ellipseInstances_.Reset();
        for(auto& buffer:ellipseVertices_)buffer.Reset();
        ellipseVertexCounts_.fill(0);ellipses_.clear();ellipseInstanceCount_=0;
        foliageInstances_.Reset();
        for(auto& buffer:foliageVertices_)buffer.Reset();
        foliageVertexCounts_.fill(0);foliage_.clear();foliageInstanceCount_=0;
        lumaTraceCompute_.reset();escResidentPacket_.clear();escTrace_=escargot_gpu::TraceBatch{};escBuild_=escargot_gpu::PacketBuilder{};objectOptics_=escargot_gpu::RasterOptics{};projectedOptics_.invalidate(); // shaders belong to the device, not the scene
        lumaCpuScene_={};
        if(lumaTrace&&!prepareLumaTraceCpuScene(scene,*lumaBvh,lumaCpuScene_,error))
            return false;
        const uint32_t lumaTriangleCount=lumaTrace?lumaCpuScene_.triangleCount:0;
        zRuntime_.build(scene);
        std::vector<uint32_t> zTag;
        if(lumaTrace&&!scene.opticalPlanes.empty()){
            zTag.assign(scene.obj.size(),zreflect::noOptical);
            for(size_t i=0;i<scene.opticalPlanes.size()&&i<0x7ffffffeu;++i)
                for(uint32_t t:scene.opticalPlanes[i].triangles)
                    if(t<zTag.size())zTag[t]=zreflect::opticalTag|uint32_t(i);
        }
        MeshBuilder mesh;
        std::vector<GpuBoxInstance> boxData;
        // V16 JSON geometry is triangulated for CPU/Compute, but the source
        // builder now retains the exact 12-triangle ranges of unrotated
        // boxes. Skip those triangles only in the raster vertex buffer and
        // submit their identical faces through GPU box instancing instead.
        std::vector<uint8_t> skipSource;
        if((useBoxInstancing && !scene.sourceBoxes.empty()) ||
           (useFoliageInstancing && !scene.sourceFoliage.empty()))
            skipSource.assign(scene.obj.size(),0);
        if(useBoxInstancing && !scene.sourceBoxes.empty()){
            for(const SourceBox& source:scene.sourceBoxes){
                if(source.triangleCount!=12||source.material>=scene.mat.size()||
                   scene.mat[source.material].transmissionPercent>0||
                   size_t(source.firstTriangle)+12>scene.obj.size())continue;
                bool valid=true;
                for(size_t k=source.firstTriangle;k<size_t(source.firstTriangle)+12;++k){
                    const Obj& o=scene.obj[k];
                    if(o.kind!=Kind::Triangle||o.mat!=source.material||skipSource[k]){
                        valid=false;break;
                    }
                }
                if(!valid)continue;
                GpuBoxInstance box{};
                box.lo[0]=source.mn.x;box.lo[1]=source.mn.y;box.lo[2]=source.mn.z;
                box.hi[0]=source.mx.x;box.hi[1]=source.mx.y;box.hi[2]=source.mx.z;
                box.size[0]=source.mx.x-source.mn.x;
                box.size[1]=source.mx.y-source.mn.y;
                box.size[2]=source.mx.z-source.mn.z;
                box.material=source.material;
                // Two original scene artworks are mapped once to their
                // complete faces, including on GPU-instanced source boxes.
                const std::string& sourceName=scene.mat[source.material].name;
                box.uvMode=(sourceName=="cse_energetik_fresco"||
                            sourceName=="cse_personnage_facade")?2:1;
                boxData.push_back(box);
                for(size_t k=source.firstTriangle;k<size_t(source.firstTriangle)+12;++k)
                    skipSource[k]=1;
            }
        }
        if(useFoliageInstancing && !scene.sourceFoliage.empty()){
            for(const SourceFoliage& source:scene.sourceFoliage){
                const size_t begin=source.firstTriangle,end=begin+source.triangleCount;
                if(!source.triangleCount||end>scene.obj.size()||
                    source.material>=scene.mat.size()||
                    scene.mat[source.material].transmissionPercent>0||
                    source.radii.x<=0||source.radii.y<=0||source.radii.z<=0)continue;
                bool valid=true;
                for(size_t k=begin;k<end;++k){
                    if(skipSource[k]||scene.obj[k].kind!=Kind::Triangle||
                       scene.obj[k].mat!=source.material){valid=false;break;}
                }
                if(!valid)continue;
                GpuEllipseInstance canopy{};
                canopy.center[0]=source.center.x;
                canopy.center[1]=source.center.y;
                canopy.center[2]=source.center.z;
                canopy.radii[0]=source.radii.x;
                canopy.radii[1]=source.radii.y;
                canopy.radii[2]=source.radii.z;
                canopy.material=source.material;
                foliage_.push_back(canopy);
                for(size_t k=begin;k<end;++k)skipSource[k]=1;
            }
        }
        for(size_t objectIndex=0;objectIndex<scene.obj.size();++objectIndex){
            if(!skipSource.empty() && skipSource[objectIndex])continue;
            const Obj& o=scene.obj[objectIndex];
            mesh.currentReceiverId=lumaTrace&&scene.escargotReceiverIds.size()==scene.obj.size()?scene.escargotReceiverIds[objectIndex]:0u;
            const uint32_t m=o.mat;
            if(m>=scene.mat.size()){error=L"DirectX 11 : indice de matériau invalide.";return false;}
            const float uvScale=uvScaleForMaterial(scene,m);
            const bool waterSurface=m<scene.mat.size() && (scene.mat[m].waterSurface ||
                (!scene.mat[m].surfaceAuthored && scene.mat[m].name=="cse_pool_water"));
            mesh.transparentMaterial=m<scene.mat.size() && (scene.mat[m].transmissionPercent>0 || waterSurface);
            if(lumaTrace){
                if(o.kind!=Kind::Triangle){
                    error=L"LumaTrace BVH4 : les primitives analytiques ne sont pas encore prises en charge.";
                    return false;
                }
                mesh.currentPrimitiveId=lumaCpuScene_.objectToTriangle[objectIndex];
                if(mesh.transparentMaterial&&!zTag.empty()&&zTag[objectIndex]!=zreflect::noOptical)
                    mesh.currentPrimitiveId=zTag[objectIndex];
            }else mesh.currentPrimitiveId=0;
            if(o.kind==Kind::Triangle)mesh.tri(o.a,o.b,o.c,o.n0,o.n1,o.n2,o.uv0.x,o.uv0.y,o.uv1.x,o.uv1.y,o.uv2.x,o.uv2.y,m);
            else if(o.kind==Kind::Box){
                if(useBoxInstancing && !mesh.transparentMaterial){
                    GpuBoxInstance instance{};
                    instance.lo[0]=o.mn.x;instance.lo[1]=o.mn.y;instance.lo[2]=o.mn.z;
                    instance.hi[0]=o.mx.x;instance.hi[1]=o.mx.y;instance.hi[2]=o.mx.z;
                    instance.uvScale=uvScale;instance.material=m;
                    instance.size[0]=o.mx.x-o.mn.x;
                    instance.size[1]=o.mx.y-o.mn.y;
                    instance.size[2]=o.mx.z-o.mn.z;
                    instance.uvMode=0;
                    boxData.push_back(instance);
                }else mesh.box(o.mn,o.mx,m,uvScale);
            }
            else if(o.kind==Kind::RoundBox)mesh.roundedBox(o.mn,o.mx,o.r,m,uvScale);
            else if(o.kind==Kind::Ellipse){
                if(useEllipseInstancing && !mesh.transparentMaterial
                   && o.b.x>0.f && o.b.y>0.f && o.b.z>0.f){
                    GpuEllipseInstance instance{};
                    instance.center[0]=o.a.x;instance.center[1]=o.a.y;instance.center[2]=o.a.z;
                    instance.radii[0]=o.b.x;instance.radii[1]=o.b.y;instance.radii[2]=o.b.z;
                    instance.material=m;
                    ellipses_.push_back(instance);
                }else mesh.ellipsoid(o.a,o.b,m);
            }
            else if(o.kind==Kind::Capsule)mesh.capsule(o.a,o.b,o.r,m);
            else if(o.kind==Kind::Cone)mesh.frustum(o.a,o.n0,o.len,o.r,o.r1,m);
            mesh.transparentMaterial=false;
            if(mesh.limitExceeded){error=L"Scène trop grande pour l’aperçu DirectX 11 (limite de sécurité mémoire).";return false;}
        }
        if(lumaTrace&&lumaTriangleCount!=mesh.vertices.size()/3){
            error=L"LumaTrace BVH4 : le flux de triangles opaque diverge du tampon GPU.";
            return false;
        }
        // The optional V17 ground is not in Scene::order/BVH; leave it out of
        // LumaTrace's per-triangle radiance lookup while keeping raster output.
        mesh.currentPrimitiveId=lumaTrace?0xffffffffu:0u;mesh.currentReceiverId=0;
        mesh.ground(scene);
        if(mesh.limitExceeded){error=L"Scène trop grande pour l’aperçu DirectX 11 (limite de sécurité mémoire).";return false;}
        if(mesh.vertices.empty()&&mesh.transparent.empty()&&boxData.empty()&&ellipses_.empty()&&foliage_.empty()){
            error=L"La scène ne contient aucune géométrie affichable par DirectX 11.";return false;
        }
        if(!boxData.empty()){
            // Reuse the exact 12-triangle cube winding and face normals of
            // the existing MeshBuilder. One template is shared by all boxes.
            MeshBuilder templateMesh;
            templateMesh.box({0,0,0},{1,1,1},0,1.f);
            if(templateMesh.vertices.size()!=36){
                error=L"Maillage cube d'instancing incorrect.";return false;
            }
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth=UINT(templateMesh.vertices.size()*sizeof(GpuVertex));
            bd.Usage=D3D11_USAGE_IMMUTABLE;
            bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA init{};init.pSysMem=templateMesh.vertices.data();
            if(FAILED(device_->CreateBuffer(&bd,&init,&boxVertices_))){
                error=L"Allocation de la géométrie cube partagée impossible.";return false;
            }
            bd.ByteWidth=UINT(boxData.size()*sizeof(GpuBoxInstance));
            init.pSysMem=boxData.data();
            if(FAILED(device_->CreateBuffer(&bd,&init,&boxInstances_))){
                error=L"Allocation des instances GPU impossible.";return false;
            }
            boxInstanceCount_=UINT(boxData.size());
            constexpr size_t kBoxBatch=128;
            for(size_t first=0;first<boxData.size();first+=kBoxBatch){
                const size_t end=std::min(boxData.size(),first+kBoxBatch);
                DrawChunk batch;
                batch.first=UINT(first);batch.count=UINT(end-first);
                batch.mn={boxData[first].lo[0],boxData[first].lo[1],boxData[first].lo[2]};
                batch.mx={boxData[first].hi[0],boxData[first].hi[1],boxData[first].hi[2]};
                for(size_t i=first+1;i<end;++i){
                    const auto& box=boxData[i];
                    for(int axis=0;axis<3;++axis){
                        float *lo=axis==0?&batch.mn.x:axis==1?&batch.mn.y:&batch.mn.z;
                        float *hi=axis==0?&batch.mx.x:axis==1?&batch.mx.y:&batch.mx.z;
                        *lo=std::min(*lo,box.lo[axis]);
                        *hi=std::max(*hi,box.hi[axis]);
                    }
                }
                boxChunks_.push_back(batch);
            }
        }
        if(!ellipses_.empty()){
            // Low / medium / high: 8×16, 16×32 and 32×64. Shared meshes are
            // authored once per scene; their UVs match legacy ellipsoids.
            const int ringsLevels[]={8,16,32};
            for(int lod=0;lod<3;++lod){
                const int rings=ringsLevels[lod],slices=rings*2;
                MeshBuilder sphere;
                auto point=[&](int y,int x){
                    const float phi=PI*float(y)/rings,theta=2.f*PI*float(x)/slices;
                    return V{std::sin(phi)*std::cos(theta),std::cos(phi),
                             std::sin(phi)*std::sin(theta)};
                };
                for(int y=0;y<rings;++y)for(int x=0;x<slices;++x){
                    const V p00=point(y,x),p01=point(y,x+1);
                    const V p10=point(y+1,x),p11=point(y+1,x+1);
                    const float u0=2.5f*x/slices,u1=2.5f*(x+1)/slices;
                    const float v0=float(y)/rings,v1=float(y+1)/rings;
                    sphere.tri(p00,p10,p11,p00,p10,p11,u0,v0,u0,v1,u1,v1,0);
                    sphere.tri(p00,p11,p01,p00,p11,p01,u0,v0,u1,v1,u1,v0,0);
                }
                D3D11_BUFFER_DESC bd{};
                bd.ByteWidth=UINT(sphere.vertices.size()*sizeof(GpuVertex));
                bd.Usage=D3D11_USAGE_IMMUTABLE;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA init{};init.pSysMem=sphere.vertices.data();
                if(FAILED(device_->CreateBuffer(&bd,&init,&ellipseVertices_[lod]))){
                    error=L"Allocation des niveaux de détail ellipsoïdes impossible.";return false;
                }
                ellipseVertexCounts_[lod]=UINT(sphere.vertices.size());
            }
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth=UINT(ellipses_.size()*sizeof(GpuEllipseInstance));
            bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device_->CreateBuffer(&bd,nullptr,&ellipseInstances_))){
                error=L"Allocation du buffer dynamique des ellipsoïdes impossible.";return false;
            }
            ellipseInstanceCount_=UINT(ellipses_.size());
        }
        if(!foliage_.empty()){
            const int segmentLevels[]={8,12,18};
            const int ringLevels[]={4,6,9};
            const int leafLevels[]={0,12,36};
            for(int lod=0;lod<3;++lod){
                const auto unit=makeFoliageTemplate(
                    segmentLevels[lod],ringLevels[lod],leafLevels[lod]);
                if(unit.empty()){error=L"Le maillage LOD du feuillage est vide.";return false;}
                D3D11_BUFFER_DESC bd{};
                bd.ByteWidth=UINT(unit.size()*sizeof(GpuVertex));
                bd.Usage=D3D11_USAGE_IMMUTABLE;
                bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA init{};init.pSysMem=unit.data();
                if(FAILED(device_->CreateBuffer(&bd,&init,&foliageVertices_[lod]))){
                    error=L"Allocation des maillages LOD du feuillage impossible.";return false;
                }
                foliageVertexCounts_[lod]=UINT(unit.size());
            }
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth=UINT(foliage_.size()*sizeof(GpuEllipseInstance));
            bd.Usage=D3D11_USAGE_DYNAMIC;
            bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            if(FAILED(device_->CreateBuffer(&bd,nullptr,&foliageInstances_))){
                error=L"Allocation des instances de feuillage impossible.";return false;
            }
            foliageInstanceCount_=UINT(foliage_.size());
        }
        const auto prepareStarted=Clock::now();
        auto prepared=static_geometry::prepareStaticMesh(mesh.vertices,useStaticGeometry);
        geometryPrepareMs_=std::chrono::duration<double,std::milli>(Clock::now()-prepareStarted).count();
        if(!prepared.valid){error=L"Préparation de la géométrie statique DirectX 11 invalide.";return false;}
        staticGeometryPreparationEnabled_=useStaticGeometry;
        sourceVertexCount_=prepared.sourceVertexCount;uniqueVertexCount_=prepared.uniqueVertexCount;
        const auto& geometryVertices=useStaticGeometry?prepared.vertices:mesh.vertices;
        if(!geometryVertices.empty()){
            constexpr size_t kChunkIndices=3u*4096u;
            const size_t drawIndexCount=useStaticGeometry?prepared.indices.size():geometryVertices.size();
            const auto vertexAt=[&](size_t i)->const GpuVertex&{
                return useStaticGeometry?prepared.vertices[prepared.indices[i]]:geometryVertices[i];
            };
            for(size_t start=0;start<drawIndexCount;start+=kChunkIndices){
                const size_t end=std::min(start+kChunkIndices,drawIndexCount);
                DrawChunk chunk;chunk.first=UINT(start);chunk.count=UINT(end-start);
                const auto& first=vertexAt(start);
                chunk.mn={first.p[0],first.p[1],first.p[2]};chunk.mx=chunk.mn;
                for(size_t i=start;i<end;++i){
                    const auto& v=vertexAt(i);
                    chunk.mn.x=std::min(chunk.mn.x,v.p[0]);chunk.mn.y=std::min(chunk.mn.y,v.p[1]);chunk.mn.z=std::min(chunk.mn.z,v.p[2]);
                    chunk.mx.x=std::max(chunk.mx.x,v.p[0]);chunk.mx.y=std::max(chunk.mx.y,v.p[1]);chunk.mx.z=std::max(chunk.mx.z,v.p[2]);
                }
                chunks_.push_back(chunk);
            }
            std::vector<GpuVertexLuma> packedLumaVertices;
            const void* geometryData=geometryVertices.data();
            size_t geometryStride=sizeof(GpuVertex);
            if(lumaTrace){
                packedLumaVertices.resize(geometryVertices.size());
                for(size_t i=0;i<geometryVertices.size();++i){
                    const GpuVertex& source=geometryVertices[i];GpuVertexLuma& packed=packedLumaVertices[i];
                    std::copy(std::begin(source.p),std::end(source.p),std::begin(packed.p));
                    packed.normalPacked=packLumaNormal(source.n);
                    std::copy(std::begin(source.uv),std::end(source.uv),std::begin(packed.uv));
                    packed.material=source.material;packed.primitiveId=source.primitiveId;packed.receiverId=source.receiverId;
                }
                geometryData=packedLumaVertices.data();geometryStride=sizeof(GpuVertexLuma);
            }
            const size_t geometryBytes=geometryVertices.size()*geometryStride;
            geometryVertexBytes_=geometryBytes;
            geometryIndexBytes_=useStaticGeometry?prepared.indices.size()*sizeof(uint32_t):0u;
            const auto geometryUploadStarted=Clock::now();
        if(geometryBytes>UINT_MAX || geometryIndexBytes_>UINT_MAX){
            error=L"Géométrie GPU trop volumineuse pour un tampon DirectX 11.";
            return false;
        }
        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth=UINT(geometryBytes);
        bd.Usage=D3D11_USAGE_IMMUTABLE;
        bd.BindFlags=D3D11_BIND_VERTEX_BUFFER|(lumaTrace?D3D11_BIND_SHADER_RESOURCE:0u);
        bd.MiscFlags=lumaTrace?D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS:0u;
        bd.StructureByteStride=0u;
        D3D11_SUBRESOURCE_DATA init{};init.pSysMem=geometryData;
        const HRESULT geometryResult=device_->CreateBuffer(&bd,&init,&vertices_);
        if(FAILED(geometryResult)){
            error=L"Allocation de la géométrie GPU impossible ("+
                std::to_wstring((geometryBytes+1048575u)/1048576u)+
                L" Mio demandés, HRESULT "+
                std::to_wstring(uint32_t(geometryResult))+L").";
            return false;
        }
        vertexCount_=UINT(geometryVertices.size());indexCount_=useStaticGeometry?UINT(prepared.indices.size()):0u;
        if(useStaticGeometry){
            D3D11_BUFFER_DESC indexDesc{};indexDesc.ByteWidth=UINT(prepared.indices.size()*sizeof(uint32_t));
            indexDesc.Usage=D3D11_USAGE_IMMUTABLE;indexDesc.BindFlags=D3D11_BIND_INDEX_BUFFER | (lumaTrace?D3D11_BIND_SHADER_RESOURCE:0u);
            D3D11_SUBRESOURCE_DATA indexInit{};indexInit.pSysMem=prepared.indices.data();
            const HRESULT indexResult=device_->CreateBuffer(&indexDesc,&indexInit,&indexBuffer_);
            if(FAILED(indexResult)){error=L"Allocation du tampon d’indices DirectX 11 impossible.";return false;}
            if(lumaTrace){
                D3D11_SHADER_RESOURCE_VIEW_DESC view{};
                view.Format=DXGI_FORMAT_R32_UINT;
                view.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;
                view.Buffer.NumElements=indexCount_;
                if(FAILED(device_->CreateShaderResourceView(indexBuffer_.Get(),&view,&indicesSrv_))){
                    error=L"Lecture des indices de géométrie impossible pour LumaTrace.";
                    return false;
                }
            }
        }
        geometryUploadMs_=std::chrono::duration<double,std::milli>(Clock::now()-geometryUploadStarted).count();
        if(lumaTrace){
            D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
            sv.Format=DXGI_FORMAT_R32_TYPELESS;
            sv.ViewDimension=D3D11_SRV_DIMENSION_BUFFEREX;
            sv.BufferEx.FirstElement=0;
            sv.BufferEx.NumElements=UINT(geometryBytes/sizeof(uint32_t));
            sv.BufferEx.Flags=D3D11_BUFFEREX_SRV_FLAG_RAW;
            if(FAILED(device_->CreateShaderResourceView(vertices_.Get(),&sv,&verticesSrv_))){
                error=L"Vue brute compute de la géométrie impossible.";
                return false;
            }
        }}
        if(!mesh.transparent.empty()){D3D11_BUFFER_DESC bd{};bd.ByteWidth=UINT(mesh.transparent.size()*3*sizeof(GpuVertex));bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device_->CreateBuffer(&bd,nullptr,&transparentVertices_))){error=L"Allocation du tampon des surfaces transparentes impossible.";return false;}}
        transparent_=std::move(mesh.transparent);
        // Material textures arrive at different sizes (256..2048), but D3D11
        // Texture2DArray allocates EVERY slice at the largest width/height.
        // For 136 materials one 2048px mural used to balloon the three GPU
        // arrays + CPU upload buffers to more than 12 GiB, starving 8 GiB PCs.
        UINT texSize=512;
        for(const Material& material:scene.mat){
            if(!material.colorMip.empty())
                texSize=std::max(texSize,material.colorMip.front().size);
            else if(!material.mip.empty())
                texSize=std::max(texSize,material.mip.front().size);
        }
        const UINT matCount=UINT(std::max<size_t>(1,scene.mat.size()));
        if(matCount>64u){
            // 512 MiB maximum for the *combined* albedo (4 B), normal (4 B)
            // and roughness (1 B) arrays including mipmaps; mirrored staging
            // memory is also bounded. Do not modify GLB triangles, UVs or
            // the original 2048px CPU/scene textures.
            constexpr uint64_t kMaxGpuTextureArrayBytes=512ull*1024ull*1024ull;
            const auto arrayBytes=[matCount](UINT side)->uint64_t {
                uint64_t pixels=0;
                for(UINT level=side;level>0;level>>=1)
                    pixels+=uint64_t(level)*uint64_t(level);
                return pixels*uint64_t(matCount)*9ull;
            };
            while(texSize>256u && arrayBytes(texSize)>kMaxGpuTextureArrayBytes)
                texSize>>=1;
        }
        UINT mipLevels=1;for(UINT size=texSize;size>1;size>>=1)++mipLevels;
        std::vector<std::vector<uint32_t>> mipRgba(size_t(matCount)*mipLevels);
        std::vector<std::vector<uint32_t>> mipNormal(size_t(matCount)*mipLevels);
        std::vector<std::vector<uint8_t>> mipRoughness(size_t(matCount)*mipLevels);
        std::vector<D3D11_SUBRESOURCE_DATA> sub(size_t(matCount)*mipLevels),normalSub(size_t(matCount)*mipLevels),roughSub(size_t(matCount)*mipLevels);
        for(UINT m=0;m<matCount;m++)for(UINT level=0;level<mipLevels;level++){
            const UINT targetSize=std::max(1u,texSize>>level);const size_t subIndex=size_t(m)*mipLevels+level;
            auto& color=mipRgba[subIndex];color.resize(size_t(targetSize)*targetSize);
            auto& normal=mipNormal[subIndex];normal.resize(size_t(targetSize)*targetSize,0xffff8080u);
            auto& rough=mipRoughness[subIndex];rough.resize(size_t(targetSize)*targetSize,184);
            const Material& material=scene.mat[m];
            if(!material.colorMip.empty()){
                const ColorLevel& l=material.colorMip[std::min<size_t>(level,material.colorMip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    const uint32_t c=l.p[size_t(sy)*l.size+sx];const uint32_t r=(c>>16)&255,g=(c>>8)&255,b=c&255;
                    color[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                }
            }else if(!material.mip.empty()){
                const Level& l=material.mip[std::min<size_t>(level,material.mip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    const uint16_t c=l.p[size_t(sy)*l.size+sx];const uint32_t r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
                    color[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                }
            }else std::fill(color.begin(),color.end(),0xff808080u);
            if(!material.normalMip.empty()){
                const Level& l=material.normalMip[std::min<size_t>(level,material.normalMip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    const uint16_t c=l.p[size_t(sy)*l.size+sx];const uint32_t r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
                    normal[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                }
            }
            if(!material.roughnessMip.empty()){
                const Level& l=material.roughnessMip[std::min<size_t>(level,material.roughnessMip.size()-1)];
                if(l.size&&l.p.size()>=size_t(l.size)*l.size)for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                    const UINT sx=std::min(l.size-1,UINT(uint64_t(x)*l.size/targetSize)),sy=std::min(l.size-1,UINT(uint64_t(y)*l.size/targetSize));
                    rough[size_t(y)*targetSize+x]=uint8_t(((l.p[size_t(sy)*l.size+sx]>>11)&31)*255/31);
                }
            }
            sub[subIndex].pSysMem=color.data();sub[subIndex].SysMemPitch=targetSize*4;sub[subIndex].SysMemSlicePitch=targetSize*targetSize*4;
            normalSub[subIndex].pSysMem=normal.data();normalSub[subIndex].SysMemPitch=targetSize*4;normalSub[subIndex].SysMemSlicePitch=targetSize*targetSize*4;
            roughSub[subIndex].pSysMem=rough.data();roughSub[subIndex].SysMemPitch=targetSize;roughSub[subIndex].SysMemSlicePitch=targetSize*targetSize;
        }
        D3D11_TEXTURE2D_DESC td{};td.Width=texSize;td.Height=texSize;td.MipLevels=mipLevels;td.ArraySize=matCount;td.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;ComPtr<ID3D11Texture2D> texture;
        if(FAILED(device_->CreateTexture2D(&td,sub.data(),&texture))){error=L"Chargement des textures dans DirectX 11 impossible.";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;sv.Texture2DArray.MipLevels=mipLevels;sv.Texture2DArray.ArraySize=matCount;
        if(FAILED(device_->CreateShaderResourceView(texture.Get(),&sv,&albedo_))){error=L"Création de la table de textures DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        // SRV format must match the typed normal-map resource. Keeping the
        // previous albedo SRGB view description causes E_INVALIDARG on D3D11.
        sv.Format=td.Format;
        ComPtr<ID3D11Texture2D> normalTexture;
        if(FAILED(device_->CreateTexture2D(&td,normalSub.data(),&normalTexture))){error=L"Chargement des normales dans DirectX 11 impossible.";return false;}
        if(FAILED(device_->CreateShaderResourceView(normalTexture.Get(),&sv,&normal_))){error=L"Création de la table de normales DirectX 11 impossible.";return false;}
        td.Format=DXGI_FORMAT_R8_UNORM;ComPtr<ID3D11Texture2D> roughTexture;
        if(FAILED(device_->CreateTexture2D(&td,roughSub.data(),&roughTexture))){error=L"Chargement des rugosités dans DirectX 11 impossible.";return false;}
        sv.Format=td.Format;
        if(FAILED(device_->CreateShaderResourceView(roughTexture.Get(),&sv,&roughness_))){error=L"Création de la table de rugosités DirectX 11 impossible.";return false;}
        // Keep scene-authored paintings in a small independent texture array.
        // The ordinary atlas can drop to 256px on crowded scenes; this tier
        // preserves requested mural detail without enlarging every material.
        std::vector<uint32_t> preservedIds;
        UINT preservedSize=0;
        for(UINT m=0;m<scene.mat.size()&&m<256;m++){
            const Material& material=scene.mat[m];
            if(!material.preserveTextureDetail)continue;
            if(material.colorMip.empty()||
               material.colorMip.front().size<material.minimumTextureResolution){
                error=L"Protected texture source resolution is unavailable.";return false;
            }
            preservedTextureLayers_[m]=int32_t(preservedIds.size());
            preservedIds.push_back(m);
            preservedSize=std::max(preservedSize,material.colorMip.front().size);
        }
        preservedAlbedo_.Reset();
        if(!preservedIds.empty()){
            if(preservedSize>2048){error=L"Protected texture exceeds 2048px.";return false;}
            uint64_t preservedTexels=0;
            for(UINT level=preservedSize;level>0;level>>=1)
                preservedTexels+=uint64_t(level)*uint64_t(level);
            const uint64_t preservedBytes=preservedTexels*preservedIds.size()*4ull;
            constexpr uint64_t kMaxPreservedAlbedoBytes=256ull*1024ull*1024ull;
            if(preservedBytes>kMaxPreservedAlbedoBytes){
                error=L"Protected textures exceed the 256 MiB GPU budget.";return false;
            }
            UINT preservedMips=1;
            for(UINT level=preservedSize;level>1;level>>=1)++preservedMips;
            std::vector<std::vector<uint32_t>> highPixels(preservedIds.size()*preservedMips);
            std::vector<D3D11_SUBRESOURCE_DATA> highSub(preservedIds.size()*preservedMips);
            for(size_t layer=0;layer<preservedIds.size();layer++){
                const Material& material=scene.mat[preservedIds[layer]];
                for(UINT level=0;level<preservedMips;level++){
                    const UINT targetSize=std::max(1u,preservedSize>>level);
                    const ColorLevel& source=material.colorMip[
                        std::min<size_t>(level,material.colorMip.size()-1)];
                    if(!source.size||source.p.size()<size_t(source.size)*source.size){
                        error=L"Invalid protected texture color mip.";return false;
                    }
                    const size_t atLayer=layer*preservedMips+level;
                    auto& pixels=highPixels[atLayer];
                    pixels.resize(size_t(targetSize)*targetSize);
                    for(UINT y=0;y<targetSize;y++)for(UINT x=0;x<targetSize;x++){
                        const UINT sx=std::min(source.size-1,UINT(uint64_t(x)*source.size/targetSize));
                        const UINT sy=std::min(source.size-1,UINT(uint64_t(y)*source.size/targetSize));
                        const uint32_t color=source.p[size_t(sy)*source.size+sx];
                        const uint32_t r=(color>>16)&255,g=(color>>8)&255,b=color&255;
                        pixels[size_t(y)*targetSize+x]=0xff000000u|(b<<16)|(g<<8)|r;
                    }
                    highSub[atLayer].pSysMem=pixels.data();
                    highSub[atLayer].SysMemPitch=targetSize*4;
                    highSub[atLayer].SysMemSlicePitch=targetSize*targetSize*4;
                }
            }
            D3D11_TEXTURE2D_DESC highDesc{};
            highDesc.Width=preservedSize;highDesc.Height=preservedSize;
            highDesc.MipLevels=preservedMips;highDesc.ArraySize=UINT(preservedIds.size());
            highDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            highDesc.SampleDesc.Count=1;highDesc.Usage=D3D11_USAGE_IMMUTABLE;
            highDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            ComPtr<ID3D11Texture2D> highTexture;
            if(FAILED(device_->CreateTexture2D(&highDesc,highSub.data(),&highTexture))){
                error=L"Failed to create protected artwork texture array.";return false;
            }
            D3D11_SHADER_RESOURCE_VIEW_DESC highView{};
            highView.Format=highDesc.Format;highView.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            highView.Texture2DArray.MipLevels=preservedMips;
            highView.Texture2DArray.ArraySize=UINT(preservedIds.size());
            if(FAILED(device_->CreateShaderResourceView(highTexture.Get(),&highView,&preservedAlbedo_))){
                error=L"Failed to create protected artwork shader view.";return false;
            }
            std::clog<<"[CSE WARP D3D11] Protected artwork tier: "<<preservedIds.size()
                     <<" materials, "<<preservedSize<<"x"<<preservedSize
                     <<", "<<preservedBytes/(1024*1024)<<" MiB\n";
        }
        sceneKey_=scene.obj.data();objectCount_=scene.obj.size();
        sceneIdentity_=scene.gpuCacheIdentity;sceneRevision_=scene.gpuCacheRevision;
        boxInstancingEnabled_=useBoxInstancing;
        ellipseInstancingEnabled_=useEllipseInstancing;
        foliageInstancingEnabled_=useFoliageInstancing;
        lumaTraceGeometryEnabled_=lumaTrace;
        return true;
    }
    bool ensureTargets(int w,int h,unsigned msaa,std::wstring& error) {
        if(w==width_&&h==height_&&target_&&msaaPreference_==msaa)return true;
        if(!pending_.empty()){
            error=L"Vider les images GPU différées avant de changer de résolution.";
            return false;
        }
        for(auto& slot:readbackSlots_)slot=ReadbackSlot{};
        readbackDepth_=0;nextReadbackSlot_=0;
        width_=w;height_=h;msaaPreference_=msaa;
        target_.Reset();resolve_.Reset();rtv_.Reset();depthTexture_.Reset();dsv_.Reset();
        atmosphereDepthSrv_.Reset();atmosphereResolveRtv_.Reset();
        targetSrv_.Reset();resolveSrv_.Reset();lowTex_.Reset();lowRtv_.Reset();
        lowSrv_.Reset();lowDepth_.Reset();lowDsv_.Reset();
        mergeTex_.Reset();mergeRtv_.Reset();lowW_=0;lowH_=0;
        sampleCount_=1;sampleQuality_=0;
        for(UINT candidate:{8u,4u,2u}){
            if(candidate>msaa)continue;
            UINT levels=0,depthLevels=0;
            if(SUCCEEDED(device_->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,candidate,&levels))&&levels>0&&
               SUCCEEDED(device_->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT,candidate,&depthLevels))&&depthLevels>0){
                sampleCount_=candidate;sampleQuality_=std::min(levels,depthLevels)-1;break;
            }
        }
        D3D11_TEXTURE2D_DESC td{};td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;td.SampleDesc.Count=sampleCount_;td.SampleDesc.Quality=sampleQuality_;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&target_))){error=L"Création de la cible image DirectX 11 impossible.";return false;}
        // The render target must be TYPELESS for an sRGB RTV on D3D11/WARP.
        // A typed UNORM texture with an sRGB view fails E_INVALIDARG.
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};rtvDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;rtvDesc.ViewDimension=sampleCount_>1?D3D11_RTV_DIMENSION_TEXTURE2DMS:D3D11_RTV_DIMENSION_TEXTURE2D;
        if(sampleCount_==1)rtvDesc.Texture2D.MipSlice=0;
        if(FAILED(device_->CreateRenderTargetView(target_.Get(),&rtvDesc,&rtv_))){error=L"Création de la cible sRGB DirectX 11 impossible.";return false;}
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        srvDesc.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels=1;
        if(sampleCount_==1&&FAILED(device_->CreateShaderResourceView(target_.Get(),&srvDesc,&targetSrv_))){
            error=L"SRV couleur HD indisponible.";return false;
        }
        td.SampleDesc.Count=1;td.SampleDesc.Quality=0;td.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&resolve_))){error=L"Création de la cible de résolution DirectX 11 impossible.";return false;}
        if(FAILED(device_->CreateShaderResourceView(resolve_.Get(),&srvDesc,&resolveSrv_))){
            error=L"SRV résolution MSAA indisponible.";return false;
        }
        D3D11_RENDER_TARGET_VIEW_DESC fogRtv{};fogRtv.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;fogRtv.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        if(FAILED(device_->CreateRenderTargetView(resolve_.Get(),&fogRtv,&atmosphereResolveRtv_))){error=L"Z Atmosphere: resolve RTV indisponible.";return false;}
        td.Format=DXGI_FORMAT_R24G8_TYPELESS;td.SampleDesc.Count=sampleCount_;td.SampleDesc.Quality=sampleQuality_;td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        D3D11_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;dv.ViewDimension=sampleCount_>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;sv.ViewDimension=sampleCount_>1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D;
        if(sampleCount_==1)sv.Texture2D.MipLevels=1;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&depthTexture_))||FAILED(device_->CreateDepthStencilView(depthTexture_.Get(),&dv,&dsv_))||
           FAILED(device_->CreateShaderResourceView(depthTexture_.Get(),&sv,&atmosphereDepthSrv_))){error=L"Création du tampon de profondeur DirectX 11 impossible.";return false;}
        return true;
    }
    bool ensureZTarget(ZReflectTarget& t,int w,int h,std::wstring& error){
        if(t.rtv&&t.dsv&&t.srv&&t.w==w&&t.h==h)return true;
        ZReflectTarget fresh;
        D3D11_TEXTURE2D_DESC d{};d.Width=UINT(w);d.Height=UINT(h);d.MipLevels=1;d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
        d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        D3D11_RENDER_TARGET_VIEW_DESC r{};r.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;r.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        D3D11_SHADER_RESOURCE_VIEW_DESC s{};s.Format=r.Format;s.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;s.Texture2D.MipLevels=1;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&fresh.tex))||
           FAILED(device_->CreateRenderTargetView(fresh.tex.Get(),&r,&fresh.rtv))||
           FAILED(device_->CreateShaderResourceView(fresh.tex.Get(),&s,&fresh.srv))){
            error=L"Z Reflect: color target allocation failed";return false;}
        d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(device_->CreateTexture2D(&d,nullptr,&fresh.depth))||
           FAILED(device_->CreateDepthStencilView(fresh.depth.Get(),nullptr,&fresh.dsv))){
            error=L"Z Reflect: depth target allocation failed";return false;}
        fresh.w=w;fresh.h=h;t=std::move(fresh);return true;
    }
    bool ensureMultires(int w,int h,std::wstring& error){
        int lw=std::max(1,w/2),lh=std::max(1,h/2);
        if(lowRtv_&&mergeRtv_&&lowW_==lw&&lowH_==lh)return true;
        if(!pending_.empty()){error=L"Vider les images différées avant de changer de mode.";return false;}
        lowTex_.Reset();lowRtv_.Reset();lowSrv_.Reset();lowDepth_.Reset();
        lowDsv_.Reset();mergeTex_.Reset();mergeRtv_.Reset();
        D3D11_TEXTURE2D_DESC td{};td.Width=UINT(lw);td.Height=UINT(lh);
        td.MipLevels=1;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;
        td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&lowTex_))){
            error=L"Texture basse résolution impossible.";return false;
        }
        D3D11_RENDER_TARGET_VIEW_DESC rd{};
        rd.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rd.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        if(FAILED(device_->CreateRenderTargetView(lowTex_.Get(),&rd,&lowRtv_))){
            error=L"RTV basse résolution impossible.";return false;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels=1;
        if(FAILED(device_->CreateShaderResourceView(lowTex_.Get(),&sd,&lowSrv_))){
            error=L"SRV basse résolution impossible.";return false;
        }
        td.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;td.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&lowDepth_)) ||
           FAILED(device_->CreateDepthStencilView(lowDepth_.Get(),nullptr,&lowDsv_))){
            error=L"Profondeur basse résolution impossible.";return false;
        }
        td.Width=UINT(w);td.Height=UINT(h);
        td.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;td.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(FAILED(device_->CreateTexture2D(&td,nullptr,&mergeTex_)) ||
           FAILED(device_->CreateRenderTargetView(mergeTex_.Get(),&rd,&mergeRtv_))){
            error=L"Composition multires impossible.";return false;
        }
        lowW_=lw;lowH_=lh;return true;
    }
    bool ensureAnisotropy(unsigned aniso,std::wstring& error){
        if(sampler_&&samplerAnisotropy_==aniso)return true;
        D3D11_SAMPLER_DESC sd{};
        sd.Filter=aniso>1?D3D11_FILTER_ANISOTROPIC:D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.MaxAnisotropy=aniso;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxLOD=D3D11_FLOAT32_MAX;
        ComPtr<ID3D11SamplerState> selected;
        if(FAILED(device_->CreateSamplerState(&sd,&selected))){
            error=L"Réglage anisotropique DirectX 11 non disponible.";return false;
        }
        sampler_=std::move(selected);
        samplerAnisotropy_=aniso;
        return true;
    }
    bool ensureSwapchain(HWND hwnd,int w,int h,std::wstring& error){
        if(swapchain_ && hwnd==nativeHwnd_ && w==nativeW_ && h==nativeH_)return true;
        releasePresenter();
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        if(FAILED(device_.As(&dxgiDevice))||
           FAILED(dxgiDevice->GetAdapter(&adapter))||
           FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))){
            error=L"DirectX 11 : fabrique DXGI 1.2 indisponible.";return false;
        }
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width=UINT(w);desc.Height=UINT(h);
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Stereo=FALSE;desc.SampleDesc.Count=1;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount=2;desc.Scaling=DXGI_SCALING_STRETCH;
        desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
        const HRESULT hr=factory->CreateSwapChainForHwnd(
            device_.Get(),hwnd,&desc,nullptr,nullptr,&swapchain_);
        if(FAILED(hr)){
            error=L"DirectX 11 : création de la swapchain DXGI impossible.";
            return false;
        }
        factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
        nativeHwnd_=hwnd;nativeW_=w;nativeH_=h;
        return true;
    }
    bool ensureReadback(unsigned depth,int w,int h,std::wstring& error){
        if(depth==readbackDepth_ && readbackSlots_[0].staging)return true;
        if(!pending_.empty()){
            error=L"La profondeur de lecture GPU ne peut changer tant que des images sont en attente.";
            return false;
        }
        for(auto& slot:readbackSlots_)slot=ReadbackSlot{};
        nextReadbackSlot_=0;readbackDepth_=0;
        D3D11_TEXTURE2D_DESC td{};
        td.Width=UINT(w);td.Height=UINT(h);td.MipLevels=1;td.ArraySize=1;
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count=1;td.SampleDesc.Quality=0;
        td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(unsigned i=0;i<depth;++i){
            auto& slot=readbackSlots_[i];
            if(FAILED(device_->CreateTexture2D(&td,nullptr,&slot.staging))){
                error=L"Impossible de créer les tampons de lecture GPU différée.";
                return false;
            }
            slot.width=w;slot.height=h;
            // Timestamp/disjoint queries are optional on older drivers;
            // rendering and byte-identical readback still work without them.
            D3D11_QUERY_DESC qd{};
            qd.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;
            const bool disjointOk=SUCCEEDED(device_->CreateQuery(&qd,&slot.disjoint));
            qd.Query=D3D11_QUERY_TIMESTAMP;
            const bool timestampsOk=disjointOk &&
                SUCCEEDED(device_->CreateQuery(&qd,&slot.start)) &&
                SUCCEEDED(device_->CreateQuery(&qd,&slot.afterDraw)) &&
                SUCCEEDED(device_->CreateQuery(&qd,&slot.afterCopy));
            slot.queryReady=disjointOk&&timestampsOk;
        }
        readbackDepth_=depth;
        return true;
    }
    ComPtr<ID3D11Device> device_;ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain1> swapchain_;
    HWND nativeHwnd_=nullptr;
    int nativeW_=0,nativeH_=0;
    ComPtr<ID3D11VertexShader> vs_,boxVs_,ellipseVs_,mergeVs_,skyVs_;
    ComPtr<ID3D11PixelShader> ps_,mergePs_,skyPs_,reflectionDepthPs_;
    ComPtr<ID3D11InputLayout> layout_,lumaLayout_,boxLayout_,ellipseLayout_;
    ComPtr<ID3D11Buffer> skyConstants_,constants_,vertices_,transparentVertices_,boxVertices_,boxInstances_,ellipseInstances_,foliageInstances_;
    ComPtr<ID3D11ShaderResourceView> verticesSrv_,indicesSrv_;
    std::array<ComPtr<ID3D11Buffer>,3> ellipseVertices_;
    std::array<UINT,3> ellipseVertexCounts_{{0,0,0}};
    std::vector<GpuEllipseInstance> ellipses_;
    UINT ellipseInstanceCount_=0;
    std::array<ComPtr<ID3D11Buffer>,3> foliageVertices_;
    std::array<UINT,3> foliageVertexCounts_{{0,0,0}};
    std::vector<GpuEllipseInstance> foliage_;
    UINT foliageInstanceCount_=0;
    ComPtr<ID3D11RasterizerState> raster_,scissorRaster_;ComPtr<ID3D11DepthStencilState> depth_,transparentDepth_,skyDepth_;ComPtr<ID3D11BlendState> blend_;ComPtr<ID3D11SamplerState> sampler_,probeSampler_;ComPtr<ID3D11ShaderResourceView> albedo_,roughness_,normal_,shadowSrv_,probeSrv_,preservedAlbedo_;ComPtr<ID3D11Texture3D> probeTexture_,probeNextTexture_;
    std::array<int32_t,256> preservedTextureLayers_=[](){
        std::array<int32_t,256> layers{};layers.fill(-1);return layers;
    }();
    ComPtr<ID3D11ShaderResourceView> probeNextSrv_;
    gibake::Table bakedGi_{};bool hasBakedGi_=false;
    uint64_t loadedGiFileRevision_=0;
    int lowFrame_=-1,highFrame_=-1;
    float giHourMix_=0.f;
    V probeOrigin_{},probeInvSpan_{};ComPtr<ID3D11Texture2D> shadowTexture_;int shadowSize_=0;uint64_t shadowRevision_=0;std::vector<float> shadowUploadCache_;std::vector<TransparentTriangle> transparent_;
    ComPtr<ID3D11Texture2D> target_,resolve_,depthTexture_,lowTex_,lowDepth_,mergeTex_;
    zreflect::RuntimeOptics zRuntime_;
    std::vector<zreflect::ActivePlane> zActive_;
    std::array<ZReflectTarget,2> zTargets_;
    ComPtr<ID3D11Texture2D> escColor_,escMeta_;
    ComPtr<ID3D11ShaderResourceView> escColorSrv_,escMetaSrv_;
    int escW_=0,escH_=0;
    ComPtr<ID3D11Buffer> escPacket_;
    ComPtr<ID3D11ShaderResourceView> escPacketSrv_;
    size_t escPacketCapacity_=0;
    std::vector<float> escResidentPacket_;
    escargot_gpu::TraceBatch escTrace_;
    escargot_gpu::RasterOptics objectOptics_;
    escargot_gpu::ProjectedOptics projectedOptics_;
    ComPtr<ID3D11PixelShader> objectPs_;
    std::wstring objectShaderError_;
    escargot_gpu::PacketBuilder escBuild_;
    bool uploadEscargotPacket(const std::vector<float>& data,std::wstring& error,bool reuse=false){
        ID3D11ShaderResourceView* clear=nullptr;context_->PSSetShaderResources(13,1,&clear);
        const size_t count=data.size()/4;
        if(count<2||count>4u*1024u*1024u||data.size()%4){error=L"Invalid Escargot packet";return false;}
        if(reuse&&escPacket_&&escPacketSrv_&&escResidentPacket_.size()==data.size()&&std::memcmp(escResidentPacket_.data(),data.data(),data.size()*sizeof(float))==0)return true;
        escResidentPacket_.clear();
        if(!escPacket_||!escPacketSrv_||count>escPacketCapacity_){
            escPacket_.Reset();escPacketSrv_.Reset();escPacketCapacity_=0;
            size_t capacity=2;while(capacity<count)capacity*=2;
            D3D11_BUFFER_DESC desc{};desc.ByteWidth=UINT(capacity*16);desc.Usage=D3D11_USAGE_DYNAMIC;
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;desc.StructureByteStride=16;
            if(FAILED(device_->CreateBuffer(&desc,nullptr,escPacket_.GetAddressOf()))){error=L"Escargot packet allocation";return false;}
            D3D11_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_UNKNOWN;view.ViewDimension=D3D11_SRV_DIMENSION_BUFFER;view.Buffer.NumElements=UINT(capacity);
            if(FAILED(device_->CreateShaderResourceView(escPacket_.Get(),&view,escPacketSrv_.GetAddressOf()))){error=L"Escargot packet SRV";return false;}
            escPacketCapacity_=capacity;
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if(FAILED(context_->Map(escPacket_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){error=L"Escargot packet map";return false;}
        std::memcpy(mapped.pData,data.data(),data.size()*sizeof(float));context_->Unmap(escPacket_.Get(),0);
        if(reuse)escResidentPacket_=data;return true;
    }
    bool uploadEscargot(int w,int h,const EscargotInput& input,std::wstring& error){
        ID3D11ShaderResourceView* clear[2]={nullptr,nullptr};context_->PSSetShaderResources(11,2,clear);
        if(!escColor_||!escMeta_||!escColorSrv_||!escMetaSrv_||escW_!=w||escH_!=h){
            escColor_.Reset();escMeta_.Reset();escColorSrv_.Reset();escMetaSrv_.Reset();
            D3D11_TEXTURE2D_DESC d{};d.Width=UINT(w);d.Height=UINT(h);d.MipLevels=1;d.ArraySize=1;
            d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            if(FAILED(device_->CreateTexture2D(&d,nullptr,escColor_.GetAddressOf()))||
               FAILED(device_->CreateTexture2D(&d,nullptr,escMeta_.GetAddressOf()))||
               FAILED(device_->CreateShaderResourceView(escColor_.Get(),nullptr,escColorSrv_.GetAddressOf()))||
               FAILED(device_->CreateShaderResourceView(escMeta_.Get(),nullptr,escMetaSrv_.GetAddressOf()))){error=L"Escargot texture allocation failed";return false;}
            escW_=w;escH_=h;
        }
        context_->UpdateSubresource(escColor_.Get(),0,nullptr,input.color->data(),UINT(w*16),0);
        context_->UpdateSubresource(escMeta_.Get(),0,nullptr,input.meta->data(),UINT(w*16),0);
        return true;
    }
    ComPtr<ID3D11ShaderResourceView> targetSrv_,resolveSrv_,lowSrv_,atmosphereDepthSrv_;
    ComPtr<ID3D11RenderTargetView> atmosphereResolveRtv_;
    dx11::AtmospherePass atmospherePass_;
    ComPtr<ID3D11RenderTargetView> lowRtv_,mergeRtv_;
    ComPtr<ID3D11DepthStencilView> lowDsv_;
    int lowW_=0,lowH_=0;
    std::array<ReadbackSlot,3> readbackSlots_;
    std::deque<unsigned> pending_;
    unsigned readbackDepth_=0,nextReadbackSlot_=0;
    uint64_t lastSubmissionQualityBits_=defaultQualityBits;
    bool lastSubmissionHybrid_=false,lastSubmissionTone_=false,lastSubmissionBalanced_=false,lastSubmissionMultires_=false;
    float lastSubmissionExposure_=1.f;
    ComPtr<ID3D11RenderTargetView> rtv_;ComPtr<ID3D11DepthStencilView> dsv_;
    std::vector<DrawChunk> chunks_,boxChunks_;
    std::vector<uint8_t> dxChunkMask_,dxBoxMask_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    UINT indexCount_=0,boxInstanceCount_=0;
    bool staticGeometryPreparationEnabled_=false;
    double geometryPrepareMs_=0.,geometryUploadMs_=0.;
    size_t sourceVertexCount_=0,uniqueVertexCount_=0;
    uint64_t geometryVertexBytes_=0,geometryIndexBytes_=0;
    unsigned msaaPreference_=0,samplerAnisotropy_=16;
    bool boxInstancingEnabled_=false,ellipseInstancingEnabled_=false,foliageInstancingEnabled_=false,lumaTraceGeometryEnabled_=false;
    LumaTraceCpuScene lumaCpuScene_;
    lumatrace_gpu::Compute lumaTraceCompute_;
    const Obj* sceneKey_=nullptr;size_t objectCount_=0;uint64_t sceneIdentity_=0,sceneRevision_=0;UINT vertexCount_=0;UINT sampleCount_=1,sampleQuality_=0;int width_=0,height_=0;
};
}
RasterQualitySettings rasterQualityProfile(RasterQualityProfile preset){
    RasterQualitySettings q;
    switch(preset){
    case RasterQualityProfile::Performance:
        q.msaaSamples=1;q.anisotropy=2;q.lodQualityPercent=60;break;
    case RasterQualityProfile::Balanced:
        q.msaaSamples=2;q.anisotropy=8;q.lodQualityPercent=85;break;
    case RasterQualityProfile::High:
        q.msaaSamples=4;q.anisotropy=16;q.lodQualityPercent=100;break;
    case RasterQualityProfile::Ultra:
        q.msaaSamples=8;q.anisotropy=16;q.lodQualityPercent=200;
        q.dynamicLod=false;break;
    }
    return q;
}
RasterQualitySettings rasterQualitySettings(){
    return unpackQuality(gpuQualityBits.load(std::memory_order_acquire));
}
bool rasterHybridLightingEnabled(){return hybridLighting.load(std::memory_order_acquire);}
void setRasterHybridLightingEnabled(bool enabled){hybridLighting.store(enabled,std::memory_order_release);}
bool rasterFilmicTonemapEnabled(){return filmicTonemap.load(std::memory_order_acquire);}
void setRasterFilmicTonemapEnabled(bool enabled){filmicTonemap.store(enabled,std::memory_order_release);}
bool rasterColorEnergyBalanceEnabled(){return colorEnergyBalance.load(std::memory_order_acquire);}
void setRasterColorEnergyBalanceEnabled(bool enabled){colorEnergyBalance.store(enabled,std::memory_order_release);}
float rasterColorExposure(){return colorExposure.load(std::memory_order_acquire);}
void setRasterColorExposure(float x){colorExposure.store(clampf(x,.65f,1.45f),std::memory_order_release);}
void setRasterGiCachePath(const std::filesystem::path& file){
    {std::lock_guard<std::mutex> lock(giPathMutex);giFilePath=file;}
    giFileRevision.fetch_add(1,std::memory_order_acq_rel);
    giTableLoaded.store(false,std::memory_order_release);
}
bool rasterGiCacheLoaded(){return giTableLoaded.load(std::memory_order_acquire);}
std::filesystem::path rasterGiCachePath(){
    std::lock_guard<std::mutex> lock(giPathMutex);
    return giFilePath;
}
void setRasterPeripheralMultiresOverride(int mode){
    peripheralOverride.store(std::clamp(mode,-1,1),std::memory_order_release);
}
void setRasterCloudTimeScale(float x){
    cloudTimeScale.store(std::clamp(x,0.f,480.f),std::memory_order_release);
}
void setRasterCloudsEnabled(bool enabled){rasterCloudsEnabled.store(enabled,std::memory_order_release);}
bool setRasterQualitySettings(const RasterQualitySettings& q,std::wstring& error){
    const auto validSample=q.msaaSamples==1||q.msaaSamples==2||q.msaaSamples==4||q.msaaSamples==8;
    const auto validAniso=q.anisotropy==1||q.anisotropy==2||
        q.anisotropy==4||q.anisotropy==8||q.anisotropy==16;
    if(!validSample||!validAniso||q.lodQualityPercent<50||q.lodQualityPercent>200){
        error=L"Réglages DX11 invalides : MSAA 1/2/4/8, anisotropie 1/2/4/8/16, LOD 50..200%.";
        return false;
    }
    gpuQualityBits.store(packQuality(q),std::memory_order_release);
    return true;
}
static DirectX11Preview& rasterThreadContext(){
    thread_local DirectX11Preview renderer;
    return renderer;
}
bool renderDirectX11Preview(const Scene& scene,const ShadowMap& shadows,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error) {
    return rasterThreadContext().render(scene,shadows,cam,w,h,pixels,stats,error);
}
bool renderLumaTracePreview(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
    return rasterThreadContext().renderLumaTrace(scene,bvh,shadows,cam,w,h,pixels,stats,error);
}
bool renderLumaTraceEscargotPreview(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,
    const std::vector<float>& color,const std::vector<float>& metadata,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
    if(w<=0||h<=0||w>4096||h>4096||color.size()!=size_t(w)*h*4||metadata.size()!=color.size()||escargotInput){error=L"Invalid Escargot buffers";return false;}
    const EscargotInput input{&color,&metadata,w,h};
    struct Reset {~Reset(){escargotInput=nullptr;}} reset;
    escargotInput=&input;
    return rasterThreadContext().renderLumaTrace(scene,bvh,shadows,cam,w,h,pixels,stats,error);
}
bool renderLumaTraceEscargotGpuPreview(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,
    const std::vector<float>& packet,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
    if(w<=0||h<=0||w>4096||h>4096||packet.size()<8||packet.size()%4||escargotInput){error=L"Invalid Escargot GPU packet";return false;}
    const EscargotInput input{nullptr,nullptr,w,h,&packet};
    struct Reset {~Reset(){escargotInput=nullptr;}} reset;
    escargotInput=&input;
    return rasterThreadContext().renderLumaTrace(scene,bvh,shadows,cam,w,h,pixels,stats,error);
}
bool traceEscargotGpuPaths(const Scene& scene,const LumaTraceBvh4& bvh,const std::vector<EscargotPathQuery>& q,std::vector<EscargotPathEvent>& events,EscargotPathTiming& timing,std::wstring& error){
    return rasterThreadContext().traceEscargotPaths(scene,bvh,q,events,timing,error);
}
bool traceEscargotGpuBatch(const Scene& scene,const LumaTraceBvh4& bvh,const std::vector<EscargotRayQuery>& q,std::vector<EscargotRayHit>& hits,double& preparation,double& execution,std::wstring& error){
    return rasterThreadContext().traceEscargot(scene,bvh,q,hits,preparation,execution,error);
}
bool renderLumaTraceEscargotGpuBuiltPreview(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,
    const EscargotPacketBuild& plan,std::vector<uint32_t>& pixels,Stats& stats,EscargotBuildTiming& timing,std::wstring& error){
    if(w<=0||h<=0||w>4096||h>4096||escargotInput){error=L"Invalid Escargot GPU build dimensions";return false;}
    const EscargotInput input{nullptr,nullptr,w,h,nullptr,false,&plan,&timing};
    struct Reset {~Reset(){escargotInput=nullptr;}} reset;escargotInput=&input;
    return rasterThreadContext().renderBuiltEscargot(scene,bvh,shadows,cam,w,h,pixels,stats,timing,error);
}
bool renderLumaTraceEscargotGpuBuiltNative(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,void* hwnd,
    const EscargotPacketBuild& plan,Stats& stats,EscargotBuildTiming& timing,std::wstring& error){
    if(w<=0||h<=0||w>4096||h>4096||!hwnd||escargotInput||plan.capturePacket){error=L"Invalid native Escargot GPU build request";return false;}
    const EscargotInput input{nullptr,nullptr,w,h,nullptr,false,&plan,&timing};
    struct Reset {~Reset(){escargotInput=nullptr;}} reset;escargotInput=&input;
    return rasterThreadContext().present(scene,shadows,cam,w,h,reinterpret_cast<HWND>(hwnd),stats,error,&bvh);
}
bool renderLumaTraceEscargotGpuResidentPreview(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,int w,int h,
    const std::vector<float>& packet,std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
    if(w<=0||h<=0||w>4096||h>4096||packet.size()<8||packet.size()%4||escargotInput){error=L"Invalid resident Escargot packet";return false;}
    const EscargotInput input{nullptr,nullptr,w,h,&packet,true};
    struct Reset {~Reset(){escargotInput=nullptr;}} reset;
    escargotInput=&input;
    return rasterThreadContext().renderLumaTrace(scene,bvh,shadows,cam,w,h,pixels,stats,error);
}
bool renderDirectX11Native(const Scene& scene,const ShadowMap& shadows,const Camera& cam,
                            int w,int h,void* hwnd,Stats& stats,std::wstring& error){
    return rasterThreadContext().present(scene,shadows,cam,w,h,
                                         reinterpret_cast<HWND>(hwnd),stats,error);
}
bool renderLumaTraceNative(const Scene& scene,const LumaTraceBvh4& bvh,const ShadowMap& shadows,const Camera& cam,
                            int w,int h,void* hwnd,Stats& stats,std::wstring& error){
    return rasterThreadContext().present(scene,shadows,cam,w,h,
        reinterpret_cast<HWND>(hwnd),stats,error,&bvh);
}
void releaseDirectX11NativeForCurrentThread(){
    rasterThreadContext().releasePresenter();
}
bool enqueueDirectX11Preview(const Scene& scene,const ShadowMap& shadows,const Camera& cam,
                            int w,int h,unsigned depth,std::wstring& error){
    return rasterThreadContext().enqueue(scene,shadows,cam,w,h,depth,error);
}
bool receiveDirectX11Preview(std::vector<uint32_t>& pixels,Stats& stats,std::wstring& error){
    return rasterThreadContext().receive(pixels,stats,error);
}
}
#else
namespace m3d {
RasterQualitySettings rasterQualityProfile(RasterQualityProfile p){
    RasterQualitySettings q;
    if(p==RasterQualityProfile::Performance){q.msaaSamples=1;q.anisotropy=2;q.lodQualityPercent=60;}
    else if(p==RasterQualityProfile::Balanced){q.msaaSamples=2;q.anisotropy=8;q.lodQualityPercent=85;}
    else if(p==RasterQualityProfile::High){q.lodQualityPercent=100;}
    else if(p==RasterQualityProfile::Ultra){q.msaaSamples=8;q.lodQualityPercent=200;q.dynamicLod=false;}
    return q;
}
RasterQualitySettings rasterQualitySettings(){return RasterQualitySettings{};}
bool rasterHybridLightingEnabled(){return false;}
void setRasterHybridLightingEnabled(bool){}
bool rasterFilmicTonemapEnabled(){return false;}
void setRasterFilmicTonemapEnabled(bool){}
bool rasterColorEnergyBalanceEnabled(){return false;}
void setRasterColorEnergyBalanceEnabled(bool){}
float rasterColorExposure(){return 1.f;}
void setRasterColorExposure(float){}
void setRasterGiCachePath(const std::filesystem::path&){}
bool rasterGiCacheLoaded(){return false;}
std::filesystem::path rasterGiCachePath(){return {};}
void setRasterPeripheralMultiresOverride(int){}
void setRasterCloudTimeScale(float){}
void setRasterCloudsEnabled(bool){}
bool setRasterQualitySettings(const RasterQualitySettings&,std::wstring& error){error=L"DirectX 11 indisponible sur cet OS.";return false;}
bool renderDirectX11Preview(const Scene&,const ShadowMap&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error) { error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false; }
bool renderDirectX11Native(const Scene&,const ShadowMap&,const Camera&,int,int,void*,Stats&,std::wstring& error){error=L"Présentation DXGI disponible uniquement sous Windows.";return false;}
bool renderLumaTracePreview(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"LumaTrace DX11 disponible uniquement sous Windows.";return false;}
bool renderLumaTraceNative(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,void*,Stats&,std::wstring& error){error=L"LumaTrace DX11 natif disponible uniquement sous Windows.";return false;}
bool renderLumaTraceEscargotPreview(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,const std::vector<float>&,const std::vector<float>&,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"Escargot DX11 Windows only";return false;}
bool renderLumaTraceEscargotGpuPreview(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,const std::vector<float>&,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"Escargot GPU DX11 Windows only";return false;}
bool traceEscargotGpuPaths(const Scene&,const LumaTraceBvh4&,const std::vector<EscargotPathQuery>&,std::vector<EscargotPathEvent>&,EscargotPathTiming&,std::wstring& error){error=L"Escargot GPU paths Windows only";return false;}
bool traceEscargotGpuBatch(const Scene&,const LumaTraceBvh4&,const std::vector<EscargotRayQuery>&,std::vector<EscargotRayHit>&,double&,double&,std::wstring& error){error=L"Escargot GPU trace Windows only";return false;}
bool renderLumaTraceEscargotGpuBuiltPreview(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,const EscargotPacketBuild&,std::vector<uint32_t>&,Stats&,EscargotBuildTiming&,std::wstring& error){error=L"Escargot GPU packet build Windows only";return false;}
bool renderLumaTraceEscargotGpuBuiltNative(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,void*,const EscargotPacketBuild&,Stats&,EscargotBuildTiming&,std::wstring& error){error=L"Escargot native GPU build Windows only";return false;}
bool renderLumaTraceEscargotGpuResidentPreview(const Scene&,const LumaTraceBvh4&,const ShadowMap&,const Camera&,int,int,const std::vector<float>&,std::vector<uint32_t>&,Stats&,std::wstring& error){error=L"Escargot GPU resident Windows only";return false;}
void releaseDirectX11NativeForCurrentThread(){}
bool enqueueDirectX11Preview(const Scene&,const ShadowMap&,const Camera&,int,int,unsigned,std::wstring& error) {error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false;}
bool receiveDirectX11Preview(std::vector<uint32_t>&,Stats&,std::wstring& error) {error=L"Le rendu DirectX 11 est disponible uniquement sous Windows.";return false;}
}
#endif
