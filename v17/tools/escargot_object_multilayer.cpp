#include "../escargot/object_global.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
namespace og=m3d::escargot::object_global;
struct Path{
    og::WorkRay ray;
    uint32_t receiver=0,receiverPrimitive=0xffffffffu;
    float primaryTransmission=1,weight=0,remaining=1;
    V rgb{};
    int primaryCrossings=0,secondaryCrossings=0;
    bool secondary=false,done=false,complete=false;
    std::vector<e::HitSample> layers;
};
static void advance(Path& p,const e::HitSample& h,const Scene& s,const std::set<uint32_t>& eligible,int cap){
    if(!h.object){p.done=p.complete=true;return;}
    const auto& mat=s.mat[s.obj[h.primitive].mat];
    if(!p.secondary){
        ++p.primaryCrossings;
        if(!eligible.count(h.object)){
            p.primaryTransmission*=e::transmission(mat);
            if(p.primaryTransmission<=.001f){p.done=p.complete=true;return;}
            p.ray.low=h.depth;p.ray.previous=h.primitive;
            if(p.primaryCrossings>=cap)p.done=true;
            return;
        }
        p.receiver=h.object;p.receiverPrimitive=h.primitive;
        V geom=h.geometricNormal;if(dot(geom,p.ray.ray.d)>0)geom=geom*-1.f;
        V n=dot(h.n,geom)<0?h.n*-1.f:h.n;if(dot(n,p.ray.ray.d)>0)n=geom;
        p.weight=e::opticalWeight(mat,n,p.ray.ray.d,e::Mode::Reflection)*p.primaryTransmission;
        V d=e::reflect(p.ray.ray.d,n);if(dot(d,geom)<=0)d=e::reflect(p.ray.ray.d,geom);
        p.ray={{h.p,d},eps(h.p),0xffffffffu};p.secondary=true;return;
    }
    ++p.secondaryCrossings;p.layers.push_back(h);
    const float a=e::opacity(mat);
    e::Value v;v.uv=h.uv;v.normal=dot(h.n,p.ray.ray.d)>0?h.n*-1.f:h.n;
    p.rgb=p.rgb+e::sourceColor(mat,v)*(p.remaining*a);p.remaining*=1-a;
    if(a>=.999f||p.remaining<=.001f){p.done=p.complete=true;return;}
    p.ray.low=h.depth;p.ray.previous=h.primitive;
    if(p.secondaryCrossings>=cap)p.done=true;
}
struct Frame{std::vector<Path> pixels;uint64_t queries=0;og::Counters counters;double ms=0;};
static Frame render(const Scene& s,const LumaTraceBvh4& b,const std::vector<uint32_t>& owners,const std::set<uint32_t>& eligible,
                    e::Traversal& scalar,const Camera& cam,int w,int h,int cap,bool packet){
    Frame f;auto start=e::FrameClock::now();f.pixels.resize(size_t(w)*h);
    og::Packet p(s,b,owners);
    std::vector<size_t> ids;std::vector<og::WorkRay> rays;std::vector<e::HitSample> hits;
    for(int y=0;y<h;y+=4)for(int x=0;x<w;x+=4){
        std::vector<size_t> tile;
        for(int yy=y;yy<std::min(y+4,h);++yy)for(int xx=x;xx<std::min(x+4,w);++xx){
            size_t i=size_t(yy)*w+xx;tile.push_back(i);auto r=e::screenRay(cam,xx,yy,w,h);
            f.pixels[i].ray={r,.08f/std::max(1e-6f,dot(r.d,cam.f)),0xffffffffu};
        }
        for(int pass=0;pass<2*cap;++pass){
            ids.clear();rays.clear();for(auto i:tile)if(!f.pixels[i].done){ids.push_back(i);rays.push_back(f.pixels[i].ray);}
            if(ids.empty())break;
            f.queries+=ids.size();
            if(packet)p.trace(rays,{},hits,f.counters);
            else{hits.resize(ids.size());for(size_t j=0;j<ids.size();++j)scalar.closest(rays[j].ray,rays[j].low,hits[j],rays[j].previous);}
            for(size_t j=0;j<ids.size();++j)advance(f.pixels[ids[j]],hits[j],s,eligible,cap);
        }
    }
    f.ms=e::elapsedMs(start);return f;
}
static QImage picture(const Frame& f,int w,int h){
    QImage im(w,h,QImage::Format_ARGB32);im.fill(0);
    for(size_t i=0;i<f.pixels.size();++i){const auto& p=f.pixels[i];if(!p.receiver)continue;
        if(!p.complete){im.setPixel(int(i%w),int(i/w),qRgba(255,0,255,255));continue;}
        V c=p.rgb*p.weight;im.setPixel(int(i%w),int(i/w),qRgba(channel(c.x),channel(c.y),channel(c.z),int(255*e::sat(1-p.remaining))));
    }return im;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);auto args=app.arguments();if(args.size()!=3)return 2;
    QDir out(args[2]);if(!QDir().mkpath(out.absolutePath()))return 2;
    auto loaded=v16::loadScene(args[1]);if(!loaded)return 3;auto built=v16::buildRuntime(loaded.scene,256);if(!built)return 4;
    const auto& rt=built.runtime;const auto& s=rt.renderScene;std::vector<uint32_t> owners;
    if(!e::objectMap(rt,owners))return 5;e::Traversal scalar(s,rt.lumaTraceBvh4,owners);if(!scalar.valid())return 5;
    std::set<uint32_t> eligible;for(const auto& c:e::staticCandidates(rt,loaded.scene,e::Mode::Reflection))eligible.insert(c.object);
    updateLighting(rt.daylightHour);escargotLightingSnapshot(e::proxyDaylight,e::proxySun);
    QJsonArray results;int errors=0,cases=0;uint64_t multi=0;
    for(const QString cameraId:{QString("v7_pool_fresco"),QString("v7_curtain_wall_detail"),QString("v8_stair_first_landing")}){
        auto it=std::find_if(rt.cameras.begin(),rt.cameras.end(),[&](const auto& c){return c.id==cameraId;});if(it==rt.cameras.end())continue;
        for(int w:{320,640}){int h=w*9/16;
            for(int pose=0;pose<2;++pose){auto cp=*it;cp.eye.x+=.15f*pose;cp.target.x+=.15f*pose;auto cam=v16::makeCamera(cp,w,h);
                for(int cap:{4,8,16}){
                    auto a=render(s,rt.lumaTraceBvh4,owners,eligible,scalar,cam,w,h,cap,false);
                    auto b=render(s,rt.lumaTraceBvh4,owners,eligible,scalar,cam,w,h,cap,true);
                    int err=0,receivers=0,layered=0,unknown=0,through=0;uint64_t layers=0;
                    for(size_t i=0;i<a.pixels.size();++i){const auto& p=a.pixels[i];const auto& q=b.pixels[i];
                        receivers+=p.receiver!=0;layered+=p.layers.size()>1;unknown+=!p.complete;through+=p.receiver&&p.primaryCrossings>1;layers+=p.layers.size();
                        bool mismatch=p.receiver!=q.receiver||p.receiverPrimitive!=q.receiverPrimitive||p.complete!=q.complete||p.layers.size()!=q.layers.size()||
                            std::abs(p.weight-q.weight)>1e-6f||std::abs(p.remaining-q.remaining)>1e-6f||length(p.rgb-q.rgb)>1e-6f;
                        for(size_t j=0;j<std::min(p.layers.size(),q.layers.size());++j){
                            const auto& x=p.layers[j];const auto& y=q.layers[j];mismatch|=x.primitive!=y.primitive||x.object!=y.object||std::abs(x.depth-y.depth)>1e-5f||length(x.uv-y.uv)>1e-6f;
                        }err+=mismatch;
                    }
                    if(pose==0&&cap==8){auto x=picture(a,w,h),y=picture(b,w,h);if(x!=y)++err;
                        QString p=cameraId+QString("-w%1").arg(w);
                        if(!x.save(out.filePath(p+"-scalar.png"))||!y.save(out.filePath(p+"-packet.png")))return 6;}
                    ++cases;errors+=err;multi+=layered;
                    results.append(QJsonObject{{"camera",cameraId},{"width",w},{"pose",pose},{"cap",cap},{"receivers",receivers},{"multilayerPixels",layered},{"primaryThroughPixels",through},{"unknownPixels",unknown},{"layers",double(layers)},
                        {"scalarMs",a.ms},{"packetMs",b.ms},{"scalarQueries",double(a.queries)},{"packetLanes",double(b.queries)},{"packetNodes",double(b.counters.nodeLoads)},{"errors",err}});
                }
            }
        }
    }
    QJsonObject report{{"cases",cases},{"errors",errors},{"multilayerPixelObservations",double(multi)},{"results",results},
        {"note","First eligible receiver behind transparent primary crossings; straight transmission on secondary crossings with existing Escargot opacity/Fresnel policy and proxy light. Exact scalar vs packet parity, cap exhaustion explicitly unknown. Not full Snell refraction, not all primary reflective layers, not GPU integration. Single paired timing per case."}};
    QFile f(out.filePath("multilayer-summary.json"));if(!f.open(QIODevice::WriteOnly))return 7;f.write(QJsonDocument(report).toJson());f.close();
    return cases>0&&multi>0&&!errors?0:8;
}
