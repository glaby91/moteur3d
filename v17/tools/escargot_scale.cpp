#include "../escargot/gpu_packet.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTextStream>
#include <QDebug>
#include <chrono>
#include <cstdlib>
namespace e=m3d::escargot;
using Clock=std::chrono::steady_clock;
static double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
static double quantile(std::vector<double> a,double q){if(a.empty())return 0;std::sort(a.begin(),a.end());return a[size_t(std::ceil(q*a.size()))-1];}
static bool save(const QDir& out,const QString& file,const std::vector<uint32_t>& pixels,int w,int h){
    return pixels.size()==size_t(w)*h&&QImage(reinterpret_cast<const uchar*>(pixels.data()),w,h,w*4,QImage::Format_RGB32).save(out.filePath(file));
}
static QJsonArray xyz(double x,double y,double z){return {x,y,z};}
static m3d::v16::SceneDatabase synthetic(int n,double aspect){
    m3d::v16::SceneDatabase db;db.name="Escargot scale lab";db.revision=QString("scale-%1").arg(n);
    m3d::v16::Sector sector;sector.id="scale";
    QJsonArray mats,meshes,instances;
    auto material=[&](QString id,QJsonArray color,double metal,double trans,double rough,bool optical){
        QJsonObject m{{"id",id},{"baseColor",color},{"metallic",metal},{"transmission",trans},{"roughness",rough},{"ior",1.5},{"textureSet","pbr"},{"surfaceModel","standard"},{"planarReflection",optical?"enabled":"disabled"}};
        if(id.startsWith("source"))m.insert("emission",QJsonObject{{"color",color},{"intensity",.7}});
        mats.append(m);
    };
    material("optical",xyz(.8,.85,.9),.6,.35,.08,true);
    material("background",xyz(.14,.15,.16),0,0,.9,false);
    material("source_red",xyz(.9,.1,.07),0,0,.75,false);
    material("source_blue",xyz(.05,.25,.9),0,0,.75,false);
    material("source_yellow",xyz(.85,.75,.1),0,0,.75,false);
    meshes.append(QJsonObject{{"id","quad"},{"type","triangle_mesh"},{"vertices",QJsonArray{xyz(-.5,-.5,0),xyz(-.5,.5,0),xyz(.5,.5,0),xyz(.5,-.5,0)}},{"faces",QJsonArray{QJsonArray{0,1,2},QJsonArray{0,2,3}}},{"uv",QJsonArray{QJsonArray{0,0},QJsonArray{0,1},QJsonArray{1,1},QJsonArray{1,0}}}});
    auto add=[&](QString id,QString material,double x,double y,double z,double w,double h,bool mirror){
        instances.append(QJsonObject{{"id",id},{"geometry","quad"},{"material",material},{"position",xyz(x,y,z)},{"size",xyz(w,h,1)},{"rotationY",id.startsWith("source_")?180:0},{"planarReflection",mirror?"enabled":"disabled"}});
    };
    const int cols=n==1?1:n==5?5:n==10?5:n==20?5:10,rows=n/cols;
    const double tanHalf=std::tan(55.*3.141592653589793/360.);
    // Exact constant projected coverage in the fixed central camera:
    // span +/-0.85 NDC, fill 92% in each grid cell, vary depth to prevent
    // Z Reflect merging coplanar panels into a single virtual camera.
    for(int i=0;i<n;++i){
        const double z=10.+.04*i,dx=1.7/cols,dy=1.7/rows;
        add(QString("panel_%1").arg(i),"optical",(-.85+(i%cols+.5)*dx)*z*tanHalf,
            (-.85+(i/cols+.5)*dy)*z*tanHalf/aspect,z,dx*.92*z*tanHalf,dy*.92*z*tanHalf/aspect,true);
    }
    add("backdrop","background",0,0,17,40,30,false);
    // Source wall behind the camera, wide enough for all mirror rays.
    // Three distinct colored stripes to expose UV/projection anomalies.
    const QString sourceMaterials[]={"source_red","source_blue","source_yellow"};
    for(int i=0;i<9;++i)add(QString("source_%1").arg(i),sourceMaterials[i%3],(i-4)*12.,0,-4,12,50,false);
    auto resource=[&](QString kind,QString key,QJsonArray data){sector.resources[kind].append(m3d::v16::Resource{kind+".json",QJsonObject{{key,data}}});};
    resource("geometry","meshes",meshes);resource("materials","materials",mats);resource("instances","instances",instances);
    resource("pbr","sets",QJsonArray{QJsonObject{{"id","pbr"},{"resolution",64},{"pattern","plaster"}}});
    resource("cameras","cameras",QJsonArray{QJsonObject{{"id","scale_camera"},{"position",xyz(0,0,0)},{"target",xyz(0,0,10)},{"fovDegrees",55}}});
    db.sectors.append(sector);return db;
}
struct Metrics {
    std::vector<double> wall,gpu,build;
    std::vector<uint32_t> pixels;
    uint32_t active=0,candidates=0,deferred=0,rays=0,complete=0,unknown=0,withLayers=0;
    uint64_t planBytes=0;
};
static bool runView(m3d::v16::Runtime& rt,const m3d::v16::SceneDatabase& db,const m3d::Camera& cam,const QDir& out,
    QString id,int n,int w,int h,int reps,const e::Options& opt,QTextStream& log,QTextStream& coverage,QJsonArray& summary){
    auto& scene=rt.renderScene;e::Prepared session(rt,db,opt.mode);
    m3d::updateLighting(rt.daylightHour);m3d::Stats ss;const auto shadow=m3d::buildShadow(scene,ss);
    const auto candidates=e::projectCandidates(e::staticCandidates(rt,db,opt.mode),cam,w,h);
    std::wstring err;std::vector<uint32_t> control;std::vector<float> empty(8,0.f);m3d::Stats controlStats;
    if(!m3d::renderLumaTraceEscargotGpuPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,empty,control,controlStats,err)||control.size()!=size_t(w)*h)return false;
    Metrics stats[3];const QString names[]={"A","F32","F16"};
    auto run=[&](int mode,int iteration,bool record){
        auto& x=stats[mode];m3d::Stats st;m3d::EscargotBuildTiming bt;double pathMs=0,planMs=0;
        const auto start=Clock::now();
        if(mode==0){
            if(!m3d::renderLumaTracePreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,x.pixels,st,err)){qCritical()<<QString::fromStdWString(err);return false;}
            x.candidates=st.zReflectCandidates;x.deferred=st.zReflectDeferred;x.active=st.zReflectActive;
        }else{
            e::ResidentStats rs;auto frame=session.frame(cam,w,h,opt,false,true,true,nullptr,&rs);pathMs=ms(start);
            if(!frame.ok){qCritical()<<QString::fromStdString(frame.error);return false;}
            const auto planStart=Clock::now();auto plan=e::planGpuBuild(frame,scene,rt.objects.size(),mode==1?32:16,mode==2);planMs=ms(planStart);
            if(!plan.ok){qCritical()<<QString::fromStdString(plan.error);return false;}
            if(!m3d::renderLumaTraceEscargotGpuBuiltPreview(scene,rt.lumaTraceBvh4,shadow,cam,w,h,plan.native,x.pixels,st,bt,err)){qCritical()<<QString::fromStdWString(err);return false;}
            x.candidates=frame.candidates;x.deferred=frame.deferred;x.active=st.zReflectActive;x.rays=frame.rays;x.complete=frame.completePaths;x.unknown=frame.unknownPaths;
            x.withLayers=0;for(const auto& s:frame.surfaces)x.withLayers+=!s.layers.empty();
            x.planBytes=plan.native.data.size()*sizeof(float);
            if(x.active||x.rays>opt.rayBudget)return false;
        }
        const double total=ms(start);
        if(x.pixels.size()!=control.size())return false;
        if(record){
            x.wall.push_back(total);if(st.gpuTimingValid)x.gpu.push_back(st.gpuTotalMs);if(bt.gpuTimingValid)x.build.push_back(bt.gpuMs);
            log<<id<<','<<n<<','<<iteration<<','<<names[mode]<<','<<total<<','<<st.gpuTotalMs<<','<<int(st.gpuTimingValid)<<','<<pathMs<<','<<planMs<<','<<bt.gpuMs<<','<<int(bt.gpuTimingValid)<<','<<x.active<<','<<x.candidates<<','<<x.deferred<<','<<x.withLayers<<','<<x.rays<<','<<x.complete<<','<<x.unknown<<','<<qulonglong(x.planBytes)<<'\n';
        }
        return true;
    };
    for(int m=0;m<3;++m)if(!run(m,-1,false))return false;
    // Six permutations balance position and immediate predecessor when reps is a multiple of six.
    const int orders[6][3]={{0,1,2},{1,2,0},{2,0,1},{0,2,1},{2,1,0},{1,0,2}};
    bool parity=stats[1].pixels==stats[2].pixels;
    for(int j=0;j<reps;++j){for(int m:orders[j%6])if(!run(m,j,true))return false;
        parity=parity&&stats[1].pixels==stats[2].pixels;}
    QJsonArray result;
    for(int m=0;m<3;++m){
        auto& x=stats[m];uint64_t changed=0,absolute=0;int maxError=0;
        for(size_t i=0;i<control.size();++i){
            const auto a=control[i],b=x.pixels[i];changed+=((a&0xffffffu)!=(b&0xffffffu));
            for(int shift:{0,8,16}){int d=std::abs(int((a>>shift)&255)-int((b>>shift)&255));absolute+=d;maxError=std::max(maxError,d);}
        }
        int panelsWithSignal=0;uint64_t maskPixels=0,signalPixels=0;
        // Synthetic panels are unobstructed and nonoverlapping in this fixed
        // camera. AABB screen bounds are exact up to rounding; exclude one
        // pixel at each edge. For real scene these are NOT visibility masks.
        if(n>0)for(const auto& c:candidates){
            uint64_t count=0,signal=0;
            for(int y=c.bounds.y+1;y<c.bounds.y+c.bounds.h-1;++y)for(int px=c.bounds.x+1;px<c.bounds.x+c.bounds.w-1;++px){
                ++count;const size_t i=size_t(y)*w+px;signal+=((control[i]&0xffffffu)!=(x.pixels[i]&0xffffffu));
            }
            maskPixels+=count;signalPixels+=signal;panelsWithSignal+=signal>0;
            coverage<<id<<','<<n<<','<<names[m]<<','<<c.object<<','<<qulonglong(count)<<','<<qulonglong(signal)<<'\n';
        }
        result.append(QJsonObject{{"mode",names[m]},{"p50_ms",quantile(x.wall,.5)},{"p95_ms",quantile(x.wall,.95)},
            {"gpu_p50_ms",quantile(x.gpu,.5)},{"gpu_samples",int(x.gpu.size())},{"build_gpu_p50_ms",quantile(x.build,.5)},
            {"secondary_cameras",int(x.active)},{"candidates",int(x.candidates)},{"deferred",int(x.deferred)},{"surfaces_with_layers",int(x.withLayers)},
            {"rays",int(x.rays)},{"complete_paths",int(x.complete)},{"unknown_paths",int(x.unknown)},{"plan_bytes",double(x.planBytes)},
            {"changed_pixels_vs_empty",double(changed)},{"mean_abs_channel_delta_vs_empty",double(absolute)/(3*control.size())},{"max_channel_delta",maxError},
            {"synthetic_panel_signal_count",panelsWithSignal},{"synthetic_mask_pixels",double(maskPixels)},{"synthetic_signal_pixels",double(signalPixels)}});
        if(!save(out,id+"-"+names[m]+".png",x.pixels,w,h))return false;
        qInfo()<<id<<names[m]<<"p50"<<quantile(x.wall,.5)<<"candidates"<<x.candidates<<"cameras"<<x.active<<"signal panels"<<panelsWithSignal;
    }
    if(!save(out,id+"-empty.png",control,w,h))return false;
    const bool setupOk=n<=0||(int(candidates.size())==n&&int(scene.opticalPlanes.size())==n&&stats[0].active==uint32_t(std::min(n,2)));
    summary.append(QJsonObject{{"view",id},{"requested_panels",n},{"catalog_planes",int(scene.opticalPlanes.size())},{"escargot_projected_candidates",int(candidates.size())},{"setup_valid",setupOk},{"F32_F16_pixel_parity",parity},{"F32_vs_A_time_reduction_percent",100.*(1.-quantile(stats[1].wall,.5)/std::max(.000001,quantile(stats[0].wall,.5)))},{"F16_vs_A_time_reduction_percent",100.*(1.-quantile(stats[2].wall,.5)/std::max(.000001,quantile(stats[0].wall,.5)))},{"modes",result}});
    log.flush();coverage.flush();
    return setupOk&&parity;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()<2||args.size()>7){qCritical()<<"Usage: v17_escargot_scale outputDir [width] [height] [repeats] [rays] [optional scene.json]";return 2;}
    bool ok=true;int w=args.size()>2?args[2].toInt(&ok):1280;if(!ok||w<320||w>2048)return 2;
    int h=args.size()>3?args[3].toInt(&ok):720;if(!ok||h<180||h>2048)return 2;
    int reps=args.size()>4?args[4].toInt(&ok):12;if(!ok||reps<1||reps>120)return 2;
    e::Options opt;if(args.size()>5)opt.rayBudget=args[5].toUInt(&ok);if(!ok||!e::validOptions(opt))return 2;
    const QDir out(args[1]);if(!QDir().mkpath(out.absolutePath()))return 3;
    _putenv_s("M3D_V17_ZREFLECT","1");_putenv_s("M3D_V17_ZATMOSPHERE","0");
    m3d::setRasterPeripheralMultiresOverride(0);m3d::setRasterCloudsEnabled(false);m3d::setRasterHybridLightingEnabled(true);
    m3d::setRasterColorEnergyBalanceEnabled(true);m3d::setRasterFilmicTonemapEnabled(true);m3d::setRasterColorExposure(.85f);
    auto quality=m3d::rasterQualityProfile(m3d::RasterQualityProfile::High);quality.msaaSamples=1;quality.dynamicLod=false;
    std::wstring err;if(!m3d::setRasterQualitySettings(quality,err))return 6;
    QFile csv(out.filePath("scale_frames.csv")),cov(out.filePath("scale_panel_coverage.csv"));
    if(!csv.open(QIODevice::WriteOnly|QIODevice::Text)||!cov.open(QIODevice::WriteOnly|QIODevice::Text))return 7;
    QTextStream log(&csv),coverage(&cov);
    log<<"view,panels,iteration,mode,wall_ms,gpu_ms,gpu_valid,path_ms,plan_ms,build_gpu_ms,build_gpu_valid,secondary_cameras,candidates,deferred,surfaces_with_layers,rays,complete_paths,unknown_paths,plan_bytes\n";
    coverage<<"view,panels,mode,receiver_id,interior_pixels,changed_vs_empty\n";
    QJsonArray summary;int failed=0,realViews=0;
    for(int n:{1,5,10,20,50}){
        auto db=synthetic(n,double(w)/h);db.rootPath=out.absolutePath();
        auto built=m3d::v16::buildRuntime(db,256);
        if(!built||!built.runtime.lumaTraceBvhError.isEmpty()){qCritical()<<built.error<<built.runtime.lumaTraceBvhError;return 5;}
        auto& rt=built.runtime;if(!e::objectMap(rt,rt.renderScene.escargotReceiverIds)||rt.cameras.empty())return 5;
        rt.renderScene.markGpuGeometryChanged();
        if(!runView(rt,db,m3d::v16::makeCamera(rt.cameras.front(),w,h),out,QString("panels-%1").arg(n),n,w,h,reps,opt,log,coverage,summary))++failed;
    }
    if(args.size()==7){
        auto loaded=m3d::v16::loadScene(args[6]);if(!loaded){qCritical()<<loaded.error;return 4;}
        auto built=m3d::v16::buildRuntime(loaded.scene,256);if(!built||!built.runtime.lumaTraceBvhError.isEmpty())return 5;
        auto& rt=built.runtime;if(!e::objectMap(rt,rt.renderScene.escargotReceiverIds))return 5;rt.renderScene.markGpuGeometryChanged();
        for(const QString id:{QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
            auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==id;});
            if(it==rt.cameras.end()){qWarning()<<"Missing real camera"<<id;++failed;continue;}
            ++realViews;if(!runView(rt,loaded.scene,m3d::v16::makeCamera(*it,w,h),out,id,0,w,h,reps,opt,log,coverage,summary))++failed;
        }
    }
    QFile json(out.filePath("summary.json"));if(!json.open(QIODevice::WriteOnly))return 7;
    json.write(QJsonDocument(QJsonObject{{"width",w},{"height",h},{"repeats",reps},{"ray_budget",int(opt.rayBudget)},{"force_warp",qEnvironmentVariable("M3D_V16_FORCE_WARP")},{"real_views",realViews},{"failed",failed},{"coverage_note","Changed RGB vs empty is signal, not physical accuracy; synthetic panel interior only; real scene global pixels only; A capped at 2 virtual cameras"},{"views",summary}}).toJson());
    return failed?26:0;
}
