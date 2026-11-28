#include "../escargot/object_chart.h"
#include "../escargot/object_geometry_mask.h"
#include "../escargot/object_visibility.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <set>
namespace g=m3d::escargot::geometry_mask;
struct PairHit {uint32_t receiver=0,source=0;int count=0;};
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 2;
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 2;
    auto loaded=v16::loadScene(args[1]);if(!loaded)return 3;auto built=v16::buildRuntime(loaded.scene,256);if(!built)return 4;
    const auto& rt=built.runtime;const auto& s=rt.renderScene;std::vector<uint32_t> owners;
    if(!e::objectMap(rt,owners))return 5;e::Traversal trace(s,rt.lumaTraceBvh4,owners);if(!trace.valid())return 5;
    auto catalogue=e::staticCandidates(rt,loaded.scene,e::Mode::Reflection);std::set<uint32_t> eligible;
    for(const auto& c:catalogue)eligible.insert(c.object);
    constexpr int W=320,H=180;
    QJsonArray views,cases;int tested=0,certifiedErrors=0;std::map<uint32_t,Chart> chartCache;std::set<uint32_t> sourceObjects;
    auto chartFor=[&](uint32_t id)->const Chart&{
        auto it=chartCache.find(id);if(it!=chartCache.end())return it->second;
        auto c=chart(s,owners,id);return chartCache.emplace(id,std::move(c)).first->second;
    };
    // Camera IDs are only QA test locations. Neither receiver nor source
    // discovery tests any name, texture name or special water/fresco ID.
    for(const QString cameraId:{QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing"),QString("v17_showcase_pool_634")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==cameraId;});if(it==rt.cameras.end())continue;
        const auto cam=v16::makeCamera(*it,W,H);auto start=e::FrameClock::now();
        std::map<std::pair<uint32_t,uint32_t>,std::map<std::pair<uint32_t,uint32_t>,int>> found;
        uint64_t queries=0;std::vector<uint32_t> seenReceivers(rt.objects.size()+1,0);
        auto projected=e::projectCandidates(catalogue,cam,W,H);
        // Start with distributed screen rays, then sample each eligible
        // receiver's projected bounds so small candidates get a chance.
        // Deduplicate screen positions; discovery is bounded, not exhaustive.
        std::vector<uint8_t> sampled(W*H,0);
        auto detect=[&](int x,int y){
            int i=y*W+x;if(sampled[i]||queries+2>8192)return;sampled[i]=1;
            auto ray=e::screenRay(cam,x,y,W,H);e::HitSample a,b;++queries;
            if(!trace.closest(ray,.08f/std::max(1e-6f,dot(ray.d,cam.f)),a)||!eligible.count(a.object))return;
            ++seenReceivers[a.object];++queries;
            V d=e::reflect(ray.d,a.geometricNormal);
            if(trace.closest({a.p,d},eps(a.p),b))++found[{a.object,b.object}][{a.primitive,b.primitive}];
        };
        for(int y=3;y<H;y+=7)for(int x=3;x<W;x+=7)detect(x,y);
        for(const auto& c:projected)for(int j=0;j<3;++j)for(int i=0;i<3;++i)
            detect(c.bounds.x+(c.bounds.w-1)*i/2,c.bounds.y+(c.bounds.h-1)*j/2);
        double discoveryMs=e::elapsedMs(start);QJsonArray inventory;std::vector<PairHit> ranked;
        for(const auto& pair:found){
            int sum=0;std::pair<uint32_t,uint32_t> best{};int count=-1;
            for(const auto& h:pair.second){sum+=h.second;if(h.second>count){count=h.second;best=h.first;}}
            inventory.append(QJsonObject{{"receiver",rt.objects[int(pair.first.first-1)].id},{"source",rt.objects[int(pair.first.second-1)].id},{"hits",sum},{"primitivePairs",int(pair.second.size())}});
            ranked.push_back({best.first,best.second,sum});
        }
        std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.count!=b.count?a.count>b.count:std::tie(a.receiver,a.source)<std::tie(b.receiver,b.source);});
        int untested=0;for(const auto& c:projected)untested+=seenReceivers[c.object]==0;
        views.append(QJsonObject{{"camera",cameraId},{"eligibleProjectedReceivers",int(projected.size())},{"receiversWithoutHit",untested},{"discoveryQueries",double(queries)},{"discoveryMs",discoveryMs},{"inventory",inventory}});
        // Limit ONLY the diagnostic workload, not the discovery inventory.
        // Each pair is tested independently as one planar UV chart; complex
        // objects remain listed even if not fully covered by this chart.
        int selected=0;
        for(const auto& pair:ranked){
            if(selected>=3)break;
            const auto& receiver=chartFor(pair.receiver);const auto& source=chartFor(pair.source);
            if(receiver.tris.empty()||source.tris.empty())continue;
            auto t=e::FrameClock::now();std::vector<Projection> proj(W*H);std::vector<int> domain;
            for(int y=0;y<H;++y)for(int x=0;x<W;++x){int i=y*W+x;proj[i]=project(s,receiver,source,cam,x,y,W,H);if(proj[i].valid)domain.push_back(i);}
            double projectMs=e::elapsedMs(t);if(domain.empty())continue;
            std::vector<float> mask(W*H,-1);t=e::FrameClock::now();
            const auto gs=g::classify(s,rt.lumaTraceBvh4,owners,receiver,source,cam,proj,W,H,mask);
            double geometryMs=e::elapsedMs(t);
            std::vector<int> unresolved;for(int i:domain)if(mask[i]<0)unresolved.push_back(i);
            std::vector<float> combined=mask;uint64_t fallbackRays=0;t=e::FrameClock::now();
            auto vs=e::object_visibility::reconstruct(W,unresolved,512,[&](int i){return visibility(trace,s,receiver,source,cam,proj[i],fallbackRays);},combined);
            double fallbackMs=e::elapsedMs(t);
            // Independent dense oracle, never used for geometry selection.
            uint64_t denseRays=0;int fp=0,fn=0,unknown=0,certWrong=0,oracleVisible=0;
            QImage image(W,H,QImage::Format_RGB32),truthImage(W,H,QImage::Format_RGB32);image.fill(qRgb(40,40,40));truthImage.fill(qRgb(40,40,40));
            for(int i:domain){
                int truth=visibility(trace,s,receiver,source,cam,proj[i],denseRays);oracleVisible+=truth;
                if(mask[i]>=0&&int(mask[i])!=truth)++certWrong;
                float v=combined[i];unknown+=v<0;if(v>=0){fp+=v>=.5f&&!truth;fn+=v<.5f&&truth;}
                image.setPixel(i%W,i/W,v<0?qRgb(255,0,255):v>=.5f?qRgb(0,200,100):qRgb(40,80,160));
                truthImage.setPixel(i%W,i/W,truth?qRgb(0,200,100):qRgb(40,80,160));
            }
            if(!oracleVisible)continue;
            ++selected;++tested;certifiedErrors+=certWrong;sourceObjects.insert(source.owner);
            QString prefix=cameraId+QString("-pair%1").arg(selected);
            if(!image.save(out.filePath(prefix+"-mask.png"))||!truthImage.save(out.filePath(prefix+"-oracle.png")))return 6;
            cases.append(QJsonObject{{"camera",cameraId},{"receiver",rt.objects[int(receiver.owner-1)].id},{"source",rt.objects[int(source.owner-1)].id},
                {"sourceChartTriangles",int(source.tris.size())},{"sourceObjectTriangles",rt.objects[int(source.owner-1)].triangleCount},{"receiverChartTriangles",int(receiver.tris.size())},
                {"projected",int(domain.size())},{"oracleVisible",oracleVisible},{"geometryClear",double(gs.clear)},{"geometryBlocked",double(gs.blocked)},{"geometryUnknown",double(gs.unknown)},{"geometryWrong",certWrong},
                {"bundleLists",double(gs.lists)},{"overflowLists",double(gs.overflow)},{"nodeVisits",double(gs.nodes)},{"localTriangleTests",double(gs.triangleTests)},
                {"fallbackRays",double(fallbackRays)},{"fallbackSamples",vs.measured},{"denseRays",double(denseRays)},{"falseVisible",fp},{"falseHidden",fn},{"unknown",unknown},
                {"projectionMs",projectMs},{"geometryMs",geometryMs},{"fallbackMs",fallbackMs}});
        }
    }
    QJsonObject report{{"testedPairs",tested},{"distinctSourceObjects",int(sourceObjects.size())},{"certifiedErrors",certifiedErrors},{"views",views},{"cases",cases},
        {"note","Generic discovery, one coherent planar chart per tested pair; unobserved objects remain unknown. CPU conservative BVH bundle AABBs and exact local triangles, capped lists, adaptive fallback. Geometry work is NOT free or equivalent to zero GPU time."}};
    QFile f(out.filePath("generic-summary.json"));if(!f.open(QIODevice::WriteOnly))return 7;f.write(QJsonDocument(report).toJson());f.close();
    qInfo().noquote()<<"Generic pairs"<<tested<<"distinct objects"<<sourceObjects.size()<<"exact geometry disagreements"<<certifiedErrors;
    return tested>0&&sourceObjects.size()>1&&!certifiedErrors?0:8;
}
