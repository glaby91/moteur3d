#include "../escargot/runtime.h"
#include <QCoreApplication>
#include <QDir>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <chrono>
namespace e=m3d::escargot;
using namespace m3d;
struct Chart {
    uint32_t owner=0,mat=0; V o{},n{},du{},dv{};float u=0,v=0;
    float u0=INF,v0=INF,u1=-INF,v1=-INF;
    std::vector<uint32_t> tris;
    V uv(V p)const{return {u+dot(du,p-o),v+dot(dv,p-o),0};}
    bool intersect(const Scene& s,V from,V d,V& p,V& tex,float& t)const{
        float den=dot(n,d);if(std::abs(den)<1e-7f)return false;
        t=dot(n,o-from)/den;if(t<=1e-5f||!std::isfinite(t))return false;
        p=from+d*t;tex=uv(p);
        for(auto id:tris){const auto& a=s.obj[id];V x=a.b-a.a,y=a.c-a.a,z=p-a.a;
            float xx=dot(x,x),xy=dot(x,y),yy=dot(y,y),det=xx*yy-xy*xy;if(det<=1e-12f)continue;
            float k=(dot(z,x)*yy-dot(z,y)*xy)/det,l=(dot(z,y)*xx-dot(z,x)*xy)/det;
            if(k>=-1e-4f&&l>=-1e-4f&&k+l<=1.0001f)return true;
        }return false;
    }
};
static Chart chart(const Scene& s,const std::vector<uint32_t>& owner,uint32_t id){
    Chart a;if(id>=s.obj.size())return a;const auto& t=s.obj[id];V x=t.b-t.a,y=t.c-t.a;
    float xx=dot(x,x),xy=dot(x,y),yy=dot(y,y),det=xx*yy-xy*xy;if(det<=1e-12f)return a;
    V gx=(x*yy-y*xy)/det,gy=(y*xx-x*xy)/det;
    a.owner=owner[id];a.mat=t.mat;a.o=t.a;a.n=norm(cross(x,y));a.u=t.uv0.x;a.v=t.uv0.y;
    a.du=gx*(t.uv1.x-a.u)+gy*(t.uv2.x-a.u);a.dv=gx*(t.uv1.y-a.v)+gy*(t.uv2.y-a.v);
    if(!e::finite(a.du)||!e::finite(a.dv))return {};
    for(uint32_t j=0;j<s.obj.size();j++){const auto& q=s.obj[j];if(owner[j]!=a.owner||q.mat!=a.mat)continue;
        bool ok=true;for(auto pair:{std::pair<V,V>{q.a,q.uv0},{q.b,q.uv1},{q.c,q.uv2}}){auto z=a.uv(pair.first);
            if(std::abs(dot(pair.first-a.o,a.n))>.002f||std::abs(z.x-pair.second.x)>.002f||std::abs(z.y-pair.second.y)>.002f)ok=false;}
        if(ok){a.tris.push_back(j);for(V p:{q.a,q.b,q.c}){auto z=a.uv(p);a.u0=std::min(a.u0,z.x);a.u1=std::max(a.u1,z.x);a.v0=std::min(a.v0,z.y);a.v1=std::max(a.v1,z.y);}}
    }return a;
}
struct Projection {V water{},source{},uv{},direction{};float primary=0,secondary=0;bool valid=false;};
static Projection project(const Scene& s,const Chart& water,const Chart& src,const Camera& cam,int x,int y,int w,int h){
    Projection p;auto r=e::screenRay(cam,x,y,w,h);V unused;
    if(!water.intersect(s,r.o,r.d,p.water,unused,p.primary))return p;
    p.direction=e::reflect(r.d,water.n);
    p.valid=src.intersect(s,p.water,p.direction,p.source,p.uv,p.secondary);return p;
}
static float eps(V p){return std::max(1e-5f,8.f*std::numeric_limits<float>::epsilon()*std::max(1.f,length(p)));}
static int visibility(e::Traversal& b,const Scene& s,const Chart& water,const Chart& src,const Camera& cam,const Projection& p,uint64_t& rays){
    e::HitSample hit;const V d=norm(p.water-cam.eye);++rays;
    if(!b.closest({cam.eye,d},.08f/std::max(1e-6f,dot(d,cam.f)),hit)||hit.object!=water.owner||std::abs(hit.depth-p.primary)>.003f)return 0;
    ++rays;if(!b.closest({p.water,p.direction},eps(p.water),hit))return 0;
    if(hit.object==src.owner&&std::abs(hit.depth-p.secondary)<.003f&&s.obj[hit.primitive].mat==src.mat)return 1;
    return 0; // transparent crossings deliberately excluded in single-layer test
}
static int channel(float x){x=e::sat(x);return int(std::round(255*(x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f)));}
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
        double projectMs=e::elapsedMs(start);
        // Dense visibility oracle at identical geometry/lighting; not timed as candidate.
        std::vector<int> truth(W*H,-1);uint64_t denseRays=0;for(int i=0;i<W*H;i++)if(proj[i].valid)truth[i]=visibility(trace,s,water,src,cam,proj[i],denseRays);
        for(int budget:{256,1024,4096}){
            start=e::FrameClock::now();std::vector<int> samples(W*H,-3);std::vector<float> mask(W*H,-1);uint64_t rays=0;
            auto sample=[&](int x,int y){int i=y*W+x;if(samples[i]!=-3)return samples[i];if(!proj[i].valid)return samples[i]=-1;if(rays+2>uint64_t(budget))return -2;return samples[i]=visibility(trace,s,water,src,cam,proj[i],rays);};
            std::function<void(int,int,int,int)> refine=[&](int x0,int y0,int x1,int y1){
                int xm=(x0+x1)/2,ym=(y0+y1)/2;int v[]={sample(x0,y0),sample(x1,y0),sample(x0,y1),sample(x1,y1),sample(xm,ym),sample(xm,y0),sample(xm,y1),sample(x0,ym),sample(x1,ym)};
                bool diff=std::find(v,v+9,0)!=v+9&&std::find(v,v+9,1)!=v+9;
                if(diff&&std::max(x1-x0,y1-y0)>2&&rays+36<=uint64_t(budget)){
                    for(int y=y0;y<=y1;y=ym+1){int b=y==y0?ym:y1;
                        for(int x=x0;x<=x1;x=xm+1){int a=x==x0?xm:x1;refine(x,y,a,b);if(a==x1)break;}if(b==y1)break;}return;
                }
                int known=0,positive=0;for(int z:v)if(z>=0){known++;positive+=z;}
                for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){int i=y*W+x;if(!proj[i].valid)continue;
                    if(!known)continue;
                    float u=x0==x1?0:float(x-x0)/(x1-x0),w=y0==y1?0:float(y-y0)/(y1-y0);
                    float weights[]={(1-u)*(1-w),u*(1-w),(1-u)*w,u*w},sum=0,weight=0;
                    for(int k=0;k<4;k++)if(v[k]>=0){sum+=weights[k]*v[k];weight+=weights[k];}
                    mask[i]=weight>1e-6f?sum/weight:float(positive)/known;
                    if(samples[i]>=0)mask[i]=float(samples[i]);
                }
            };
            // Global coarse pass first: reserve half the approximate query budget
            // for refinement; visibility zero/outside/unknown never alter UV.
            int step=1;while(double(step)*step<double(W)*H*36/budget)step*=2;
            for(int y=0;y<H;y+=step)for(int x=0;x<W;x+=step){
                int a=std::min(W-1,x+step-1),b=std::min(H-1,y+step-1),xm=(x+a)/2,ym=(y+b)/2;
                for(int yy:{y,ym,b})for(int xx:{x,xm,a})sample(xx,yy);
            }
            for(int y=0;y<H;y+=step)for(int x=0;x<W;x+=step)refine(x,y,std::min(W-1,x+step-1),std::min(H-1,y+step-1));
            double maskMs=e::elapsedMs(start);QImage candidate(W,H,QImage::Format_ARGB32),oracle(W,H,QImage::Format_ARGB32),diagnostic(W,H,QImage::Format_RGB32);candidate.fill(0);oracle.fill(0);diagnostic.fill(qRgb(40,40,40));
            int covered=0,visible=0,fp=0,fn=0,unknown=0;
            for(int i=0;i<W*H;i++){if(truth[i]<0)continue;covered++;visible+=truth[i];float a=mask[i];if(a<0)unknown++;else{fp+=a>=.5f&&!truth[i];fn+=a<.5f&&truth[i];}
                V color=e::sourceTexture(s.mat[src.mat],proj[i].uv)*luminosity(proj[i].uv)+s.mat[src.mat].emissionColor*std::max(0.f,s.mat[src.mat].emissionIntensity);
                auto pixel=[&](float t){return qRgba(channel(color.x),channel(color.y),channel(color.z),int(255*e::sat(t)));};
                int x=i%W,y=i/W;candidate.setPixel(x,y,pixel(std::max(0.f,a)));oracle.setPixel(x,y,pixel(float(truth[i])));diagnostic.setPixel(x,y,a<0?qRgb(255,0,255):a>=.5f?qRgb(0,200,100):qRgb(40,80,160));
            }
            QString prefix=QString("pose%1-budget%2").arg(pose).arg(budget);
            if(!candidate.save(out.filePath(prefix+"-candidate.png"))||!oracle.save(out.filePath(prefix+"-oracle.png"))||!diagnostic.save(out.filePath(prefix+"-mask.png")))return 10;
            results.append(QJsonObject{{"pose",pose},{"budget",budget},{"rays",double(rays)},{"denseRays",double(denseRays)},{"projected",covered},{"visibleOracle",visible},{"falseVisible",fp},{"falseHidden",fn},{"unknown",unknown},{"projectionMs",projectMs},{"maskMs",maskMs}});
            if(!covered||!visible||rays>uint64_t(budget))return 11;
        }
    }
    QJsonObject report{{"receiver",rt.objects[int(water.owner-1)].id},{"source",rt.objects[int(src.owner-1)].id},{"sourceMaterial",rt.objects[int(src.owner-1)].material},{"sourceChartTriangles",int(src.tris.size())},{"sourceObjectTriangles",rt.objects[int(src.owner-1)].triangleCount},{"waterChartTriangles",int(water.tris.size())},{"discoveryQueries",double(discovery)},{"lightingQueriesOnce",double(lightRays)},{"lightingSamples",9},{"cases",results},{"note","CPU optical diagnostic, one coplanar UV chart, transparent occluders treated as opaque; unknown mask is magenta; no GPU FPS claim"}};
    QFile f(out.filePath("summary.json"));if(!f.open(QIODevice::WriteOnly))return 12;f.write(QJsonDocument(report).toJson());qInfo().noquote()<<QJsonDocument(report).toJson();return 0;
}
