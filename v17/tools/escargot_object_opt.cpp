#include "../escargot/object_opt.h"
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
struct Variant {const char* name;int tile,cap;bool narrow;};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 2;
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 2;
    auto loaded=v16::loadScene(args[1]);if(!loaded)return 3;auto built=v16::buildRuntime(loaded.scene,256);if(!built)return 4;
    const auto& rt=built.runtime;const auto& s=rt.renderScene;std::vector<uint32_t> owners;
    if(!e::objectMap(rt,owners))return 5;e::Traversal trace(s,rt.lumaTraceBvh4,owners);if(!trace.valid())return 5;
    auto catalogue=e::staticCandidates(rt,loaded.scene,e::Mode::Reflection);std::set<uint32_t> eligible;
    for(const auto& c:catalogue)eligible.insert(c.object);
    const std::array<Variant,9> variants={{{"baseline",8,96,false},{"tile4",4,96,false},{"tile16",16,96,false},{"cap32",8,32,false},{"cap256",8,256,false},{"cap512",8,512,false},{"narrow96",8,96,true},{"narrow256",8,256,true},{"narrow16",16,96,true}}};
    constexpr int W=320,H=180;QJsonArray results,reuse,meshResults,cropResults;int certErrors=0,cropErrors=0,meshErrors=0,tested=0;
    std::map<uint32_t,Chart> charts;
    auto chartFor=[&](uint32_t id)->const Chart&{auto p=charts.find(id);if(p!=charts.end())return p->second;return charts.emplace(id,chart(s,owners,id)).first->second;};
    for(const QString cameraId:{QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==cameraId;});if(it==rt.cameras.end())continue;
        auto cam=v16::makeCamera(*it,W,H);auto initial=discover(cam,catalogue,eligible,trace,W,H);
        std::vector<std::pair<int,Key>> ranked;for(const auto& p:initial.pairs){int sum=0;Key best{};int count=-1;
            for(const auto& q:p.second){sum+=q.second;if(q.second>count){count=q.second;best=q.first;}}ranked.push_back({sum,best});}
        std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.first!=b.first?a.first>b.first:a.second<b.second;});
        // Candidate selection never uses dense reference. Same rank cutoff as pass 4.
        if(ranked.size()>3)ranked.resize(3);
        for(int pose=0;pose<3;++pose){
            auto cp=*it;cp.eye.x+=.075f*pose;cp.target.x+=.075f*pose;cam=v16::makeCamera(cp,W,H);
            auto fresh=pose?discover(cam,catalogue,eligible,trace,W,H):initial;
            int found=0,miss=0,hits=0,missHits=0,stale=0;
            for(const auto& p:fresh.pairs){bool old=initial.pairs.count(p.first);found+=old;miss+=!old;for(const auto& q:p.second){hits+=q.second;if(!old)missHits+=q.second;}}
            for(const auto& p:initial.pairs)stale+=!fresh.pairs.count(p.first);
            reuse.append(QJsonObject{{"camera",cameraId},{"pose",pose},{"initialPairs",int(initial.pairs.size())},{"freshPairs",int(fresh.pairs.size())},{"reusedPairs",found},{"newPairs",miss},{"stalePairs",stale},{"freshHits",hits},{"newPairHits",missHits},{"freshQueries",double(fresh.rays)},{"freshMs",fresh.ms}});
            for(const auto& rank:ranked){
                const auto& rec=chartFor(rank.second.first);const auto& src=chartFor(rank.second.second);if(rec.tris.empty()||src.tris.empty())continue;
                QString receiver=rt.objects[int(rec.owner-1)].id,source=rt.objects[int(src.owner-1)].id;
                auto t=e::FrameClock::now();std::vector<Projection> full(W*H);
                for(int y=0;y<H;++y)for(int x=0;x<W;++x)full[y*W+x]=project(s,rec,src,cam,x,y,W,H);
                double fullMs=e::elapsedMs(t);t=e::FrameClock::now();auto rect=o::crop(s,rec,src,cam,W,H);
                std::vector<Projection> cropped(W*H);for(int y=rect.y;y<rect.y+rect.h;++y)for(int x=rect.x;x<rect.x+rect.w;++x)cropped[y*W+x]=project(s,rec,src,cam,x,y,W,H);
                double cropMs=e::elapsedMs(t);std::vector<int> domain;int wrong=0;
                for(int i=0;i<W*H;++i){if(full[i].valid)domain.push_back(i);wrong+=full[i].valid!=cropped[i].valid;}
                cropErrors+=wrong;cropResults.append(QJsonObject{{"camera",cameraId},{"pose",pose},{"receiver",receiver},{"source",source},{"fullPixels",W*H},{"cropPixels",rect.w*rect.h},{"projected",int(domain.size())},{"mismatch",wrong},{"fullMs",fullMs},{"cropMs",cropMs}});
                if(domain.empty())continue;++tested;
                std::vector<int> truth(W*H,-1);uint64_t denseRays=0;t=e::FrameClock::now();
                for(int i:domain)truth[i]=visibility(trace,s,rec,src,cam,full[i],denseRays);
                double denseMs=e::elapsedMs(t);
                // Rotate order between poses. Warm each variant once, then report
                // median of three repetitions for geometry + fallback together.
                for(size_t vi=0;vi<variants.size();++vi){
                    const auto& v=variants[(vi+pose)%variants.size()];std::vector<double> times;
                    g::Stats gs;uint64_t boxTests=0,rays=0;int fp=0,fn=0,unknown=0,geoWrong=0;
                    for(int rep=-1;rep<3;++rep){
                        std::vector<float> mask(W*H,-1);t=e::FrameClock::now();
                        if(v.narrow){auto r=o::narrow(s,rt.lumaTraceBvh4,owners,rec,src,cam,full,W,H,mask,v.tile,v.cap);gs=r.base;boxTests=r.segmentBoxTests;}
                        else{gs=g::classify(s,rt.lumaTraceBvh4,owners,rec,src,cam,full,W,H,mask,v.tile,v.cap);boxTests=0;}
                        std::vector<int> unresolved;for(int i:domain)if(mask[i]<0)unresolved.push_back(i);
                        geoWrong=0;for(int i:domain)if(mask[i]>=0&&int(mask[i])!=truth[i])++geoWrong;
                        rays=0;e::object_visibility::reconstruct(W,unresolved,512,[&](int i){return visibility(trace,s,rec,src,cam,full[i],rays);},mask);
                        double ms=e::elapsedMs(t);if(rep>=0)times.push_back(ms);
                        fp=fn=unknown=0;for(int i:domain){float a=mask[i];if(a<0)++unknown;else{fp+=a>=.5f&&!truth[i];fn+=a<.5f&&truth[i];}}
                    }
                    std::sort(times.begin(),times.end());certErrors+=geoWrong;
                    results.append(QJsonObject{{"camera",cameraId},{"pose",pose},{"receiver",receiver},{"source",source},{"variant",v.name},{"projected",int(domain.size())},{"geometryResolved",double(gs.clear+gs.blocked)},{"geometryWrong",geoWrong},{"overflowLists",double(gs.overflow)},{"nodeVisits",double(gs.nodes)},{"triangleTests",double(gs.triangleTests)},{"segmentBoxTests",double(boxTests)},{"fallbackRays",double(rays)},{"denseRays",double(denseRays)},{"denseMs",denseMs},{"falseVisible",fp},{"falseHidden",fn},{"unknown",unknown},{"medianMs",times[1]}});
                }
                // Whole-object projection: all source faces, no chart name or
                // planar restriction. Compare exact source hit against full BVH
                // on receiver hits in a stratified grid; count other-face hits.
                uint64_t tests=0;int checked=0,otherFace=0,meshWrong=0,meshProjected=0;double meshMs=0;
                const auto& object=rt.objects[int(src.owner-1)];
                for(int y=0;y<H;y+=3)for(int x=0;x<W;x+=3){
                    auto ray=e::screenRay(cam,x,y,W,H);V p,uv;float dist;
                    if(!rec.intersect(s,ray.o,ray.d,p,uv,dist))continue;
                    V d=e::reflect(ray.d,rec.n);float low=eps(p);
                    t=e::FrameClock::now();auto m=o::mesh(s,object,{p,d},low,tests);meshMs+=e::elapsedMs(t);
                    if(m.primitive!=0xffffffffu)++meshProjected;
                    e::HitSample hit;if(!trace.closest({p,d},low,hit)||hit.object!=src.owner)continue;
                    ++checked;if(std::find(src.tris.begin(),src.tris.end(),hit.primitive)==src.tris.end())++otherFace;
                    if(m.primitive!=hit.primitive||std::abs(m.depth-hit.depth)>.001f||std::abs(m.uv.x-hit.uv.x)>.001f||std::abs(m.uv.y-hit.uv.y)>.001f)++meshWrong;
                }
                meshErrors+=meshWrong;meshResults.append(QJsonObject{{"camera",cameraId},{"pose",pose},{"receiver",receiver},{"source",source},{"chartTriangles",int(src.tris.size())},{"objectTriangles",object.triangleCount},{"projectedSamples",meshProjected},{"visibleChecked",checked},{"otherFaceHits",otherFace},{"mismatch",meshWrong},{"triangleTests",double(tests)},{"meshMs",meshMs}});
            }
        }
    }
    QJsonObject report{{"testedPairPoses",tested},{"geometryErrors",certErrors},{"cropErrors",cropErrors},{"meshErrors",meshErrors},{"variants",results},{"crop",cropResults},{"discoveryReuse",reuse},{"allFaces",meshResults},
        {"note","Independent CPU diagnostic sweep; no renderer modifications. Approximate fallback errors reported, not hidden. Discovery reuse compares stale ID catalogue against fresh sparse detection, not exhaustive coverage. Whole-mesh test checks exact hit/UV for visible source in sampled receiver rays."}};
    QFile f(out.filePath("optimization-summary.json"));if(!f.open(QIODevice::WriteOnly))return 6;f.write(QJsonDocument(report).toJson());f.close();
    qInfo()<<"tested"<<tested<<"geometry errors"<<certErrors<<"crop errors"<<cropErrors<<"mesh errors"<<meshErrors;
    return tested>0&&!certErrors&&!cropErrors&&!meshErrors?0:7;
}
