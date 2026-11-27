#pragma once
#include "types.h"
#include "src/m3d_zreflect_catalog.h"
namespace m3d::escargot {
inline bool objectMap(const v16::Runtime& rt,std::vector<uint32_t>& owner){
    owner.assign(rt.renderScene.obj.size(),0);
    if(rt.objects.size()>=std::numeric_limits<uint32_t>::max())return false;
    for(int i=0;i<rt.objects.size();++i){
        const auto& o=rt.objects[i];
        if(o.firstTriangle<0||o.triangleCount<0||size_t(o.firstTriangle)>owner.size()||size_t(o.triangleCount)>owner.size()-size_t(o.firstTriangle))return false;
        for(int j=0;j<o.triangleCount;++j){auto& id=owner[size_t(o.firstTriangle)+j];if(id)return false;id=uint32_t(i)+1;}
    }return std::find(owner.begin(),owner.end(),0u)==owner.end();
}
// Conservative screen footprint of world AABB. Clip the 12 edges at near plane;
// if camera inside the box use full viewport. No secondary camera.
inline Rect projectBounds(V lo,V hi,const Camera& c,int w,int h){
    if(!finite(lo)||!finite(hi)||lo.x>hi.x||lo.y>hi.y||lo.z>hi.z||!validCamera(c)||w<=0||h<=0)return {};
    if(c.eye.x>=lo.x&&c.eye.x<=hi.x&&c.eye.y>=lo.y&&c.eye.y<=hi.y&&c.eye.z>=lo.z&&c.eye.z<=hi.z)return {0,0,w,h};
    V p[8];for(int i=0;i<8;++i){V q=V{(i&1)?hi.x:lo.x,(i&2)?hi.y:lo.y,(i&4)?hi.z:lo.z}-c.eye;p[i]={dot(q,c.r),dot(q,c.u),dot(q,c.f)};}
    float minx=INF,miny=INF,maxx=-INF,maxy=-INF;
    auto add=[&](V a){if(a.z<.07999f)return;const float x=(.5f+.5f*a.x/(a.z*c.tanHalf))*w,y=(.5f-.5f*a.y*c.aspect/(a.z*c.tanHalf))*h;minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);};
    for(int i=0;i<8;++i){if(p[i].z>=.08f)add(p[i]);for(int bit:{1,2,4})if(!(i&bit)){V a=p[i],b=p[i|bit];if((a.z>=.08f)!=(b.z>=.08f))add(a+(b-a)*((.08f-a.z)/(b.z-a.z)));}}
    if(!std::isfinite(minx)||!std::isfinite(miny)||maxx<=0||maxy<=0||minx>=w||miny>=h)return {};
    const int x0=int(std::floor(clampf(minx,0.f,float(w)))),y0=int(std::floor(clampf(miny,0.f,float(h))));
    const int x1=int(std::ceil(clampf(maxx,0.f,float(w)))),y1=int(std::ceil(clampf(maxy,0.f,float(h))));
    return {x0,y0,x1-x0,y1-y0};
}
// Deliberate optical policy: ordinary dielectric Fresnel is not eligibility.
inline bool eligible(bool water,bool mirror,float transmissionValue,float metal,float rough,zreflect::Permission perm,Mode mode){
    if(perm==zreflect::Permission::Disabled||perm==zreflect::Permission::Invalid)return false;
    if(mode==Mode::Refraction)return water||transmissionValue>=.10f;
    if(water||mirror||perm==zreflect::Permission::Enabled)return true;
    return (transmissionValue>=.10f&&rough<=.25f)||(metal>=.80f&&rough<=.25f);
}
inline std::vector<Candidate> staticCandidates(const v16::Runtime& rt,const v16::SceneDatabase& db,Mode mode){
    const auto instances=zreflect::authored(db,"instances"),materials=zreflect::authored(db,"materials");
    const auto& s=rt.renderScene;std::vector<Candidate> out;
    for(int i=0;i<rt.objects.size();++i){
        const auto& o=rt.objects[i];if(o.triangleCount<=0||o.firstTriangle<0||size_t(o.firstTriangle)>=s.obj.size()||size_t(o.triangleCount)>s.obj.size()-size_t(o.firstTriangle))continue;
        const uint32_t mid=s.obj[o.firstTriangle].mat;if(mid>=s.mat.size())continue;
        const auto& mat=s.mat[mid];auto authored=materials.value(o.material);
        const auto perm=zreflect::resolvePermission(instances.value(o.id),authored,db.manifest);
        if(perm==zreflect::Permission::Disabled||perm==zreflect::Permission::Invalid)continue;
        const bool water=waterMaterial(mat);
        const float rough=clampf(float(authored.value("roughness").toDouble(.72)),0.f,1.f);
        if(!eligible(water,authored.value("opticalMirror").toBool(),transmission(mat),mat.metallic,rough,perm,mode))continue;
        V lo{INF,INF,INF},hi{-INF,-INF,-INF};
        for(int j=0;j<o.triangleCount;++j){const auto& t=s.obj[size_t(o.firstTriangle)+j];
            lo={std::min(lo.x,t.mn.x),std::min(lo.y,t.mn.y),std::min(lo.z,t.mn.z)};
            hi={std::max(hi.x,t.mx.x),std::max(hi.y,t.mx.y),std::max(hi.z,t.mx.z)};
        }
        out.push_back({uint32_t(i)+1,mid,{},lo,hi,.25+.75*(1-rough),0});
    }return out;
}
inline std::vector<Candidate> projectCandidates(const std::vector<Candidate>& catalogue,const Camera& cam,int w,int h){
    std::vector<Candidate> out;out.reserve(catalogue.size());
    for(auto c:catalogue){
        c.bounds=projectBounds(c.lo,c.hi,cam,w,h);
        if(!c.bounds.valid()||uint64_t(c.bounds.w)*c.bounds.h<16)continue;
        c.priority*=std::sqrt(double(c.bounds.w)*c.bounds.h);
        out.push_back(c);
    }
    // Deterministic, proportional allocation; budget is a query count, not a cap on surfaces.
    std::sort(out.begin(),out.end(),[](const Candidate& a,const Candidate& b){return a.priority>b.priority||(a.priority==b.priority&&a.object<b.object);});
    return out;
}
inline void distribute(std::vector<Candidate>& c,uint32_t budget){
    // Each admitted footprint needs at least one primary and one secondary query.
    for(auto& item:c)item.budget=0;
    const size_t n=std::min(c.size(),size_t(budget/2));if(!n)return;
    double weight=0;for(size_t i=0;i<n;++i)weight+=c[i].priority;
    uint32_t left=budget-uint32_t(n)*2;
    uint32_t used=0;
    for(size_t i=0;i<n;++i){uint32_t extra=uint32_t(std::floor(left*c[i].priority/std::max(weight,1e-12)));extra=std::min(extra,left-used);c[i].budget=2+extra;used+=extra;}
    for(size_t i=0;used<left;++i,++used)++c[i%n].budget;
}
inline Ray screenRay(const Camera& c,int x,int y,int w,int h){
    const float sx=(2.f*(x+.5f)/w-1.f)*c.tanHalf,sy=(1.f-2.f*(y+.5f)/h)*c.tanHalf/c.aspect;
    return {c.eye,norm(c.f+c.r*sx+c.u*sy)};
}
}
