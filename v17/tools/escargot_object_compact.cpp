#include "../escargot/object_compact.h"
namespace compact=m3d::escargot::object_compact;
#include "../escargot/object_visibility.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <set>
namespace o=m3d::escargot::object_opt;
namespace g=m3d::escargot::geometry_mask;
using Key=std::pair<uint32_t,uint32_t>;
struct Discovery{std::map<Key,std::map<Key,int>> pairs;uint64_t rays=0;double ms=0;};
static Discovery discover(const Camera& cam,const std::vector<e::Candidate>& catalogue,const std::set<uint32_t>& eligible,e::Traversal& trace,int w,int h){
    Discovery d;auto start=e::FrameClock::now();std::vector<uint8_t> sampled(w*h,0);
    auto detect=[&](int x,int y){
        int i=y*w+x;if(sampled[i]||d.rays+2>8192)return;sampled[i]=1;
        auto ray=e::screenRay(cam,x,y,w,h);e::HitSample a,b;++d.rays;
        if(!trace.closest(ray,.08f/std::max(1e-6f,dot(ray.d,cam.f)),a)||!eligible.count(a.object))return;
        ++d.rays;if(trace.closest({a.p,e::reflect(ray.d,a.geometricNormal)},eps(a.p),b))
            ++d.pairs[{a.object,b.object}][{a.primitive,b.primitive}];
    };
    for(int y=3;y<h;y+=7)for(int x=3;x<w;x+=7)detect(x,y);
    for(const auto& c:e::projectCandidates(catalogue,cam,w,h))for(int j=0;j<3;++j)for(int i=0;i<3;++i)
        detect(c.bounds.x+(c.bounds.w-1)*i/2,c.bounds.y+(c.bounds.h-1)*j/2);
    d.ms=e::elapsedMs(start);return d;
}

#include <QImage>
struct Variant{const char* name;bool crop,narrow,dense,compact,prepared;};
struct Result{
    double total=0,projection=0,visibilityMs=0;uint64_t rays=0,geometry=0,boxes=0,triangles=0,nodes=0,overflow=0;
    int testedPixels=0;uint64_t storageBytes=0; e::Rect rect{}; std::vector<Projection> p;std::vector<float> mask;
    int index(int x,int y)const{return rect.contains(x,y)?(y-rect.y)*rect.w+x-rect.x:-1;}
};
static Result run(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owners,e::Traversal& trace,
                  const Chart& rec,const Chart& src,const Camera& cam,int w,int h,Variant mode,const compact::PreparedChart& prec,const compact::PreparedChart& psrc){
    Result r;auto start=e::FrameClock::now();
    e::Rect rect=mode.crop?o::crop(s,rec,src,cam,w,h):e::Rect{0,0,w,h};
    r.rect=mode.compact?compact::aligned(rect,w,h):e::Rect{0,0,w,h};
    r.p.resize(size_t(r.rect.w)*r.rect.h);r.mask.assign(r.p.size(),-1);
    r.storageBytes=r.p.capacity()*sizeof(Projection)+r.mask.capacity()*sizeof(float);
    std::vector<int> domain;
    r.testedPixels=rect.w*rect.h;domain.reserve(r.testedPixels);
    for(int y=rect.y;y<rect.y+rect.h;++y)for(int x=rect.x;x<rect.x+rect.w;++x){
        int i=r.index(x,y);r.p[i]=mode.prepared?compact::projectPrepared(prec,psrc,cam,x,y,w,h):project(s,rec,src,cam,x,y,w,h);
        if(r.p[i].valid)domain.push_back(i);
    }
    r.projection=e::elapsedMs(start);auto vstart=e::FrameClock::now();
    if(!mode.dense){
        g::Stats gs;
        if(mode.narrow){auto ns=o::narrow(s,b,owners,rec,src,cam,r.p,r.rect.w,r.rect.h,r.mask,16,96);gs=ns.base;r.boxes=ns.segmentBoxTests;}
        else gs=g::classify(s,b,owners,rec,src,cam,r.p,r.rect.w,r.rect.h,r.mask,8,96);
        r.geometry=gs.clear+gs.blocked;r.triangles=gs.triangleTests;r.nodes=gs.nodes;r.overflow=gs.overflow;
    }
    // EXACT fallback for every variant, including baseline. No adaptive
    // guessing: equal quality at high resolution is the comparison contract.
    for(int i:domain)if(r.mask[i]<0)r.mask[i]=float(visibility(trace,s,rec,src,cam,r.p[i],r.rays));
    r.visibilityMs=e::elapsedMs(vstart);r.total=e::elapsedMs(start);return r;
}
struct Error{int coverage=0,mask=0,uv=0;};
static Error compare(const Result& a,const Result& b,int w,int h){
    Error e;
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        int i=a.index(x,y),j=b.index(x,y);
        bool av=i>=0&&a.p[i].valid,bv=j>=0&&b.p[j].valid;
        if(av!=bv){++e.coverage;continue;}if(!av)continue;
        if(a.mask[i]!=b.mask[j])++e.mask;
        if(std::abs(a.p[i].uv.x-b.p[j].uv.x)>1e-6f||std::abs(a.p[i].uv.y-b.p[j].uv.y)>1e-6f)++e.uv;
    }return e;
}
static QImage image(const Scene& s,const Chart& src,const Result& r,int w,int h){
    QImage im(w,h,QImage::Format_ARGB32);im.fill(0);
    for(int y=r.rect.y;y<r.rect.y+r.rect.h;++y)for(int x=r.rect.x;x<r.rect.x+r.rect.w;++x){
        int i=r.index(x,y);if(!r.p[i].valid||r.mask[i]!=1.f)continue;
        const V c=e::sourceTexture(s.mat[src.mat],r.p[i].uv);
        im.setPixel(x,y,qRgba(channel(c.x),channel(c.y),channel(c.z),255));
    }return im;
}
static double median(std::vector<double> x){std::sort(x.begin(),x.end());return x[x.size()/2];}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 2;
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 2;
    auto loaded=v16::loadScene(args[1]);if(!loaded)return 3;
    auto built=v16::buildRuntime(loaded.scene,256);if(!built)return 4;
    const auto& rt=built.runtime;const auto& s=rt.renderScene;std::vector<uint32_t> owners;
    if(!e::objectMap(rt,owners))return 5;e::Traversal trace(s,rt.lumaTraceBvh4,owners);if(!trace.valid())return 5;
    auto catalogue=e::staticCandidates(rt,loaded.scene,e::Mode::Reflection);std::set<uint32_t> eligible;
    for(const auto& c:catalogue)eligible.insert(c.object);
    const std::array<Variant,5> variants={{{"dense",false,false,true,false,false},{"combined",true,true,false,false,false},{"compact_only",true,true,false,true,false},{"prepared_only",true,true,false,false,true},{"compact_prepared",true,true,false,true,true}}};
    QJsonArray cases,skipped;int coverageErrors=0,maskErrors=0,uvErrors=0,caseCount=0,images=0;
    std::map<uint32_t,Chart> charts;std::map<const Chart*,compact::PreparedChart> prepared;
    double prepareMs=0;
    auto prepare=[&](const Chart& c)->const compact::PreparedChart&{auto it=prepared.find(&c);if(it!=prepared.end())return it->second;auto t=e::FrameClock::now();auto p=prepared.emplace(&c,compact::PreparedChart(s,c)).first;prepareMs+=e::elapsedMs(t);return p->second;};
    auto chartFor=[&](uint32_t id)->const Chart&{auto p=charts.find(id);if(p!=charts.end())return p->second;return charts.emplace(id,chart(s,owners,id)).first->second;};
    for(const QString cameraId:{QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==cameraId;});if(it==rt.cameras.end())continue;
        // Same selection used by the successful preceding sweep; independent
        // of the oracle and frozen across the tested resolutions/poses.
        auto initial=discover(v16::makeCamera(*it,320,180),catalogue,eligible,trace,320,180);
        std::vector<std::pair<int,Key>> ranked;
        for(const auto& p:initial.pairs){int sum=0;Key best{};int count=-1;
            for(const auto& q:p.second){sum+=q.second;if(q.second>count){count=q.second;best=q.first;}}ranked.push_back({sum,best});}
        std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.first!=b.first?a.first>b.first:a.second<b.second;});
        if(ranked.size()>3)ranked.resize(3);
        for(int w:{320,640,1280,1920}){const int h=w*9/16;
            for(int pose=0;pose<3;++pose){
                auto cp=*it;cp.eye.x+=.075f*pose;cp.target.x+=.075f*pose;auto cam=v16::makeCamera(cp,w,h);
                for(size_t ri=0;ri<ranked.size();++ri){
                    const auto& rec=chartFor(ranked[ri].second.first);const auto& src=chartFor(ranked[ri].second.second);
                    if(rec.tris.empty()||src.tris.empty())continue;
                    const auto& prec=prepare(rec);const auto& psrc=prepare(src);
                    // Untimed independent full reference used only for parity,
                    // never for scheduling or reconstructing a candidate.
                    auto oracle=run(s,rt.lumaTraceBvh4,owners,trace,rec,src,cam,w,h,variants[0],prec,psrc);
                    int projected=0,visible=0;for(size_t i=0;i<oracle.p.size();++i)if(oracle.p[i].valid){++projected;visible+=oracle.mask[i]==1.f;}
                    const QString receiver=rt.objects[int(rec.owner-1)].id,source=rt.objects[int(src.owner-1)].id;
                    if(!projected){skipped.append(QJsonObject{{"camera",cameraId},{"width",w},{"pose",pose},{"receiver",receiver},{"source",source}});continue;}
                    ++caseCount;
                    std::array<std::vector<double>,5> times,projection,visibilityTimes;
                    std::array<Result,5> last;std::array<Error,5> errors{};
                    // Warm each variant, then interleave three measured runs.
                    // Retain only metrics after each pass to avoid multiplying
                    // large full-resolution image allocations.
                    for(int rep=-1;rep<3;++rep){
                        for(size_t k=0;k<variants.size();++k){
                            size_t vi=(k+size_t(rep+1)+size_t(pose)+ri)%variants.size();
                            auto r=run(s,rt.lumaTraceBvh4,owners,trace,rec,src,cam,w,h,variants[vi],prec,psrc);
                            auto err=compare(r,oracle,w,h);errors[vi].coverage+=err.coverage;errors[vi].mask+=err.mask;errors[vi].uv+=err.uv;
                            if(rep>=0){times[vi].push_back(r.total);projection[vi].push_back(r.projection);visibilityTimes[vi].push_back(r.visibilityMs);}
                            if(rep==2&&vi==4&&pose==0&&(w==320||w==1920)){
                                const auto a=image(s,src,r,w,h),b=image(s,src,oracle,w,h);
                                QString p=cameraId+QString("-w%1-pair%2").arg(w).arg(int(ri+1));
                                if(!a.save(out.filePath(p+"-compact.png"))||!b.save(out.filePath(p+"-dense.png")))return 6;
                                images+=2;if(a!=b)++errors[vi].uv;
                            }
                            r.p.clear();r.mask.clear();last[vi]=std::move(r);
                        }
                    }
                    for(size_t vi=0;vi<variants.size();++vi){
                        const auto& r=last[vi];const auto& err=errors[vi];
                        coverageErrors+=err.coverage;maskErrors+=err.mask;uvErrors+=err.uv;
                        cases.append(QJsonObject{{"camera",cameraId},{"width",w},{"height",h},{"pose",pose},{"receiver",receiver},{"source",source},
                            {"variant",variants[vi].name},{"projected",projected},{"visible",visible},{"testedPixels",r.testedPixels},{"storageBytes",double(r.storageBytes)},
                            {"fallbackRays",double(r.rays)},{"geometryResolved",double(r.geometry)},{"localTriangleTests",double(r.triangles)},
                            {"segmentBoxTests",double(r.boxes)},{"nodeVisits",double(r.nodes)},{"overflowLists",double(r.overflow)},
                            {"medianTotalMs",median(times[vi])},{"medianProjectionMs",median(projection[vi])},{"medianVisibilityMs",median(visibilityTimes[vi])},
                            {"coverageErrors",err.coverage},{"maskErrors",err.mask},{"uvErrors",err.uv}});
                    }
                }
            }
        }
    }
    QJsonObject report{{"pairPoseResolutionCases",caseCount},{"measuredRuns",caseCount*5*3},{"coverageErrors",coverageErrors},{"maskErrors",maskErrors},{"uvErrors",uvErrors},{"images",images},{"chartPreparationMs",prepareMs},{"cases",cases},{"skipped",skipped},
        {"note","Compact footprint storage and cached triangle coefficients against validated combined path and dense BVH. Exact fallback. Timings include projection allocation and visibility, not discovery or chart preparation (separately reported). Storage counts projection/mask vector capacities only, not entire process memory. Diagnostic only; no final GPU rendering."}};
    QFile f(out.filePath("compact-summary.json"));if(!f.open(QIODevice::WriteOnly))return 7;f.write(QJsonDocument(report).toJson());f.close();
    return caseCount>0&&!coverageErrors&&!maskErrors&&!uvErrors?0:8;
}
