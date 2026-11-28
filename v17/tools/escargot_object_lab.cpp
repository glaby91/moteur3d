#include "../escargot/runtime.h"
#include "../escargot/object_visibility.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <chrono>
#include "../escargot/object_chart.h"
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 2;
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 2;
    auto loaded=v16::loadScene(args[1]);if(!loaded){qCritical()<<loaded.error;return 3;}
    auto built=v16::buildRuntime(loaded.scene,256);if(!built){qCritical()<<built.error;return 4;}
    const auto& rt=built.runtime;const auto& s=rt.renderScene;std::vector<uint32_t> owner;
    if(!e::objectMap(rt,owner))return 5;e::Traversal trace(s,rt.lumaTraceBvh4,owner);if(!trace.valid())return 5;
    auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[](const auto& c){return c.id==QStringLiteral("v7_pool_fresco");});if(it==rt.cameras.end())return 6;
    constexpr int W=320,H=180;auto cam=v16::makeCamera(*it,W,H);
    // Discovery once: water top face and reflected named artwork, using actual BVH.
    uint32_t waterId=0xffffffffu,sourceId=0xffffffffu;std::map<uint32_t,int> waterHits,sourceHits;
    uint64_t discovery=0;
    auto isArt=[&](uint32_t id){if(!id||id>uint32_t(rt.objects.size()))return false;const auto& o=rt.objects[int(id-1)];
        QString name=(o.id+" "+o.material).toLower();return name.contains("fresc")||name.contains("fresq")||name.contains("mural")||name.contains("painting")||name.contains("artwork");};
    for(int y=2;y<H;y+=5)for(int x=2;x<W;x+=5){auto r=e::screenRay(cam,x,y,W,H);e::HitSample h;++discovery;
        if(!trace.closest(r,.08f/std::max(1e-6f,dot(r.d,cam.f)),h)||!e::waterMaterial(s.mat[s.obj[h.primitive].mat])||std::abs(h.geometricNormal.y)<.95f)continue;
        ++waterHits[h.primitive];auto d=e::reflect(r.d,h.geometricNormal);e::HitSample hit;++discovery;
        if(trace.closest({h.p,d},eps(h.p),hit)&&isArt(hit.object))++sourceHits[hit.primitive];
    }
    auto best=[](const auto& m){uint32_t id=0xffffffffu;int n=0;for(auto [k,v]:m)if(v>n){id=k;n=v;}return id;};
    waterId=best(waterHits);sourceId=best(sourceHits);
    if(waterId==0xffffffffu||sourceId==0xffffffffu){
        qWarning()<<"No named fresco reflection discovered; candidate inventory follows.";
        for(int i=0;i<rt.objects.size();++i)if(isArt(uint32_t(i+1)))qInfo()<<i+1<<rt.objects[i].id<<rt.objects[i].material;
        return 7;
    }
    auto water=chart(s,owner,waterId),src=chart(s,owner,sourceId);
    if(water.tris.empty()||src.tris.empty()||src.u1-src.u0<1e-6f||src.v1-src.v0<1e-6f)return 8;
    qInfo()<<"receiver"<<rt.objects[int(water.owner-1)].id<<"source"<<rt.objects[int(src.owner-1)].id<<rt.objects[int(src.owner-1)].material<<"chart triangles"<<src.tris.size();
    updateLighting(rt.daylightHour);float day;V sun;escargotLightingSnapshot(day,sun);
    // Nine source-space lighting samples, independent of observer. Proxy
    // diffuse only, same ambient/direct terms as old Escargot, plus shadow test.
    V n=src.n;if(dot(n,water.o-src.o)<0)n=n*-1.f;
    float uu=dot(src.du,src.du),uv=dot(src.du,src.dv),vv=dot(src.dv,src.dv),det=uu*vv-uv*uv;if(det<=1e-12f)return 9;
    V U=(src.du*vv-src.dv*uv)/det,VV=(src.dv*uu-src.du*uv)/det;
    std::array<float,9> light{};uint64_t lightRays=0;
    for(int j=0;j<3;j++)for(int i=0;i<3;i++){
        float u=src.u0+(src.u1-src.u0)*i*.5f,v=src.v0+(src.v1-src.v0)*j*.5f;
        V p=src.o+U*(u-src.u)+VV*(v-src.v);e::HitSample h;
        float direct=e::sat(dot(n,sun));if(direct>0){++lightRays;if(trace.closest({p+n*eps(p),sun},eps(p),h))direct=0;}
        light[j*3+i]=.12f+.35f*day+.48f*day*direct;
    }
    auto luminosity=[&](V uv){float x=2*e::sat((uv.x-src.u0)/(src.u1-src.u0)),y=2*e::sat((uv.y-src.v0)/(src.v1-src.v0));int i=std::min(1,int(x)),j=std::min(1,int(y));x-=i;y-=j;
        return (1-y)*((1-x)*light[j*3+i]+x*light[j*3+i+1])+y*((1-x)*light[(j+1)*3+i]+x*light[(j+1)*3+i+1]);};
    QJsonArray results;
    for(int pose=0;pose<3;pose++){
        auto cp=*it;cp.eye.x+=.075f*pose;cp.target.x+=.075f*pose;cam=v16::makeCamera(cp,W,H);
        auto start=e::FrameClock::now();std::vector<Projection> proj(W*H);for(int y=0;y<H;y++)for(int x=0;x<W;x++)proj[y*W+x]=project(s,water,src,cam,x,y,W,H);
        std::vector<int> domain;domain.reserve(W*H/8);
        int x0=W,y0=H,x1=-1,y1=-1;
        for(int i=0;i<W*H;++i)if(proj[i].valid){domain.push_back(i);x0=std::min(x0,i%W);x1=std::max(x1,i%W);y0=std::min(y0,i/W);y1=std::max(y1,i/W);}
        double projectMs=e::elapsedMs(start);
        // Dense visibility oracle at identical geometry/lighting; not timed as candidate.
        std::vector<int> truth(W*H,-1);uint64_t denseRays=0;for(int i=0;i<W*H;i++)if(proj[i].valid)truth[i]=visibility(trace,s,water,src,cam,proj[i],denseRays);
        for(int budget:{256,1024,4096}){
            start=e::FrameClock::now();std::vector<float> mask(W*H,-1);uint64_t rays=0;
            auto stats=e::object_visibility::reconstruct(W,domain,budget/2,[&](int i){
                if(rays+2>uint64_t(budget))return -1;
                return visibility(trace,s,water,src,cam,proj[i],rays);
            },mask);
            double maskMs=e::elapsedMs(start);QImage candidate(W,H,QImage::Format_ARGB32),oracle(W,H,QImage::Format_ARGB32),diagnostic(W,H,QImage::Format_RGB32);candidate.fill(0);oracle.fill(0);diagnostic.fill(qRgb(40,40,40));
            int covered=0,visible=0,fp=0,fn=0,unknown=0;
            for(int i=0;i<W*H;i++){if(truth[i]<0)continue;covered++;visible+=truth[i];float a=mask[i];if(a<0)unknown++;else{fp+=a>=.5f&&!truth[i];fn+=a<.5f&&truth[i];}
                V color=e::sourceTexture(s.mat[src.mat],proj[i].uv)*luminosity(proj[i].uv)+s.mat[src.mat].emissionColor*std::max(0.f,s.mat[src.mat].emissionIntensity);
                auto pixel=[&](float t){return qRgba(channel(color.x),channel(color.y),channel(color.z),int(255*e::sat(t)));};
                int x=i%W,y=i/W;candidate.setPixel(x,y,pixel(std::max(0.f,a)));oracle.setPixel(x,y,pixel(float(truth[i])));diagnostic.setPixel(x,y,a<0?qRgb(255,0,255):a>=.5f?qRgb(0,200,100):qRgb(40,80,160));
            }
            QString prefix=QString("pose%1-budget%2").arg(pose).arg(budget);
            if(!candidate.save(out.filePath(prefix+"-candidate.png"))||!oracle.save(out.filePath(prefix+"-oracle.png"))||!diagnostic.save(out.filePath(prefix+"-mask.png")))return 10;
            results.append(QJsonObject{{"pose",pose},{"budget",budget},{"rays",double(rays)},{"denseRays",double(denseRays)},{"projected",covered},{"visibleOracle",visible},{"falseVisible",fp},{"falseHidden",fn},{"unknown",unknown},{"projectionMs",projectMs},{"maskMs",maskMs},{"footprintX",x0},{"footprintY",y0},{"footprintW",x1-x0+1},{"footprintH",y1-y0+1},{"seedSamples",stats.seedSamples},{"measuredSamples",stats.measured},{"zeroSplits",stats.zeroSplits},{"zeroProbes",stats.zeroProbes},{"mixedSplits",stats.mixedSplits},{"otherSplits",stats.otherSplits},{"exhaustive",stats.exhaustive}});
            if(!covered||!visible||rays>uint64_t(budget)||stats.seedSamples<std::min({32,covered,budget/2})||
                (stats.exhaustive&&(fp||fn||unknown)))return 11;
        }
    }
    QJsonObject report{{"receiver",rt.objects[int(water.owner-1)].id},{"source",rt.objects[int(src.owner-1)].id},{"sourceMaterial",rt.objects[int(src.owner-1)].material},{"sourceChartTriangles",int(src.tris.size())},{"sourceObjectTriangles",rt.objects[int(src.owner-1)].triangleCount},{"waterChartTriangles",int(water.tris.size())},{"discoveryQueries",double(discovery)},{"lightingQueriesOnce",double(lightRays)},{"lightingSamples",9},{"cases",results},{"note","CPU optical diagnostic, one coplanar UV chart, transparent occluders treated as opaque; unknown mask is magenta; no GPU FPS claim"}};
    QFile f(out.filePath("summary.json"));if(!f.open(QIODevice::WriteOnly))return 12;f.write(QJsonDocument(report).toJson());qInfo().noquote()<<QJsonDocument(report).toJson();return 0;
}
