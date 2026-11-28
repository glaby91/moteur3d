#include "../escargot/object_global.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <stdexcept>
namespace og=m3d::escargot::object_global;
struct PixelHit{uint32_t index;og::WorkRay ray;e::HitSample receiver;};
struct Frame{
    std::vector<e::HitSample> reflected;
    std::vector<uint32_t> receiver;
    uint64_t primaryRays=0,secondaryRays=0,discoveryRays=0;
    og::Counters primary{},secondary{};
    double primaryMs=0,discoveryMs=0,secondaryMs=0,totalMs=0;
};
struct SeedCache{
    std::map<uint32_t,std::vector<uint32_t>> byReceiver;
    void add(uint32_t receiver,uint32_t primitive){
        auto& a=byReceiver[receiver];if(std::find(a.begin(),a.end(),primitive)!=a.end())return;
        if(a.size()==32)a.erase(a.begin());a.push_back(primitive);
    }
};
static Frame render(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owners,
                    const std::set<uint32_t>& eligible,e::Traversal& scalar,const Camera& cam,int w,int h,
                    int mode,SeedCache& cache){
    Frame frame;auto start=e::FrameClock::now();
    frame.reflected.resize(size_t(w)*h);frame.receiver.resize(size_t(w)*h);
    std::vector<PixelHit> work;work.reserve(size_t(w)*h/4);
    og::Packet packet(s,b,owners);
    // This primary visibility pass will later be supplied by the raster
    // G-buffer. Here it is independent and included in the CPU timing.
    auto record=[&](uint32_t i,const Ray& r,const e::HitSample& hit){
        if(!eligible.count(hit.object))return;
        frame.receiver[i]=hit.object;
        work.push_back({i,{{hit.p,e::reflect(r.d,hit.geometricNormal)},eps(hit.p)},hit});
    };
    constexpr int T=4;
    std::vector<og::WorkRay> rays;std::vector<uint32_t> indices;std::vector<e::HitSample> hits;
    for(int y=0;y<h;y+=T)for(int x=0;x<w;x+=T){
        rays.clear();indices.clear();
        for(int yy=y;yy<std::min(h,y+T);++yy)for(int xx=x;xx<std::min(w,x+T);++xx){
            auto r=e::screenRay(cam,xx,yy,w,h);rays.push_back({r,.08f/std::max(1e-6f,dot(r.d,cam.f))});indices.push_back(uint32_t(yy*w+xx));
        }
        if(mode==3){packet.trace(rays,{},hits,frame.primary);for(size_t j=0;j<rays.size();++j)record(indices[j],rays[j].ray,hits[j]);}
        else for(size_t j=0;j<rays.size();++j){e::HitSample h;++frame.primaryRays;scalar.closest(rays[j].ray,rays[j].low,h);record(indices[j],rays[j].ray,h);}
    }
    frame.primaryMs=e::elapsedMs(start);auto t=e::FrameClock::now();
    // Sparse discovery is a warm start. Geometry is never excluded by it.
    // Even a wholly new object can win the exhaustive nearest-hit search.
    if(mode>=2){
        for(const auto& p:work){int x=int(p.index%w),y=int(p.index/w);if(x%16!=8||y%16!=8)continue;
            e::HitSample h;++frame.discoveryRays;
            if(scalar.closest(p.ray.ray,p.ray.low,h))cache.add(p.receiver.object,h.primitive);
        }
    }
    frame.discoveryMs=e::elapsedMs(t);t=e::FrameClock::now();
    if(mode==0){
        for(const auto& p:work){++frame.secondaryRays;scalar.closest(p.ray.ray,p.ray.low,frame.reflected[p.index]);}
    }else{
        // Group by original screen tile and receiver; no independent
        // reconstruction or interpolation at the tile boundary.
        for(size_t at=0;at<work.size();){
            const auto& first=work[at];uint32_t tile=(first.index/w/T)*uint32_t((w+T-1)/T)+(first.index%w/T);
            size_t end=at+1;
            while(end<work.size()){auto i=work[end].index;uint32_t k=(i/w/T)*uint32_t((w+T-1)/T)+(i%w/T);if(k!=tile)break;++end;}
            std::map<uint32_t,std::vector<size_t>> groups;
            for(size_t j=at;j<end;++j)groups[work[j].receiver.object].push_back(j);
            for(const auto& group:groups){
                rays.clear();for(auto j:group.second)rays.push_back(work[j].ray);
                std::vector<uint32_t> seeds;
                if(mode>=2){auto found=cache.byReceiver.find(group.first);if(found!=cache.byReceiver.end())seeds=found->second;}
                packet.trace(rays,seeds,hits,frame.secondary);
                for(size_t j=0;j<hits.size();++j)frame.reflected[work[group.second[j]].index]=hits[j];
            }at=end;
        }
    }
    frame.secondaryMs=e::elapsedMs(t);frame.totalMs=e::elapsedMs(start);return frame;
}
static QImage texture(const Scene& s,const Frame& f,int w,int h){
    QImage im(w,h,QImage::Format_ARGB32);im.fill(0);
    for(size_t i=0;i<f.reflected.size();++i){const auto& hit=f.reflected[i];if(!hit.object)continue;
        V color=e::sourceTexture(s.mat[s.obj[hit.primitive].mat],hit.uv);im.setPixel(int(i%w),int(i/w),qRgba(channel(color.x),channel(color.y),channel(color.z),255));
    }return im;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 2;
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 2;
    auto loaded=v16::loadScene(args[1]);if(!loaded)return 3;auto built=v16::buildRuntime(loaded.scene,256);if(!built)return 4;
    const auto& rt=built.runtime;const auto& s=rt.renderScene;std::vector<uint32_t> owners;
    if(!e::objectMap(rt,owners))return 5;e::Traversal scalar(s,rt.lumaTraceBvh4,owners);if(!scalar.valid())return 5;
    std::set<uint32_t> eligible;for(const auto& c:e::staticCandidates(rt,loaded.scene,e::Mode::Reflection))eligible.insert(c.object);
    QJsonArray cases;int errors=0,caseCount=0,images=0;const char* names[]={"scalar","packet","seeded","packet_primary_seeded"};
    for(const QString cameraId:{QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==cameraId;});if(it==rt.cameras.end())continue;
        for(int w:{320,640,1280}){int h=w*9/16;
            std::array<SeedCache,4> caches;
            for(int pose=0;pose<3;++pose){
                auto cp=*it;cp.eye.x+=.075f*pose;cp.target.x+=.075f*pose;auto cam=v16::makeCamera(cp,w,h);
                SeedCache unused;auto oracle=render(s,rt.lumaTraceBvh4,owners,eligible,scalar,cam,w,h,0,unused);
                std::set<uint32_t> sources,receivers;std::map<uint32_t,std::set<uint32_t>> faces;int visible=0,receiverPixels=0;
                for(size_t i=0;i<oracle.reflected.size();++i){
                    if(oracle.receiver[i]){receivers.insert(oracle.receiver[i]);++receiverPixels;}
                    auto h=oracle.reflected[i];if(h.object){sources.insert(h.object);faces[h.object].insert(h.primitive);++visible;}
                }
                int multiPrimitive=0;for(const auto& f:faces)multiPrimitive+=f.second.size()>1;
                if(!receiverPixels)continue;++caseCount;
                for(int mode=0;mode<4;++mode){
                    auto initial=caches[mode];std::vector<double> times,primary,secondary,discovery;
                    Frame last;int receiverErr=0,objectErr=0,primitiveErr=0,uvErr=0,depthErr=0;
                    for(int rep=-1;rep<3;++rep){
                        // Every repetition starts with the same previous-frame
                        // seeds, not with data from its own oracle or warmup.
                        auto cache=initial;auto f=render(s,rt.lumaTraceBvh4,owners,eligible,scalar,cam,w,h,mode,cache);
                        for(size_t i=0;i<f.reflected.size();++i){
                            const auto& a=f.reflected[i];const auto& b=oracle.reflected[i];
                            receiverErr+=f.receiver[i]!=oracle.receiver[i];objectErr+=a.object!=b.object;
                            if(!a.object||!b.object)continue;
                            primitiveErr+=a.primitive!=b.primitive;
                            uvErr+=std::abs(a.uv.x-b.uv.x)>1e-6f||std::abs(a.uv.y-b.uv.y)>1e-6f;
                            depthErr+=std::abs(a.depth-b.depth)>1e-5f;
                        }
                        if(rep>=0){times.push_back(f.totalMs);primary.push_back(f.primaryMs);secondary.push_back(f.secondaryMs);discovery.push_back(f.discoveryMs);}
                        if(rep==2){last=std::move(f);caches[mode]=std::move(cache);}
                    }
                    auto median=[](std::vector<double> x){std::sort(x.begin(),x.end());return x[x.size()/2];};
                    if(mode==3&&pose==0){
                        auto a=texture(s,last,w,h),b=texture(s,oracle,w,h);if(a!=b)++uvErr;
                        QString p=cameraId+QString("-w%1").arg(w);
                        if(!a.save(out.filePath(p+"-global.png"))||!b.save(out.filePath(p+"-scalar.png")))return 6;images+=2;
                    }
                    errors+=receiverErr+objectErr+primitiveErr+uvErr+depthErr;
                    cases.append(QJsonObject{{"camera",cameraId},{"width",w},{"pose",pose},{"variant",names[mode]},
                        {"receiverPixels",receiverPixels},{"reflectedPixels",visible},{"receivers",int(receivers.size())},{"sources",int(sources.size())},{"multiPrimitiveSources",multiPrimitive},
                        {"totalMs",median(times)},{"primaryMs",median(primary)},{"secondaryMs",median(secondary)},{"discoveryMs",median(discovery)},
                        {"primaryRays",double(last.primaryRays)},{"secondaryRays",double(last.secondaryRays)},{"discoveryRays",double(last.discoveryRays)},
                        {"primaryPackets",double(last.primary.packets)},{"secondaryPackets",double(last.secondary.packets)},
                        {"nodeLoads",double(last.primary.nodeLoads+last.secondary.nodeLoads)},{"boxTests",double(last.primary.boxTests+last.secondary.boxTests)},
                        {"triangleTests",double(last.primary.triangleTests+last.secondary.triangleTests)},{"seedTests",double(last.secondary.seedTests)},{"seedHits",double(last.secondary.seedHits)},
                        {"receiverErrors",receiverErr},{"objectErrors",objectErr},{"primitiveErrors",primitiveErr},{"uvErrors",uvErr},{"depthErrors",depthErr}});
                }
            }
        }
    }
    QJsonObject report{{"frameCases",caseCount},{"measuredRuns",caseCount*4*3},{"errors",errors},{"images",images},{"cases",cases},
        {"note","Global first-hit reflection diagnostic, all eligible visible receivers and all source mesh triangles, no pair cap. Exact shared packet BVH traversal with optional sparse geometric warm start. Seeds cannot exclude new objects. CPU primary pass included; no GPU integration, no lighting, no transparent multilayer transport. Timings exclude texture PNG rendering."}};
    QFile f(out.filePath("global-summary.json"));if(!f.open(QIODevice::WriteOnly))return 7;f.write(QJsonDocument(report).toJson());f.close();
    return caseCount>0&&!errors?0:8;
}
