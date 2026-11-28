#include "../escargot/debug.h"
#include "../escargot/gpu_packet.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <limits>
namespace e=m3d::escargot;
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
static bool save(const QDir& out,const QString& name,const std::vector<uint32_t>& p,int w,int h){
    return p.size()==size_t(w)*h&&QImage(reinterpret_cast<const uchar*>(p.data()),w,h,w*4,QImage::Format_RGB32).save(out.filePath(name));
}
// Re-evaluate a particular original triangle in CPU float, without the
// low/high clipping, so repeated crossings and shared edges can be diagnosed.
struct RawIntersection {float t=0,u=0,v=0;bool finite=false;};
static RawIntersection rawIntersection(const m3d::Scene& scene,uint32_t primitive,const m3d::EscargotRayQuery& q){
    RawIntersection hit;
    if(primitive>=scene.obj.size())return hit;
    const auto& o=scene.obj[primitive];
    const m3d::V origin{q.origin[0],q.origin[1],q.origin[2]},dir{q.direction[0],q.direction[1],q.direction[2]};
    const auto e=o.b-o.a,f=o.c-o.a,p=m3d::cross(dir,f);
    const float det=m3d::dot(e,p);if(std::abs(det)<1e-10f)return hit;
    const auto a=origin-o.a,z=m3d::cross(a,e);
    hit.u=m3d::dot(a,p)/det;hit.v=m3d::dot(dir,z)/det;hit.t=m3d::dot(f,z)/det;
    hit.finite=std::isfinite(hit.t)&&std::isfinite(hit.u)&&std::isfinite(hit.v);return hit;
}
static double quantile(std::vector<double> v,double q){std::sort(v.begin(),v.end());return v.empty()?0:v[size_t(std::ceil(q*v.size()))-1];}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();
    if(args.size()<3||args.size()>7){qCritical()<<"Usage: v17_escargot_ab scene.json outputDir [width] [height] [repeats] [rayBudget]";return 2;}
    bool ok=true;const int w=args.size()>3?args[3].toInt(&ok):640;if(!ok||w<16||w>2048)return 2;
    const int h=args.size()>4?args[4].toInt(&ok):360;if(!ok||h<16||h>2048)return 2;
    const int repeats=args.size()>5?args[5].toInt(&ok):5;if(!ok||repeats<1||repeats>120)return 2;
    e::Options opt;if(args.size()>6)opt.rayBudget=args[6].toUInt(&ok);if(!ok||!e::validOptions(opt))return 2;
    const QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 3;
    auto loaded=m3d::v16::loadScene(args[1]);if(!loaded){qCritical()<<loaded.error;return 4;}
    auto built=m3d::v16::buildRuntime(loaded.scene,256);if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){qCritical()<<built.error<<built.runtime.lumaTraceBvhError;return 5;}
    auto& rt=built.runtime;auto& scene=rt.renderScene;
    if(!e::objectMap(rt,scene.escargotReceiverIds)){qCritical()<<"Invalid receiver ownership";return 5;}
    scene.markGpuGeometryChanged();
    e::Prepared session(rt,loaded.scene,opt.mode);
    _putenv_s("M3D_V17_ZREFLECT","1");_putenv_s("M3D_V17_ZATMOSPHERE","0");
    m3d::setRasterPeripheralMultiresOverride(0);m3d::setRasterCloudsEnabled(false);m3d::setRasterHybridLightingEnabled(true);
    m3d::setRasterColorEnergyBalanceEnabled(true);m3d::setRasterFilmicTonemapEnabled(true);m3d::setRasterColorExposure(.85f);
    auto quality=m3d::rasterQualityProfile(m3d::RasterQualityProfile::High);quality.msaaSamples=1;quality.dynamicLod=false;
    std::wstring err;if(!m3d::setRasterQualitySettings(quality,err)){qCritical()<<QString::fromStdWString(err);return 6;}
    m3d::updateLighting(rt.daylightHour);m3d::Stats shadowStats;const auto shadow=m3d::buildShadow(scene,shadowStats);
    QFile csv(out.filePath("frames.csv"));if(!csv.open(QIODevice::WriteOnly|QIODevice::Text))return 7;
    QTextStream log(&csv);log<<"camera,iteration,mode,total_ms,raster_ms,matrix_ms,composition_ms,atlas_ms,secondary_cameras,rays,candidates,deferred,complete_paths,unknown_paths,atlas_nonzero,texture_samples,reused_cells,gpu_ms,gpu_timing_valid,prepare_once_ms,candidate_ms,admission_ms,escargot_total_ms,static_candidates,packet_bytes,packet_tiles,packet_layer_lists,packet_observations,affine_layers,ray_workers\n";
    QFile rayFile(out.filePath("gpu_bvh_mismatches.csv"));if(!rayFile.open(QIODevice::WriteOnly|QIODevice::Text))return 22;
    QTextStream rayLog(&rayFile);rayLog.setRealNumberPrecision(12);
    rayLog<<"camera,index,classification,previous_primitive,low,ox,oy,oz,dx,dy,dz,cpu_primitive,cpu_owner,cpu_depth,gpu_status,gpu_primitive,gpu_owner,gpu_depth,gpu_u,gpu_v,gpu_triangle_cpu_t,gpu_triangle_cpu_u,gpu_triangle_cpu_v,gpu_triangle_cpu_finite\n";
    QFile waveFile(out.filePath("gpu_wavefront_frames.csv"));if(!waveFile.open(QIODevice::WriteOnly|QIODevice::Text))return 23;
    QTextStream waveLog(&waveFile);waveLog<<"camera,iteration,total_ms,matrix_ms,trace_wall_ms,prepare_ms,dispatch_readback_ms,packet_ms,raster_ms,rays,batches,max_batch,complete_paths,unknown_paths,packet_bytes,secondary_cameras\n";
    QFile waveDiffFile(out.filePath("gpu_wavefront_mismatches.csv"));if(!waveDiffFile.open(QIODevice::WriteOnly|QIODevice::Text))return 23;
    QTextStream waveDiffLog(&waveDiffFile);waveDiffLog.setRealNumberPrecision(12);waveDiffLog<<"camera,iteration,surface,index,cpu_primitive,gpu_primitive,cpu_depth,gpu_depth,cpu_low,gpu_low,cpu_previous,gpu_previous,cpu_ox,cpu_oy,cpu_oz,gpu_ox,gpu_oy,gpu_oz,cpu_dx,cpu_dy,cpu_dz,gpu_dx,gpu_dy,gpu_dz\n";
    QJsonArray summary;int tested=0;uint64_t allChanged=0,allActive=0,allCoverage=0,allEffective=0;int gpuReplayFailed=0,waveFailed=0;
    for(const QString id:{QString("v17_showcase_pool_634"),QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==id;});if(it==rt.cameras.end()){qWarning()<<"Missing camera"<<id;continue;}
        const auto cam=m3d::v16::makeCamera(*it,w,h);std::vector<uint32_t> a,b;std::vector<double> aTimes,bTimes;std::vector<float> lastPacket;
        auto runA=[&](int index,bool record)->bool{
            m3d::Stats st;const auto t=Clock::now();
            if(!m3d::renderLumaTracePreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,a,st,err)){qCritical()<<QString::fromStdWString(err);return false;}
            const auto elapsed=ms(t);if(!record)return true;
            aTimes.push_back(elapsed);allActive+=st.zReflectActive;
            log<<id<<','<<index<<",A,"<<elapsed<<','<<elapsed<<",0,0,0,"<<st.zReflectActive<<",0,"<<st.zReflectCandidates<<','<<st.zReflectDeferred<<",0,0,0,0,0,"<<st.gpuTotalMs<<','<<int(st.gpuTimingValid)<<",0,0,0,0,0,0,0,0,0,0,0\n";return true;
        };
        auto runB=[&](int index,bool record)->bool{
            const auto t=Clock::now();auto frame=session.frame(cam,w,h,opt,false);if(!frame.ok){qCritical()<<QString::fromStdString(frame.error);return false;}
            const auto atlasStart=Clock::now();auto packet=e::packGpu(frame,scene,rt.objects.size());
            if(!packet.ok){qCritical()<<QString::fromStdString(packet.error);return false;}
            const uint64_t nonzero=packet.tiles; // recorded separately; not a pixel coverage count
            const double atlasMs=ms(atlasStart);const auto rasterStart=Clock::now();m3d::Stats st;
            if(!m3d::renderLumaTraceEscargotGpuPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,packet.data,b,st,err)){qCritical()<<QString::fromStdWString(err);return false;}
            const double rasterMs=ms(rasterStart),elapsed=ms(t);if(st.zReflectActive!=0){qCritical()<<"Escargot unexpectedly submitted secondary views";return false;}
            if(!record)return true;bTimes.push_back(elapsed);allCoverage+=nonzero;
            log<<id<<','<<index<<",B,"<<elapsed<<','<<rasterMs<<','<<frame.matrixMs<<','<<frame.compositionMs<<','<<atlasMs<<','<<st.zReflectActive<<','<<frame.rays<<','<<frame.candidates<<','<<frame.deferred<<','<<frame.completePaths<<','<<frame.unknownPaths<<','<<0<<','<<qulonglong(frame.textureSamples)<<','<<qulonglong(frame.reusedCells)<<','<<st.gpuTotalMs<<','<<int(st.gpuTimingValid)<<','<<frame.prepareMs<<','<<frame.candidateMs<<','<<frame.admissionMs<<','<<frame.totalMs<<','<<frame.staticCandidates<<','<<qulonglong(packet.data.size()*sizeof(float))<<','<<qulonglong(packet.tiles)<<','<<qulonglong(packet.layerLists)<<','<<qulonglong(packet.observations)<<','<<qulonglong(packet.affineLayers)<<','<<frame.rayWorkers<<'\n';lastPacket=std::move(packet.data);return true;
        };
        if(!runA(-1,false)||!runB(-1,false))return 8;
        for(int j=0;j<repeats;++j){if(j%2==0){if(!runA(j,true)||!runB(j,true))return 9;}else{if(!runB(j,true)||!runA(j,true))return 9;}}
        if(a.size()!=size_t(w)*h||b.size()!=a.size())return 10;
        uint64_t changed=0;double errorSum=0;std::vector<uint32_t> diff(a.size());
        for(size_t i=0;i<a.size();++i){int dr=std::abs(int((a[i]>>16)&255)-int((b[i]>>16)&255)),dg=std::abs(int((a[i]>>8)&255)-int((b[i]>>8)&255)),db=std::abs(int(a[i]&255)-int(b[i]&255));
            changed+=(dr||dg||db);errorSum+=dr+dg+db;diff[i]=0xff000000u|(uint32_t(std::min(255,dr*4))<<16)|(uint32_t(std::min(255,dg*4))<<8)|uint32_t(std::min(255,db*4));}
        allChanged+=changed;
        // Negative control: suppress cameras AND send an empty atlas. A/B
        // difference alone could only prove A's cameras were removed.
        std::vector<float> empty(8,0.f);std::vector<uint32_t> control;m3d::Stats controlStats;
        if(!m3d::renderLumaTraceEscargotGpuPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,empty,control,controlStats,err)||control.size()!=b.size()||controlStats.zReflectActive!=0)return 14;
        uint64_t effective=0;for(size_t i=0;i<b.size();++i)effective+=((b[i]&0xffffffu)!=(control[i]&0xffffffu));
        allEffective+=effective;
        if(!save(out,id+"-B-empty-control.png",control,w,h))return 14;
        // Old CPU reference, deliberately outside A/B timing.
        auto serialOpt=opt;serialOpt.rayWorkers=1;serialOpt.captureQueries=true;
        auto referenceFrame=session.frame(cam,w,h,serialOpt,true);if(!referenceFrame.ok)return 16;
        const auto atlas=e::composeScreen(referenceFrame,w,h);
        std::vector<float> cpuColor(size_t(w)*h*4),cpuMeta(cpuColor.size());
        for(size_t i=0;i<atlas.size();++i){const auto& p=atlas[i];if(p.alpha<=0||p.weight<=0)continue;
            cpuColor[4*i]=p.rgb.x;cpuColor[4*i+1]=p.rgb.y;cpuColor[4*i+2]=p.rgb.z;cpuColor[4*i+3]=p.alpha;
            cpuMeta[4*i]=p.receiverDepth;cpuMeta[4*i+1]=float(p.receiverMaterial+1);cpuMeta[4*i+2]=p.weight;cpuMeta[4*i+3]=float(p.receiverObject);
        }
        std::vector<uint32_t> cpuReference;m3d::Stats cpuStats;
        if(!m3d::renderLumaTraceEscargotPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,cpuColor,cpuMeta,cpuReference,cpuStats,err)||!save(out,id+"-C-cpu-reference.png",cpuReference,w,h))return 16;
        // Fixed-pose reuse diagnostic. Keep A/B cold preparation timings
        // unchanged; do not pass off this static case as a moving-camera gain.
        auto cachedPacket=e::packGpu(referenceFrame,scene,rt.objects.size());if(!cachedPacket.ok)return 17;
        if(cachedPacket.data!=lastPacket){qCritical()<<"Serial/parallel optical observations differ"<<id;return 18;}
        std::vector<double> fixedTimes;std::vector<uint32_t> fixed;m3d::Stats fixedStats;
        for(int j=0;j<repeats;++j){const auto start=Clock::now();
            if(!m3d::renderLumaTraceEscargotGpuPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,cachedPacket.data,fixed,fixedStats,err))return 17;
            fixedTimes.push_back(ms(start));
            if(fixed!=b){qCritical()<<"Serial/parallel packet or fixed-pose reuse parity failed"<<id;return 18;}
        }
        // Exact-pose CPU cache + resident GPU buffer. Warm once; no CPU
        // reconstruction or GPU packet upload on the timed cache hits.
        e::PacketCache residentCache;bool reused=false;
        const auto& residentPacket=residentCache.get(session,scene,rt.objects.size(),cam,w,h,opt,reused);
        if(reused||!residentPacket.ok||residentPacket.data!=lastPacket)return 19;
        std::vector<uint32_t> resident;m3d::Stats residentStats;
        if(!m3d::renderLumaTraceEscargotGpuResidentPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,residentPacket.data,resident,residentStats,err)||resident!=b)return 19;
        std::vector<double> residentTimes;
        for(int j=0;j<repeats;++j){auto start=Clock::now();const auto& p=residentCache.get(session,scene,rt.objects.size(),cam,w,h,opt,reused);
            if(!reused||!p.ok||!m3d::renderLumaTraceEscargotGpuResidentPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,p.data,resident,residentStats,err))return 19;
            residentTimes.push_back(ms(start));if(resident!=b)return 19;
        }
        // Actual moving camera: cache must miss on each changed eye position.
        std::vector<double> moveA,moveB;std::vector<uint32_t> movingPixels;m3d::Stats movingStats;
        for(int j=1;j<=3;++j){auto shifted=cam;shifted.eye=cam.eye+cam.r*(.025f*float(j));
            auto start=Clock::now();
            if(!m3d::renderLumaTracePreview(scene,rt.lumaTraceBvh4,shadow,shifted,w,h,movingPixels,movingStats,err))return 20;
            moveA.push_back(ms(start));start=Clock::now();
            const auto& p=residentCache.get(session,scene,rt.objects.size(),shifted,w,h,opt,reused);
            if(reused||!p.ok||!m3d::renderLumaTraceEscargotGpuResidentPreview(scene,rt.lumaTraceBvh4,shadow,shifted,w,h,p.data,movingPixels,movingStats,err))return 20;
            moveB.push_back(ms(start));
        }
        // Replay the exact real optical CPU queries in a single native compute
        // dispatch. This validates the GPU traversal, not a full GPU path solver.
        std::vector<m3d::EscargotRayQuery> queries;std::vector<e::RecordedQuery> reference;
        for(const auto& sf:referenceFrame.surfaces)for(const auto& q:sf.recorded){queries.push_back(q.query);reference.push_back(q);}
        uint64_t gpuMismatch=0,gpuOverflow=0,nearLow=0,sameOwner=0,differentOwner=0,hitMiss=0,depthOnly=0;
        double gpuPrepare=0,gpuQuery=0,warmPrepare=0,warmQuery=0;
        std::vector<m3d::EscargotRayHit> hits;
        bool gpuTraceOk=!queries.empty()&&m3d::traceEscargotGpuBatch(scene,rt.lumaTraceBvh4,queries,hits,gpuPrepare,gpuQuery,err)&&hits.size()==reference.size();
        if(gpuTraceOk){
            for(size_t i=0;i<hits.size();++i){const auto& x=hits[i];const auto& y=reference[i];const auto& q=y.query;
                if(x.status>1){++gpuOverflow;continue;}
                const bool cpuHit=y.primitive!=0xffffffffu,gpuHit=x.status==1;
                const bool mismatch=cpuHit!=gpuHit||(cpuHit&&gpuHit&&(x.primitive!=y.primitive||!std::isfinite(x.depth)||std::abs(x.depth-y.depth)>std::max(.0001f,std::abs(y.depth)*.00001f)));
                if(!mismatch)continue;
                ++gpuMismatch;
                const auto owner=[&](uint32_t p){return p<scene.escargotReceiverIds.size()?scene.escargotReceiverIds[p]:0u;};
                const uint32_t cpuOwner=cpuHit?owner(y.primitive):0u,gpuOwner=gpuHit?owner(x.primitive):0u;
                const auto raw=rawIntersection(scene,x.primitive,q);
                // Classify likely repeat of a just-crossed surface; not an exemption.
                const float tolerance=std::max(.00001f,std::abs(q.low)*.000002f);
                const bool near=gpuHit&&std::isfinite(x.depth)&&std::abs(x.depth-q.low)<=tolerance;
                const char* category="depth_only";
                if(near){++nearLow;category="near_low";}
                else if(cpuHit!=gpuHit){++hitMiss;category="hit_miss";}
                else if(x.primitive!=y.primitive){
                    if(cpuOwner==gpuOwner){++sameOwner;category="same_owner";}
                    else{++differentOwner;category="different_owner";}
                }else ++depthOnly;
                rayLog<<id<<','<<qulonglong(i)<<','<<category<<','<<q.previousPrimitive<<','<<q.low<<','<<q.origin[0]<<','<<q.origin[1]<<','<<q.origin[2]<<','<<q.direction[0]<<','<<q.direction[1]<<','<<q.direction[2]<<','<<y.primitive<<','<<cpuOwner<<','<<y.depth<<','<<x.status<<','<<x.primitive<<','<<gpuOwner<<','<<x.depth<<','<<x.u<<','<<x.v<<','<<raw.t<<','<<raw.u<<','<<raw.v<<','<<int(raw.finite)<<'\n';
            }
            // Second dispatch on the same buffer to separate cold-start latency.
            std::vector<m3d::EscargotRayHit> warmHits;std::wstring warmError;
            if(!m3d::traceEscargotGpuBatch(scene,rt.lumaTraceBvh4,queries,warmHits,warmPrepare,warmQuery,warmError)||warmHits.size()!=hits.size()){
                gpuTraceOk=false;err=warmError;
            }else{
                for(size_t i=0;i<hits.size();++i){const auto& a=hits[i];const auto& b=warmHits[i];if(a.status!=b.status||a.primitive!=b.primitive||a.depth!=b.depth||a.u!=b.u||a.v!=b.v){gpuTraceOk=false;err=L"GPU replay nondeterministic";break;}}
            }
            rayLog.flush();
        }else qWarning()<<"Experimental GPU trace unavailable"<<QString::fromStdWString(err);
        if(!gpuTraceOk||gpuOverflow||gpuMismatch)++gpuReplayFailed;
        const QString gpuTraceError=gpuTraceOk?QString():QString::fromStdWString(err);
        qInfo()<<id<<"GPU BVH replay"<<gpuTraceOk<<"queries"<<qulonglong(queries.size())<<"mismatches"<<qulonglong(gpuMismatch)<<"overflow"<<qulonglong(gpuOverflow)<<"ms"<<gpuQuery;
        // D is a live path solver: only the queries for the current wave are
        // submitted, and returned GPU hits drive every subsequent crossing.
        // W uses the identical fixed-quota scheduler, but CPU intersections.
        e::WaveStats wStats,dStats;auto waveOpt=opt;waveOpt.captureQueries=true;
        auto wStart=Clock::now();auto waveCpu=session.frame(cam,w,h,waveOpt,false,true,false,&wStats);
        const double waveCpuMatrix=ms(wStart);
        if(!waveCpu.ok){qCritical()<<QString::fromStdString(waveCpu.error);return 23;}
        auto wPacket=e::packGpu(waveCpu,scene,rt.objects.size());if(!wPacket.ok)return 23;
        std::vector<uint32_t> waveRef;m3d::Stats wRender;
        if(!m3d::renderLumaTraceEscargotGpuPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,wPacket.data,waveRef,wRender,err)||!save(out,id+"-W-wavefront-cpu.png",waveRef,w,h))return 23;
        std::vector<double> dTimes,dMatrixTimes,dDispatchTimes,dPackTimes,dRasterTimes;
        uint64_t wavePacketDifferences=0,wavePixelDifferences=0,waveQueryDifferences=0,waveComparedQueries=0,oldBDifferences=0;
        uint32_t waveRays=0,waveBatches=0,waveMaxBatch=0,waveComplete=0,waveUnknown=0;
        bool waveOk=true;QString waveError;std::vector<uint32_t> dPixels;
        for(int j=-1;j<repeats;++j){
            const auto begin=Clock::now();auto waveGpu=session.frame(cam,w,h,waveOpt,false,true,true,&dStats);
            const double matrixTime=ms(begin);
            if(!waveGpu.ok){waveOk=false;waveError=QString::fromStdString(waveGpu.error);break;}
            auto packStart=Clock::now();auto dPacket=e::packGpu(waveGpu,scene,rt.objects.size());const double packTime=ms(packStart);
            if(!dPacket.ok){waveOk=false;waveError=QString::fromStdString(dPacket.error);break;}
            m3d::Stats st;const auto renderStart=Clock::now();
            if(!m3d::renderLumaTraceEscargotGpuPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,dPacket.data,dPixels,st,err)||st.zReflectActive||dPixels.size()!=waveRef.size()){
                waveOk=false;waveError=QString::fromStdWString(err);break;}
            const double rasterTime=ms(renderStart),total=ms(begin);
            if(j>=0){dTimes.push_back(total);dMatrixTimes.push_back(matrixTime);dDispatchTimes.push_back(dStats.dispatchMs);dPackTimes.push_back(packTime);dRasterTimes.push_back(rasterTime);
                waveLog<<id<<','<<j<<','<<total<<','<<matrixTime<<','<<dStats.traceMs<<','<<dStats.prepareMs<<','<<dStats.dispatchMs<<','<<packTime<<','<<rasterTime<<','<<waveGpu.rays<<','<<dStats.batches<<','<<dStats.maxBatch<<','<<waveGpu.completePaths<<','<<waveGpu.unknownPaths<<','<<qulonglong(dPacket.data.size()*sizeof(float))<<','<<st.zReflectActive<<'\n';}
            uint64_t packetDiff=std::max(dPacket.data.size(),wPacket.data.size())-std::min(dPacket.data.size(),wPacket.data.size());
            for(size_t k=0;k<std::min(dPacket.data.size(),wPacket.data.size());++k)packetDiff+=dPacket.data[k]!=wPacket.data[k];
            uint64_t pixelDiff=0,oldDiff=0;
            for(size_t k=0;k<dPixels.size();++k){pixelDiff+=dPixels[k]!=waveRef[k];oldDiff+=dPixels[k]!=b[k];}
            wavePacketDifferences=std::max(wavePacketDifferences,packetDiff);wavePixelDifferences=std::max(wavePixelDifferences,pixelDiff);oldBDifferences=std::max(oldBDifferences,oldDiff);
            uint64_t queryDiff=0,compared=0;
            for(size_t k=0;k<waveCpu.surfaces.size();++k){
                const auto& x=waveCpu.surfaces[k].recorded;const auto& y=waveGpu.surfaces[k].recorded;
                queryDiff+=std::max(x.size(),y.size())-std::min(x.size(),y.size());
                for(size_t m=0;m<std::min(x.size(),y.size());++m){
                    ++compared;const auto& a=x[m];const auto& bq=y[m];
                    bool same=a.primitive==bq.primitive&&a.depth==bq.depth&&a.query.low==bq.query.low&&a.query.previousPrimitive==bq.query.previousPrimitive;
                    for(int axis=0;axis<3;++axis)same=same&&a.query.origin[axis]==bq.query.origin[axis]&&a.query.direction[axis]==bq.query.direction[axis];
                    queryDiff+=!same;
                    if(!same&&j==-1){waveDiffLog<<id<<','<<j<<','<<qulonglong(k)<<','<<qulonglong(m)<<','<<a.primitive<<','<<bq.primitive<<','<<a.depth<<','<<bq.depth<<','<<a.query.low<<','<<bq.query.low<<','<<a.query.previousPrimitive<<','<<bq.query.previousPrimitive;
                        for(float v:a.query.origin)waveDiffLog<<','<<v;for(float v:bq.query.origin)waveDiffLog<<','<<v;
                        for(float v:a.query.direction)waveDiffLog<<','<<v;for(float v:bq.query.direction)waveDiffLog<<','<<v;waveDiffLog<<'\n';}
                }
            }
            waveQueryDifferences=std::max(waveQueryDifferences,queryDiff);waveComparedQueries=std::max(waveComparedQueries,compared);
            waveRays=waveGpu.rays;waveBatches=dStats.batches;waveMaxBatch=dStats.maxBatch;waveComplete=waveGpu.completePaths;waveUnknown=waveGpu.unknownPaths;
            if(waveGpu.rays!=waveCpu.rays||waveGpu.completePaths!=waveCpu.completePaths||waveGpu.unknownPaths!=waveCpu.unknownPaths){waveOk=false;waveError="path counters differ";break;}
        }
        if(!dPixels.empty()&&!save(out,id+"-D-wavefront-gpu.png",dPixels,w,h))return 23;
        const bool waveParity=waveOk&&!wavePacketDifferences&&!wavePixelDifferences&&!waveQueryDifferences&&dTimes.size()==size_t(repeats);
        if(!waveParity)++waveFailed;
        qInfo()<<id<<"GPU wavefront"<<waveParity<<"rays"<<waveRays<<"batches"<<waveBatches<<"query diffs"<<qulonglong(waveQueryDifferences)<<"packet diffs"<<qulonglong(wavePacketDifferences)<<"pixel diffs"<<qulonglong(wavePixelDifferences)<<"p50"<<quantile(dTimes,.5)<<waveError;
        // A must remain stable after switching back from B.
        std::vector<uint32_t> again;m3d::Stats againStats;
        if(!m3d::renderLumaTracePreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,again,againStats,err)||again!=a){qCritical()<<"A/B restore parity failed"<<id;return 15;}
        if(!save(out,id+"-A-virtual.png",a,w,h)||!save(out,id+"-B-escargot.png",b,w,h)||!save(out,id+"-difference-x4.png",diff,w,h))return 11;
        summary.append(QJsonObject{{"camera",id},{"A_p50_ms",quantile(aTimes,.50)},{"A_p95_ms",quantile(aTimes,.95)},{"B_p50_ms",quantile(bTimes,.50)},{"B_p95_ms",quantile(bTimes,.95)},{"B_fixed_pose_reuse_p50_ms",quantile(fixedTimes,.50)},{"B_fixed_pose_reuse_p95_ms",quantile(fixedTimes,.95)},{"B_resident_p50_ms",quantile(residentTimes,.50)},{"B_resident_p95_ms",quantile(residentTimes,.95)},{"A_moving_p50_ms",quantile(moveA,.50)},{"B_moving_p50_ms",quantile(moveB,.50)},{"gpuTraceAvailable",gpuTraceOk},{"gpuTraceError",gpuTraceError},{"gpuTraceQueries",double(queries.size())},{"gpuTraceMismatches",double(gpuMismatch)},{"gpuTraceOverflow",double(gpuOverflow)},{"gpuTracePrepareMs",gpuPrepare},{"gpuTraceReplayMs",gpuQuery},{"gpuTraceWarmReplayMs",warmQuery},{"gpuMismatchNearLow",double(nearLow)},{"gpuMismatchSameOwner",double(sameOwner)},{"gpuMismatchDifferentOwner",double(differentOwner)},{"gpuMismatchHitMiss",double(hitMiss)},{"gpuMismatchDepthOnly",double(depthOnly)},{"wavefrontValidated",waveParity},{"wavefrontError",waveError},{"D_p50_ms",quantile(dTimes,.50)},{"D_p95_ms",quantile(dTimes,.95)},{"D_matrix_p50_ms",quantile(dMatrixTimes,.50)},{"D_dispatch_p50_ms",quantile(dDispatchTimes,.50)},{"D_pack_p50_ms",quantile(dPackTimes,.50)},{"D_raster_p50_ms",quantile(dRasterTimes,.50)},{"W_cpu_matrix_ms",waveCpuMatrix},{"wavefrontRays",int(waveRays)},{"wavefrontCpuRays",int(waveCpu.rays)},{"wavefrontBatches",int(waveBatches)},{"wavefrontMaxBatch",int(waveMaxBatch)},{"wavefrontComplete",int(waveComplete)},{"wavefrontUnknown",int(waveUnknown)},{"wavefrontComparedQueries",double(waveComparedQueries)},{"wavefrontQueryDifferences",double(waveQueryDifferences)},{"wavefrontPacketDifferences",double(wavePacketDifferences)},{"wavefrontPixelDifferences",double(wavePixelDifferences)},{"wavefrontVsOldBPixelDifferences",double(oldBDifferences)},{"changedPixels",double(changed)},{"escargotEffectivePixels",double(effective)},{"meanAbsChannelDifference",errorSum/(3*a.size())}});
        qInfo()<<id<<"A p50"<<quantile(aTimes,.5)<<"B p50"<<quantile(bTimes,.5)<<"changed"<<qulonglong(changed)<<"effective Escargot pixels"<<qulonglong(effective);++tested;log.flush();
    }
    if(!tested||!allActive||!allCoverage||!allChanged||!allEffective){qCritical()<<"Invalid A/B: no active virtual camera, optical data or visible changes";return 12;}
    QFile json(out.filePath("summary.json"));if(!json.open(QIODevice::WriteOnly))return 13;
    const QJsonObject report{{"scene",loaded.scene.revision},{"width",w},{"height",h},{"repeats",repeats},{"rayBudget",int(opt.rayBudget)},{"textureResolutionCap",256},{"msaaRequested",1},{"modeB","CPU sparse BVH observations + compact GPU four-neighbor reconstruction, no secondary cameras"},{"receiverMatch","exact source object ID + material + local tangent plane; no averaged receiver depth rejection"},{"sourceLighting","resident GPU textures + simplified proxy, not LT radiance"},{"atlasNonzeroNote","not applicable in GPU mode; use effective pixels and packet_tiles"},{"cpuReference","untimed C capture, prior CPU reconstruction and depth gate, serial rays"},{"fixedPoseReuse","separate diagnostic: identical camera/scene/lighting, reuse CPU packet; includes GPU upload/render/readback, not moving-camera timing"},{"residentPose","exact camera/light/scene/opt key; byte-identical GPU packet remains resident; includes raster/readback"},{"movingPose","three actual lateral shifts 2.5/5/7.5 cm, cache misses enforced"},{"gpuTrace","experimental native nearest-hit replay on same DX11 device and resident BVH nodes; full transparent triangle geometry/order, not active B path"},{"forceWarp",qEnvironmentVariable("M3D_V16_FORCE_WARP")},{"gpuReplayValidated",gpuReplayFailed==0},{"gpuReplayFailedViews",gpuReplayFailed},{"wavefrontValidated",waveFailed==0},{"wavefrontFailedViews",waveFailed},{"wavefrontMode","D: actual batched GPU intersections, CPU path state and packet assembly; W: identical fixed per-sample quotas with CPU intersections; B unchanged"},{"cameras",summary}};
    const auto bytes=QJsonDocument(report).toJson();if(json.write(bytes)!=bytes.size()||log.status()!=QTextStream::Ok)return 13;
    rayLog.flush();if(rayLog.status()!=QTextStream::Ok)return 22;
    waveLog.flush();waveDiffLog.flush();if(waveLog.status()!=QTextStream::Ok||waveDiffLog.status()!=QTextStream::Ok)return 23;
    if(gpuReplayFailed){qWarning()<<"A/B captures saved, but native GPU BVH replay not validated";return 21;}
    if(waveFailed){qWarning()<<"A/B captures saved, but GPU wavefront parity not validated";return 23;}
    return 0;
}
