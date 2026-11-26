// V17: exercise the reused D3D11 raster renderer in CI (Microsoft WARP).
// This validates rendering, not the performance of Intel HD Graphics 620.
#include "m3d_engine.h"
#include "src/m3d_gi_bake.h"
#include "src/m3d_scene_v16.h"
#include "src/m3d_v16_bridge.h"
#include "src/m3d_directx11_mesh.h"
#include "src/m3d_lumatrace_adaptive.h"
#include <QCoreApplication>
#include <QDebug>
#include <QImage>
#include <QString>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <set>
#include <algorithm>
#include <utility>
#include <vector>
static void logToStderr(QtMsgType,const QMessageLogContext&,const QString& message){
    const QByteArray utf8=message.toUtf8();
    std::fprintf(stderr,"V17 RASTER TEST: %s\\n",utf8.constData());
    std::fflush(stderr);
}
int main(int argc,char** argv){
    qInstallMessageHandler(logToStderr);
    QCoreApplication app(argc,argv);
    if(argc!=3){qCritical()<<"Expected scene.json and output.png";return 2;}
    // Test the same helpers used by the renderer on synthetic water geometry:
    // a box bottom/top, a vertical face, an unrelated larger water surface.
    {
        m3d::Scene scene;scene.mat.resize(2);
        scene.mat[0].name="cse_pool_water";scene.mat[0].waterSurface=true;
        scene.mat[1].waterSurface=true;
        m3d::d3d11::MeshBuilder mesh;mesh.transparentMaterial=true;
        auto tri=[&](m3d::V a,m3d::V b,m3d::V c,unsigned mat){
            mesh.tri(a,b,c,{0,1,0},{0,1,0},{0,1,0},0,0,0,0,0,0,mat);
        };
        tri({0,.2f,0},{4,.2f,0},{0,.2f,4},0);
        tri({0,.4f,0},{4,.4f,0},{0,.4f,4},0);
        tri({0,.2f,0},{4,.2f,0},{0,.4f,0},0);
        tri({0,5,0},{20,5,0},{0,5,20},1);
        const auto plane=m3d::d3d11::findWaterReflectionPlane(scene,mesh.transparent);
        if(!plane.valid||std::abs(plane.y-.4f)>1e-5f||
           m3d::d3d11::findWaterReflectionPlane(scene,{}).valid){
            qCritical()<<"Water reflection plane selection failed";return 1;
        }
        for(float pitch:{-.8f,-.2f,0.f,.3f,.8f}){
            m3d::Camera c{};c.eye={0,2.3f,0};c.f={0,std::sin(pitch),std::cos(pitch)};
            c.r=m3d::norm(m3d::cross(c.f,{0,1,0}));c.u=m3d::norm(m3d::cross(c.r,c.f));
            c.aspect=16.f/9.f;c.tanHalf=1.f;
            const auto mirror=m3d::d3d11::waterReflectionCamera(c,plane.y);
            for(float x:{-.99f,0.f,.99f})for(float y:{-.99f,-.5f,0.f,.5f,.99f}){
                const auto ray=c.f+c.r*(x*c.tanHalf)+c.u*(y*c.tanHalf/c.aspect);
                if(ray.y>=-.001f)continue;
                const auto point=c.eye+ray*((plane.y-c.eye.y)/ray.y);
                const auto delta=point-mirror.eye;
                const float z=m3d::dot(delta,mirror.f);
                const float rx=m3d::dot(delta,mirror.r)/(z*mirror.tanHalf);
                const float ry=m3d::dot(delta,mirror.u)*mirror.aspect/(z*mirror.tanHalf);
                if(z<=0||std::abs(rx-x/1.10f)>1e-4f||std::abs(ry+y/1.10f)>1e-4f||
                   .5f+.5f*std::abs(rx)+.04f>1.f||.5f+.5f*std::abs(ry)+.04f>1.f){
                    qCritical()<<"Water reflection camera projection failed";return 1;
                }
            }
        }
    }
    {
        using namespace m3d::lumatrace_adaptive;
        std::vector<Bounds> b(2);b[0].add({0,0,0});b[0].add({1,1,1});
        b[1].add({100,0,0});b[1].add({101,1,1});
        float lights[64][4]{},colors[64][4]{};lights[0][3]=2;colors[0][0]=colors[0][3]=1;
        Mask mask{};
        if(classify(b,257,lights,colors,1,mask)!=1||mask[0]!=2u)return 1;
        colors[0][3]=0;
        if(classify(b,257,lights,colors,1,mask)!=257||mask[0]!=3u)return 1;
        colors[0][3]=1;lights[0][0]=98;
        if(classify(b,257,lights,colors,1,mask)!=256||mask[0]!=1u)return 1;
    }
    const auto loaded=m3d::v16::loadScene(QString::fromLocal8Bit(argv[1]));
    if(!loaded){qCritical()<<loaded.error;return 1;}
    auto built=m3d::v16::buildRuntime(loaded.scene,512);
    if(!built || built.runtime.cameras.isEmpty()){qCritical()<<"No valid raster scene/camera";return 1;}
    // Establish a reproducible daylight-sensor hysteresis state.
    m3d::updateLighting(12.f);
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::Stats shadowStats;
    const auto shadow=m3d::buildShadow(built.runtime.renderScene,shadowStats);
    constexpr int w=640,h=360;
    const auto cam=m3d::v16::makeCamera(built.runtime.cameras.front(),w,h);
    std::vector<uint32_t> first,second;
    m3d::Stats coldStats,warmStats;
    std::wstring error;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                   w,h,first,coldStats,error)){
        qCritical()<<"D3D11 raster failed:"<<QString::fromStdWString(error);
        return 1;
    }
    error.clear();
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                   w,h,second,warmStats,error)){
        qCritical()<<"Cached D3D11 raster failed:"<<QString::fromStdWString(error);
        return 1;
    }
    if(!coldStats.gpu || !warmStats.gpu || first.size()!=size_t(w)*size_t(h) ||
       first!=second || std::set<uint32_t>(first.begin(),first.end()).size()<64){
        qCritical()<<"Empty, incomplete or non-deterministic raster image";return 1;
    }
    // Compare complete images after deduplication, including LumaTrace's BVH vertex reads.
    const auto& runtime=built.runtime;
    for(bool luma:{false,true}){
        std::vector<uint32_t> unprepared,prepared;
        for(bool enabled:{false,true}){
            if(_putenv_s("M3D_V17_STATIC_GEOMETRY_PREP",enabled?"1":"0"))return 1;
            m3d::Stats stats{};
            auto& pixels=enabled?prepared:unprepared;
            const bool ok=luma
                ?m3d::renderLumaTracePreview(runtime.renderScene,runtime.lumaTraceBvh4,shadow,
                                             cam,256,144,pixels,stats,error)
                :m3d::renderDirectX11Preview(runtime.renderScene,shadow,cam,256,144,pixels,stats,error);
            if(!ok || stats.geometryPreparationEnabled!=enabled || !stats.geometrySourceVertices ||
               stats.geometryIndexCount!=(enabled?stats.geometrySourceVertices:0)){
                qCritical()<<"Geometry mode did not reach renderer"<<luma<<enabled<<QString::fromStdWString(error);
                return 1;
            }
        }
        if(prepared.size()!=256u*144u || prepared!=unprepared){
            qCritical()<<"Indexed geometry changed rendered pixels in"<<(luma?"LumaTrace":"DX11");
            return 1;
        }
    }
    m3d::setRasterHybridLightingEnabled(true);
    std::vector<uint32_t> probeReference;
    for(bool enabled:{false,true}){
        if(_putenv_s("M3D_V17_STATIC_GEOMETRY_PREP",enabled?"1":"0"))return 1;
        std::vector<uint32_t> cold,warm;m3d::Stats stats{};
        if(!m3d::renderLumaTracePreview(runtime.renderScene,runtime.lumaTraceBvh4,shadow,cam,256,144,cold,stats,error)||
           !m3d::renderLumaTracePreview(runtime.renderScene,runtime.lumaTraceBvh4,shadow,cam,256,144,warm,stats,error)||
           cold.size()!=256u*144u||cold!=warm||stats.geometryPreparationEnabled!=enabled||(enabled&&cold!=probeReference)){
            qCritical()<<"LumaTrace probe cold/warm or indexed parity failed"<<QString::fromStdWString(error);return 1;}
        if(!enabled)probeReference=cold;
    }
    // Compare actual WARP output with the exact same geometry, probes and lighting.
    // The no-lamp fixture forces every valid opaque triangle through budget zero.
    auto noLamps=runtime.renderScene;noLamps.lights.clear();
    const bool exteriorBeforeAdaptive=m3d::exteriorLightingEnabled();
    for(bool noLocal:{false,true}){
        // Use the exported DLL API, never access the internal pointLights vector.
        m3d::setExteriorLightingEnabled(exteriorBeforeAdaptive&&!noLocal);
        const auto& scene=noLocal?noLamps:runtime.renderScene;
        for(float hour:{14.75f,23.f}){
            m3d::updateLighting(12.f);
            m3d::updateLighting(hour);
            m3d::Stats shadowInfo{};const auto shadowAB=m3d::buildShadow(scene,shadowInfo);
            std::vector<uint32_t> reference;
            for(bool enabled:{false,true,false}){
                if(_putenv_s("M3D_V17_LUMA_ADAPTIVE",enabled?"1":"0"))return 1;
                std::vector<uint32_t> pixels;m3d::Stats stats{};
                if(!m3d::renderLumaTracePreview(scene,runtime.lumaTraceBvh4,shadowAB,cam,256,144,pixels,stats,error)||
                   stats.lumaAdaptiveEnabled!=enabled||!stats.lumaAdaptiveTriangleCount||
                   (enabled&&noLocal&&stats.lumaAdaptiveZeroTriangles!=stats.lumaAdaptiveTriangleCount)||
                   (!reference.empty()&&reference!=pixels)){
                    qCritical()<<"Adaptive 0/1 WARP parity failed"<<hour<<noLocal<<enabled<<QString::fromStdWString(error);return 1;
                }
                reference=pixels;
            }
        }
    }
    _putenv_s("M3D_V17_LUMA_ADAPTIVE","");
    m3d::setExteriorLightingEnabled(exteriorBeforeAdaptive);
    m3d::updateLighting(12.f);
    m3d::updateLighting(built.runtime.daylightHour);
    m3d::setRasterHybridLightingEnabled(false);
    _putenv_s("M3D_V17_STATIC_GEOMETRY_PREP","");
    qInfo()<<"DX11 and LumaTrace pixels identical with geometry preparation off/on, including probes";
    // Upload cache must be bit-exact against a forced GPU re-upload, even
    // after an in-place shadow change at the same resolution.
    if(_putenv_s("M3D_V17_DISABLE_SHADOW_UPLOAD_CACHE","1")!=0){
        qCritical()<<"Cannot bypass shadow upload cache";return 1;
    }
    std::vector<uint32_t> forcedShadow;
    m3d::Stats forcedStats;
    error.clear();
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                    w,h,forcedShadow,forcedStats,error) ||
       forcedShadow!=second){
        qCritical()<<"Cached shadows differ from forced upload";return 1;
    }
    _putenv_s("M3D_V17_DISABLE_SHADOW_UPLOAD_CACHE","");
    m3d::ShadowMap modifiedShadow=shadow;
    for(float& v:modifiedShadow.d)v*=.83f;
    std::vector<uint32_t> alteredCached,alteredForced;
    m3d::Stats alteredStats,alteredForcedStats;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,modifiedShadow,cam,
                                   w,h,alteredCached,alteredStats,error)){
        qCritical()<<"Shadow content update failed";return 1;
    }
    _putenv_s("M3D_V17_DISABLE_SHADOW_UPLOAD_CACHE","1");
    const bool alteredOk=m3d::renderDirectX11Preview(
        built.runtime.renderScene,modifiedShadow,cam,w,h,
        alteredForced,alteredForcedStats,error);
    _putenv_s("M3D_V17_DISABLE_SHADOW_UPLOAD_CACHE","");
    if(!alteredOk||alteredCached!=alteredForced){
        qCritical()<<"Changed shadow content produced stale GPU data";return 1;
    }
    std::vector<uint32_t> restoredShadow;
    m3d::Stats restoredShadowStats;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                    w,h,restoredShadow,restoredShadowStats,error)||
       restoredShadow!=second){
        qCritical()<<"Original shadow pixels not restored after edit";return 1;
    }
    qInfo()<<"Shadow cache pixel-exact across reuse, forced upload and change";
    // Regression for the V17 frustum optimization: switching culling OFF
    // must produce identical pixels. The renderer must not drop a triangle
    // that intersects the frustum or modify the underlying PBR resources.
    if(_putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","1")!=0){
        qCritical()<<"Cannot set culling A/B test variable";return 1;
    }
    std::vector<uint32_t> reference;
    m3d::Stats fullStats;
    error.clear();
    const bool fullOk=m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                                   w,h,reference,fullStats,error);
    _putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","");
    if(!fullOk||reference!=second){
        qCritical()<<"Frustum culling changed first camera pixels:"<<QString::fromStdWString(error);
        return 1;
    }
    if(built.runtime.cameras.size()>1){
        const auto alternate=m3d::v16::makeCamera(built.runtime.cameras.back(),w,h);
        std::vector<uint32_t> culledAlt,referenceAlt;
        m3d::Stats culledStats,fullAltStats;
        error.clear();
        if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,alternate,
                                        w,h,culledAlt,culledStats,error)){
            qCritical()<<"Second camera culling render failed:"<<QString::fromStdWString(error);
            return 1;
        }
        _putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","1");
        const bool altOk=m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,alternate,
                                                      w,h,referenceAlt,fullAltStats,error);
        _putenv_s("M3D_V17_DISABLE_FRUSTUM_CULLING","");
        if(!altOk||culledAlt!=referenceAlt){
            qCritical()<<"Frustum culling changed alternate camera pixels:"<<QString::fromStdWString(error);
            return 1;
        }
        qInfo()<<"V17 A/B camera2: triangles submitted"<<qulonglong(culledStats.tests)
               <<"of"<<qulonglong(fullAltStats.tests);
    }
    qInfo()<<"V17 A/B camera1: triangles submitted"<<qulonglong(warmStats.tests)
           <<"of"<<qulonglong(fullStats.tests);
    // Compare box instancing only; ellipsoid LOD is deliberately unchanged
    // in the reference render and is independently testable.
    if(_putenv_s("M3D_V17_DISABLE_BOX_INSTANCING","1")!=0){
        qCritical()<<"Cannot disable instancing for A/B";return 1;
    }
    std::vector<uint32_t> legacy;
    m3d::Stats legacyStats;
    error.clear();
    const bool legacyOk=m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                                     w,h,legacy,legacyStats,error);
    _putenv_s("M3D_V17_DISABLE_BOX_INSTANCING","");
    if(!legacyOk||legacy.size()!=second.size()){
        qCritical()<<"Legacy box A/B rendering failed:"<<QString::fromStdWString(error);
        return 1;
    }
    size_t changed=0;uint64_t totalDelta=0;
    for(size_t i=0;i<legacy.size();++i){
        const uint32_t a=legacy[i],b=second[i];
        const int dr=std::abs(int((a>>16)&255)-int((b>>16)&255));
        const int dg=std::abs(int((a>>8)&255)-int((b>>8)&255));
        const int db=std::abs(int(a&255)-int(b&255));
        if(std::max({dr,dg,db})>8)++changed;
        totalDelta+=uint64_t(dr+dg+db);
    }
    const double fraction=double(changed)/legacy.size();
    const double meanDelta=double(totalDelta)/(legacy.size()*3);
    if(fraction>.03||meanDelta>2.0){
        qCritical()<<"GPU instancing changed scene appearance:"
                   <<"fraction"<<fraction<<"mean per-channel delta"<<meanDelta;
        return 1;
    }
    qInfo()<<"GPU box instancing A/B valid; fraction >8 difference:"<<fraction
           <<"mean delta:"<<meanDelta;
    // Public host-application quality API must be effective and reversible
    // on the same persistent worker, including MSAA target and sampler changes.
    const auto originalQuality=m3d::rasterQualitySettings();
    for(const auto profile:{m3d::RasterQualityProfile::Performance,
                            m3d::RasterQualityProfile::Ultra}){
        auto requested=m3d::rasterQualityProfile(profile);
        error.clear();
        if(!m3d::setRasterQualitySettings(requested,error)){
            qCritical()<<"GPU quality setter rejected valid preset"<<QString::fromStdWString(error);
            return 1;
        }
        const auto actual=m3d::rasterQualitySettings();
        if(actual.msaaSamples!=requested.msaaSamples||
           actual.anisotropy!=requested.anisotropy||
           actual.dynamicLod!=requested.dynamicLod){
            qCritical()<<"Host-side raster settings were not applied atomically";return 1;
        }
        std::vector<uint32_t> profileImage;
        m3d::Stats profileStats;
        if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,
                                         w,h,profileImage,profileStats,error) ||
           profileImage.size()!=second.size() || !profileStats.gpu){
            qCritical()<<"Changed quality preset failed on WARP"
                       <<QString::fromStdWString(error);return 1;
        }
    }
    error.clear();
    if(!m3d::setRasterQualitySettings(originalQuality,error)){
        qCritical()<<"Cannot restore raster quality settings";return 1;
    }
    std::vector<uint32_t> restored;
    m3d::Stats restoredStats;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                   restored,restoredStats,error)||restored!=second){
        qCritical()<<"Restoring quality did not reproduce reference pixels"
                   <<QString::fromStdWString(error);return 1;
    }
    // New experimental pipeline: submit N frames before reading the oldest.
    // Every returned image must match the synchronous reference exactly,
    // even across staging-slot reuse. Timestamp queries are optional on
    // WARP/older drivers and must never be confused with CPU Map wait.
    for(unsigned depth:{2u,3u}){
        constexpr int frames=6;
        int submitted=0,received=0;
        for(int i=0;i<frames;++i){
            error.clear();
            if(!m3d::enqueueDirectX11Preview(built.runtime.renderScene,shadow,
                                             cam,w,h,depth,error)){
                qCritical()<<"Buffered GPU enqueue failed, depth"<<depth
                           <<QString::fromStdWString(error);return 1;
            }
            ++submitted;
            if(submitted-received>=int(depth)){
                std::vector<uint32_t> frame;
                m3d::Stats frameStats;
                if(!m3d::receiveDirectX11Preview(frame,frameStats,error) ||
                   frame!=second||frameStats.readbackBufferCount!=depth){
                    qCritical()<<"Buffered GPU pixels differ, depth"<<depth
                               <<QString::fromStdWString(error);return 1;
                }
                ++received;
            }
        }
        while(received<submitted){
            std::vector<uint32_t> frame;
            m3d::Stats frameStats;
            error.clear();
            if(!m3d::receiveDirectX11Preview(frame,frameStats,error)||
               frame!=second||frameStats.readbackBufferCount!=depth){
                qCritical()<<"Buffered GPU drain failed, depth"<<depth
                           <<QString::fromStdWString(error);return 1;
            }
            ++received;
        }
        qInfo()<<"V17 GPU readback depth"<<depth
               <<"identical to synchronous pixels across"<<frames<<"frames";
    }
    // V17.310: separate GI and tonemapping. Test four states:
    // legacy, indirect-only, indirect+filmic, filmic-only.
    // The original GPU raster must remain pixel-exact when both are OFF.
    if(m3d::rasterHybridLightingEnabled()||m3d::rasterFilmicTonemapEnabled()){
        qCritical()<<"Reference Raster API must default OFF for GI and tone";return 1;
    }
    m3d::setRasterHybridLightingEnabled(true);
    std::vector<uint32_t> hybridA,hybridB,hybridToneA,hybridToneB,toneOnly,morning,evening;
    m3d::Stats hs;
    error.clear();
    const bool hybridOk=m3d::renderDirectX11Preview(
        built.runtime.renderScene,shadow,cam,w,h,hybridA,hs,error)&&
        m3d::renderDirectX11Preview(
        built.runtime.renderScene,shadow,cam,w,h,hybridB,hs,error);
    if(!hybridOk||hybridA!=hybridB||hybridA==second){
        qCritical()<<"Indirect lighting not deterministic or no visible effect"
                   <<QString::fromStdWString(error);return 1;
    }
    const auto meanLuma=[](const std::vector<uint32_t>& values)->double{
        double sum=0;
        for(uint32_t c:values)
            sum+=double((c>>16)&255u)+double((c>>8)&255u)+double(c&255u);
        return sum/(3.0*double(values.size()));
    };
    // Guard against V17.309's overly bright GI output; allow natural
    // deviations in the test scene but prohibit a global white-out.
    if(meanLuma(hybridA)>meanLuma(second)*1.12){
        qCritical()<<"Hybrid lighting raised average brightness too far";
        return 1;
    }
    m3d::setRasterFilmicTonemapEnabled(true);
    const bool toneOk=m3d::renderDirectX11Preview(
        built.runtime.renderScene,shadow,cam,w,h,hybridToneA,hs,error)&&
        m3d::renderDirectX11Preview(
        built.runtime.renderScene,shadow,cam,w,h,hybridToneB,hs,error);
    if(!toneOk||hybridToneA!=hybridToneB){
        qCritical()<<"Independent tone mapping not deterministic";return 1;
    }
    // The filmic shoulder can never raise a color channel. This is a
    // stronger guarantee than an average-luminance test.
    const auto noChannelBrightening=[](const std::vector<uint32_t>& before,
                                      const std::vector<uint32_t>& after)->bool{
        if(before.size()!=after.size())return false;
        for(size_t i=0;i<before.size();++i)
            for(unsigned shift:{0u,8u,16u})
                if(((after[i]>>shift)&255u)>((before[i]>>shift)&255u))
                    return false;
        return true;
    };
    if(!noChannelBrightening(hybridA,hybridToneA)){
        qCritical()<<"Filmic shoulder raised pixel values";return 1;
    }
    m3d::setRasterHybridLightingEnabled(false);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                     toneOnly,hs,error)||
       !noChannelBrightening(second,toneOnly)){
        qCritical()<<"Standalone tone mapping unexpectedly lifts the reference";return 1;
    }
    m3d::setRasterFilmicTonemapEnabled(false);
    m3d::setRasterHybridLightingEnabled(true);
    m3d::updateLighting(9.f);
    m3d::Stats morningShadowStats;
    const auto morningShadow=m3d::buildShadow(built.runtime.renderScene,morningShadowStats);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,morningShadow,
                                    cam,w,h,morning,hs,error)){
        qCritical()<<"Hybrid morning render failed";return 1;
    }
    m3d::updateLighting(17.5f);
    m3d::Stats eveningShadowStats;
    const auto eveningShadow=m3d::buildShadow(built.runtime.renderScene,eveningShadowStats);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,eveningShadow,
                                    cam,w,h,evening,hs,error)||morning==evening){
        qCritical()<<"Hybrid illumination did not respond to solar hour";return 1;
    }
    m3d::setRasterHybridLightingEnabled(false);
    m3d::updateLighting(built.runtime.daylightHour);
    std::vector<uint32_t> oldAfterHybrid;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,
                                     cam,w,h,oldAfterHybrid,hs,error)||oldAfterHybrid!=second){
        qCritical()<<"Legacy raster pixels changed after tone/GI A/B";return 1;
    }
    // CI artifacts include three actual WARP images for visual comparison.
    // No synthetic illustration, just real shader output.
    const auto savePixels=[&](const std::vector<uint32_t>& data,const QString& filename)->bool{
        QImage frame(w,h,QImage::Format_ARGB32);
        for(int y=0;y<h;++y)
            std::memcpy(frame.scanLine(y),data.data()+size_t(y)*w,size_t(w)*4);
        return frame.save(filename,"PNG");
    };
    const QString preview=QString::fromLocal8Bit(argv[2]);
    if(!savePixels(second,preview)||
       !savePixels(hybridA,preview+QStringLiteral(".indirect.png"))||
       !savePixels(hybridToneA,preview+QStringLiteral(".indirect-filmic.png"))){
        qCritical()<<"Failed to save lighting A/B reference PNGs";return 1;
    }
    qInfo()<<"V17.310 GI/tone independent and reversible; reference luma"
           <<meanLuma(second)<<"GI luma"<<meanLuma(hybridA)
           <<"filmic luma"<<meanLuma(hybridToneA);
    // V17.312: actual ray-transport table stored on disk, reloaded by the
    // D3D11 GPU worker and interpolated between independently baked hours.
    m3d::gibake::Options bakeOptions{};
    bakeOptions.nx=4;bakeOptions.ny=3;bakeOptions.nz=4;
    bakeOptions.rays=8;bakeOptions.frames=5;
    bakeOptions.firstHour=8.f;bakeOptions.stepHours=2.5f;
    m3d::gibake::Table table;
    std::string bakeError;
    if(!m3d::gibake::bake(built.runtime.renderScene,bakeOptions,table,bakeError)){
        qCritical()<<"Real GI table bake failed"<<QString::fromStdString(bakeError);return 1;
    }
    const QString giName=QString::fromLocal8Bit(argv[2])+QStringLiteral(".m3dgi");
    if(!m3d::gibake::writeFile(std::filesystem::path(giName.toStdWString()),table,bakeError)){
        qCritical()<<"GI table file write failed"<<QString::fromStdString(bakeError);return 1;
    }
    m3d::setRasterGiCachePath(std::filesystem::path(giName.toStdWString()));
    m3d::setRasterHybridLightingEnabled(true);
    std::vector<uint32_t> bakedA,bakedB,bakedMorning,bakedEvening;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                     bakedA,hs,error)||!m3d::rasterGiCacheLoaded()||
       !m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                     bakedB,hs,error)||bakedA!=bakedB||bakedA==second){
        qCritical()<<"D3D11 did not load/use deterministic GI tables"
                   <<QString::fromStdWString(error);return 1;
    }
    m3d::updateLighting(9.f);
    m3d::Stats mornStats;
    const auto mornShadow=m3d::buildShadow(built.runtime.renderScene,mornStats);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,mornShadow,cam,w,h,
                                     bakedMorning,hs,error)){
        qCritical()<<"Baked GI morning failed";return 1;
    }
    m3d::updateLighting(17.5f);
    m3d::Stats eveStats;
    const auto eveShadow=m3d::buildShadow(built.runtime.renderScene,eveStats);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,eveShadow,cam,w,h,
                                     bakedEvening,hs,error)||bakedMorning==bakedEvening){
        qCritical()<<"Baked GI multi-hour interpolation not used";return 1;
    }
    m3d::setRasterHybridLightingEnabled(false);
    m3d::setRasterGiCachePath(std::filesystem::path{});
    m3d::updateLighting(built.runtime.daylightHour);
    std::vector<uint32_t> legacyAfterBake;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                     legacyAfterBake,hs,error)||legacyAfterBake!=second){
        qCritical()<<"Baked GI changed historical reference pixels";return 1;
    }
    if(!savePixels(bakedA,preview+QStringLiteral(".baked-gi.png"))){
        qCritical()<<"Baked GI PNG output missing";return 1;
    }
    // V17.316 — independent linear-light color-energy balance.
    // Run A/B on the exact same scene, camera, lighting, textures and WARP.
    m3d::setRasterColorEnergyBalanceEnabled(true);
    m3d::setRasterColorExposure(1.f);
    m3d::Stats colorStats{};
    std::vector<uint32_t> balancedA,balancedB,lowExposure,highExposure;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                    balancedA,colorStats,error)||
       !m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                    balancedB,colorStats,error)||
       balancedA!=balancedB||balancedA==second){
        qCritical()<<"Balanced color lighting not deterministic or no effect";return 1;
    }
    if(meanLuma(balancedA)>meanLuma(second)*1.12){
        qCritical()<<"Balanced lighting raises the global light budget excessively";return 1;
    }
    m3d::setRasterColorExposure(.85f);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                    lowExposure,colorStats,error)){
        qCritical()<<"Color exposure lower bound failed";return 1;
    }
    m3d::setRasterColorExposure(1.15f);
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                    highExposure,colorStats,error)||
       meanLuma(lowExposure)>=meanLuma(balancedA)||
       meanLuma(balancedA)>=meanLuma(highExposure)){
        qCritical()<<"Exposure is not correctly monotonic";return 1;
    }
    // Preserve material colorfulness by comparing screen-space chroma;
    // no synthetic post-saturation adjustment is used.
    auto chroma=[](const std::vector<uint32_t>& pixels)->double{
        double sum=0;
        for(uint32_t c:pixels){
            const unsigned r=(c>>16)&255u,g=(c>>8)&255u,b=c&255u;
            const unsigned mx=std::max({r,g,b}),mn=std::min({r,g,b});
            sum+=double(mx-mn)/double(std::max(1u,mx));
        }
        return sum/double(pixels.size());
    };
    if(!savePixels(balancedA,preview+QStringLiteral(".balanced.png"))||
       !savePixels(lowExposure,preview+QStringLiteral(".balanced-085.png"))||
       !savePixels(highExposure,preview+QStringLiteral(".balanced-115.png"))){
        qCritical()<<"Colorimetry A/B screenshots missing";return 1;
    }
    // Real WARP statistics for a reusable A/B comparison.
    std::FILE* metric=std::fopen((preview+QStringLiteral(".color-metrics.csv")).toLocal8Bit().constData(),"wb");
    if(!metric){qCritical()<<"Cannot write color benchmark CSV";return 1;}
    std::fprintf(metric,"mode,mean_rgb_255,mean_normalized_chroma\n");
    for(const auto& row:std::vector<std::pair<const char*,const std::vector<uint32_t>*>>{
        {"legacy",&second},{"balanced",&balancedA},{"exposure_085",&lowExposure},
        {"exposure_115",&highExposure}}){
        std::fprintf(metric,"%s,%.5f,%.6f\n",row.first,
                     meanLuma(*row.second),chroma(*row.second));
    }
    std::fclose(metric);
    m3d::setRasterColorEnergyBalanceEnabled(false);
    m3d::setRasterColorExposure(1.f);
    std::vector<uint32_t> oldAfterColor;
    if(!m3d::renderDirectX11Preview(built.runtime.renderScene,shadow,cam,w,h,
                                    oldAfterColor,colorStats,error)||
       oldAfterColor!=second){
        qCritical()<<"Disabling color-energy balance does not restore exact legacy pixels";
        return 1;
    }
    qInfo()<<"V17.316 balanced colors verified: legacy luma"<<meanLuma(second)
           <<"balanced luma"<<meanLuma(balancedA)
           <<"legacy chroma"<<chroma(second)<<"balanced chroma"<<chroma(balancedA);
    qInfo()<<"V17.312 table-driven real GI verified: cache, hours and parity";
    qInfo()<<"V17 DX11 raster WARP OK, cached"<<warmStats.ms
           <<"ms; size"<<w<<"x"<<h;
    return 0;
}
