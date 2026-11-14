#include "m3d_gi_bake.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>

namespace m3d::gibake {
namespace {
constexpr uint64_t offset=14695981039346656037ull,prime=1099511628211ull;
uint64_t hashBytes(uint64_t h,const void* ptr,size_t n){
    const auto* b=static_cast<const unsigned char*>(ptr);
    for(size_t i=0;i<n;++i)h=(h^uint64_t(b[i]))*prime;
    return h;
}
template<class T> uint64_t add(uint64_t h,const T& v){return hashBytes(h,&v,sizeof v);}
V multiply(V a,V b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
float srgb(float x){x=clampf(x,0.f,1.f);return x<=.04045f?x/12.92f:
    std::pow((x+.055f)/1.055f,2.4f);}
V albedo(const Scene& scene,int index){
    if(index<0||size_t(index)>=scene.mat.size())return {.5f,.5f,.5f};
    const Material& m=scene.mat[size_t(index)];
    V c{.55f,.55f,.55f};
    if(!m.colorMip.empty()&&!m.colorMip.back().p.empty()){
        const uint32_t rgb=m.colorMip.back().p.front();
        c={srgb(float((rgb>>16)&255)/255.f),srgb(float((rgb>>8)&255)/255.f),
           srgb(float(rgb&255)/255.f)};
    }else if(!m.mip.empty()&&!m.mip.back().p.empty()){
        const uint16_t rgb=m.mip.back().p.front();
        c={srgb(float((rgb>>11)&31)/31.f),srgb(float((rgb>>5)&63)/63.f),
           srgb(float(rgb&31)/31.f)};
    }
    return c*(1.f-clampf(m.metallic,0.f,1.f))*.82f;
}
V sunAt(float hour,float& intensity){
    const float t=clampf((hour-6.f)/13.f,0.f,1.f);
    const float day=std::max(0.f,std::sin(PI*t));
    const float warm=std::max(clampf(1.f-std::abs(hour-18.3f)/2.3f,0.f,1.f),
        .72f*clampf(1.f-std::abs(hour-7.1f)/1.8f,0.f,1.f));
    const float a=(hour-14.75f)*.12f,c=std::cos(a),s=std::sin(a);
    intensity=day;
    return norm({.70f*c+.714f*s,-.16f+.92f*day,.70f*s-.714f*c});
}
V sunColor(float hour){
    const float warm=std::max(clampf(1.f-std::abs(hour-18.3f)/2.3f,0.f,1.f),
        .72f*clampf(1.f-std::abs(hour-7.1f)/1.8f,0.f,1.f));
    return {1.f,1.f-.25f*warm,1.f-.55f*warm};
}
bool visible(const Scene& scene,V start,V direction,float maxDistance){
    Hit blocker;
    closest(scene,Ray{start,direction},blocker,nullptr,false,-1,true);
    return blocker.id<0||blocker.t>=maxDistance;
}
V skyColor(float hour,V direction){
    const float daytime=std::max(0.f,std::sin(PI*clampf((hour-6.f)/13.f,0.f,1.f)));
    const float hemi=clampf(direction.y*.5f+.5f,0.f,1.f);
    return V{.115f,.16f,.255f}*(daytime*(.30f+.70f*hemi))
        +V{.012f,.011f,.018f}*(1.f-daytime);
}
V emission(const Scene& scene,int material,float hour){
    if(material<0||size_t(material)>=scene.mat.size())return {};
    const auto& n=scene.mat[size_t(material)].name;
    const float night=clampf((hour-17.f)/2.f,0.f,1.f);
    if(n=="lamp")return V{1.f,.37f,.14f}*(.12f+night*.56f);
    if(n=="led")return V{1.f,.69f,.42f}*(.10f+night*.39f);
    if(n=="neon")return V{.38f,.64f,1.f}*.34f;
    if(n=="windowlit")return V{1.f,.58f,.32f}*(.12f+.38f*night);
    return {};
}
V incident(const Scene& scene,const Hit& hit,float hour,V solar,float daytime){
    const V normal=norm(hit.n);
    const V origin=hit.p+normal*.035f;
    const V diffuse=albedo(scene,hit.mat);
    const float angle=std::max(0.f,dot(normal,solar));
    V direct{};
    if(daytime>.001f&&angle>.001f&&visible(scene,origin,solar,1000.f))
        direct=multiply(diffuse,sunColor(hour))*(daytime*angle*.80f);
    // Refracted/reflected sky influence is occlusion-tested, not an orange filter.
    const V skyDirection=norm(normal+V{0.f,1.f,0.f});
    const bool open=visible(scene,origin,skyDirection,38.f);
    const float hemi=clampf(.5f+.5f*normal.y,0.f,1.f);
    const V ambient=multiply(diffuse,skyColor(hour,skyDirection))
        *(open?.55f*(.25f+.75f*hemi):.055f);
    return direct+ambient+emission(scene,hit.mat,hour);
}
struct RaySample { Hit hit{},second{};bool firstVisibleSky=false; };
float unitFloat(uint32_t v){return float(v&0xffffffu)/16777216.f;}
V directionAt(unsigned i,unsigned total,unsigned seed){
    // Low-discrepancy-ish sphere, deterministic per-probe rotation.
    const float u=(float(i)+.5f)/float(total);
    const float y=1.f-2.f*u;
    const float azimuth=(float(i)*2.39996323f+float(seed%4096)*.00306796f);
    const float radius=std::sqrt(std::max(0.f,1.f-y*y));
    return {std::cos(azimuth)*radius,y,std::sin(azimuth)*radius};
}
V chooseBounce(V normal,unsigned rayIndex,unsigned seed){
    const V axis=std::abs(normal.y)<.9f?V{0,1,0}:V{1,0,0};
    const V tangent=norm(cross(axis,normal)),bitangent=norm(cross(normal,tangent));
    const float u=unitFloat((seed+rayIndex*1664525u)*1013904223u);
    const float phi=6.2831853f*unitFloat((seed^0x9e3779b9u)+rayIndex*2246822519u);
    const float radial=std::sqrt(u),n=std::sqrt(std::max(0.f,1.f-u));
    return norm(tangent*(std::cos(phi)*radial)+bitangent*(std::sin(phi)*radial)+normal*n);
}
template<class T> void put(std::ofstream& o,const T& value){
    o.write(reinterpret_cast<const char*>(&value),std::streamsize(sizeof value));
}
template<class T> bool get(std::ifstream& f,T& v){
    f.read(reinterpret_cast<char*>(&v),std::streamsize(sizeof v));return bool(f);
}
} // namespace
uint64_t signature(const Scene& scene){
    uint64_t h=offset;
    h=add(h,uint64_t(scene.obj.size()));
    for(const Obj& o:scene.obj){
        h=add(h,o.kind);h=add(h,o.mat);
        for(float v:{o.mn.x,o.mn.y,o.mn.z,o.mx.x,o.mx.y,o.mx.z,
                     o.a.x,o.a.y,o.a.z,o.b.x,o.b.y,o.b.z,
                     o.c.x,o.c.y,o.c.z,o.n0.x,o.n0.y,o.n0.z,
                     o.n1.x,o.n1.y,o.n1.z,o.n2.x,o.n2.y,o.n2.z,
                     o.r,o.r1,o.k})h=add(h,v);
    }
    h=add(h,uint64_t(scene.mat.size()));
    for(const Material& m:scene.mat){
        h=hashBytes(h,m.name.data(),m.name.size());h=add(h,m.metallic);
        h=add(h,m.reflectivityPercent);h=add(h,m.transmissionPercent);
        if(!m.colorMip.empty()){
            const auto& level=m.colorMip.front();
            h=add(h,level.size);h=add(h,uint64_t(level.p.size()));
            for(size_t k=0;k<level.p.size()&&level.p.size();k+=std::max<size_t>(1,level.p.size()/64))
                h=add(h,level.p[k]);
        }else if(!m.mip.empty()){
            const auto& level=m.mip.front();h=add(h,level.size);
            for(size_t k=0;k<level.p.size();k+=std::max<size_t>(1,level.p.size()/64))
                h=add(h,level.p[k]);
        }
    }
    h=add(h,scene.finiteGround);
    return h;
}
bool bake(const Scene& scene,const Options& opt,Table& out,std::string& error){
    if(opt.nx<2||opt.ny<2||opt.nz<2||opt.nx>48||opt.ny>48||opt.nz>48||
       opt.rays<4||opt.rays>128||opt.frames<2||opt.frames>64||
       opt.bounces<1||opt.bounces>3||opt.stepHours<=0||scene.obj.empty()||
       scene.node.empty()||scene.root<0){
        error="Invalid GI bake options or missing BVH";return false;
    }
    V lo{INF,INF,INF},hi{-INF,-INF,-INF};
    for(const Obj& o:scene.obj){
        const V a=o.mn,b=o.mx;
        if(!std::isfinite(a.x)||!std::isfinite(a.y)||!std::isfinite(a.z)||
           !std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||
           a.x>b.x||a.y>b.y||a.z>b.z)continue;
        lo={std::min(lo.x,a.x),std::min(lo.y,a.y),std::min(lo.z,a.z)};
        hi={std::max(hi.x,b.x),std::max(hi.y,b.y),std::max(hi.z,b.z)};
    }
    if(!std::isfinite(lo.x)){error="No valid scene bounds";return false;}
    const V size=hi-lo,padding{std::max(1.f,size.x*.01f),
      std::max(.8f,size.y*.03f),std::max(1.f,size.z*.01f)};
    out=Table{};
    out.nx=opt.nx;out.ny=opt.ny;out.nz=opt.nz;out.frames=opt.frames;
    out.firstHour=opt.firstHour;out.stepHours=opt.stepHours;
    out.origin=lo-padding;out.span=hi-lo+padding*2.f;
    out.sceneHash=signature(scene);
    const size_t count=out.voxelCount();
    out.rgba.resize(count*opt.frames*4u,0.f);
    const float rayLimit=clampf(std::max({out.span.x,out.span.y,out.span.z})/10.f,8.f,40.f);
    for(unsigned z=0;z<opt.nz;++z)
      for(unsigned y=0;y<opt.ny;++y)
        for(unsigned x=0;x<opt.nx;++x){
        const size_t voxel=(size_t(z)*opt.ny+y)*opt.nx+x;
        const V start=out.origin+V{
           out.span.x*(float(x)+.5f)/float(opt.nx),
           out.span.y*(float(y)+.5f)/float(opt.ny),
           out.span.z*(float(z)+.5f)/float(opt.nz)};
        std::vector<RaySample> paths(opt.rays);
        unsigned skyRays=0;
        for(unsigned k=0;k<opt.rays;++k){
            const V dir=directionAt(k,opt.rays,unsigned(voxel));
            Hit h{};
            closest(scene,Ray{start,dir},h,nullptr,false,-1,true);
            if(h.id<0||h.t>rayLimit){++skyRays;paths[k].firstVisibleSky=true;continue;}
            paths[k].hit=h;
            if(opt.bounces>1){
                const V outDir=chooseBounce(norm(h.n),k,unsigned(voxel));
                Hit h2{};
                closest(scene,Ray{h.p+norm(h.n)*.045f,outDir},h2,
                        nullptr,false,-1,true);
                if(h2.id>=0&&h2.t<rayLimit)paths[k].second=h2;
            }
        }
        const float open=float(skyRays)/float(opt.rays);
        for(unsigned f=0;f<opt.frames;++f){
            const float hour=opt.firstHour+float(f)*opt.stepHours;
            float daylight=0.f;
            const V solar=sunAt(hour,daylight);
            V irradiance{};
            for(unsigned k=0;k<opt.rays;++k){
                if(paths[k].firstVisibleSky){
                    irradiance=irradiance+skyColor(hour,directionAt(k,opt.rays,unsigned(voxel)));
                    continue;
                }
                const RaySample& path=paths[k];
                V energy=incident(scene,path.hit,hour,solar,daylight);
                if(path.second.id>=0){
                    const V secondary=incident(scene,path.second,hour,solar,daylight);
                    energy=energy+multiply(albedo(scene,path.hit.mat),secondary)*.22f;
                }
                irradiance=irradiance+energy;
            }
            irradiance=irradiance/float(opt.rays);
            const size_t pos=(size_t(f)*count+voxel)*4u;
            out.rgba[pos]=irradiance.x;out.rgba[pos+1]=irradiance.y;
            out.rgba[pos+2]=irradiance.z;out.rgba[pos+3]=open;
        }
    }
    return true;
}
void hourIndices(const Table& t,float hour,unsigned& low,unsigned& high,float& w){
    const float x=clampf((hour-t.firstHour)/t.stepHours,0.f,float(t.frames-1));
    low=unsigned(x);high=std::min(low+1,t.frames-1);w=x-float(low);
}
V sample(const Table& t,V position,float hour){
    if(!t.valid())return {};
    unsigned lo=0,hi=0;float w=0;
    hourIndices(t,hour,lo,hi,w);
    const float x=clampf((position.x-t.origin.x)/std::max(.001f,t.span.x),0.f,.9999f);
    const float y=clampf((position.y-t.origin.y)/std::max(.001f,t.span.y),0.f,.9999f);
    const float z=clampf((position.z-t.origin.z)/std::max(.001f,t.span.z),0.f,.9999f);
    const unsigned xi=std::min(unsigned(x*t.nx),t.nx-1);
    const unsigned yi=std::min(unsigned(y*t.ny),t.ny-1);
    const unsigned zi=std::min(unsigned(z*t.nz),t.nz-1);
    const size_t v=(size_t(zi)*t.ny+yi)*t.nx+xi;
    auto val=[&](unsigned time)->V{
        const size_t p=(size_t(time)*t.voxelCount()+v)*4u;
        return {t.rgba[p],t.rgba[p+1],t.rgba[p+2]};
    };
    return val(lo)*(1.f-w)+val(hi)*w;
}
bool writeFile(const std::filesystem::path& path,const Table& t,std::string& error){
    if(!t.valid()||t.rgba.size()>64u*1024u*1024u/sizeof(float)){
        error="Invalid or oversized GI table";return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec){error="Cannot create GI cache directory";return false;}
    const auto temp=path.wstring()+L".tmp";
    std::ofstream out(std::filesystem::path(temp),std::ios::binary|std::ios::trunc);
    if(!out){error="Cannot open GI cache temp file";return false;}
    constexpr uint32_t magic=0x32494743u,version=2u;
    put(out,magic);put(out,version);put(out,t.sceneHash);
    put(out,t.nx);put(out,t.ny);put(out,t.nz);put(out,t.frames);
    put(out,t.firstHour);put(out,t.stepHours);
    for(float f:{t.origin.x,t.origin.y,t.origin.z,t.span.x,t.span.y,t.span.z})put(out,f);
    const uint64_t count=uint64_t(t.rgba.size());
    const uint64_t digest=hashBytes(offset,t.rgba.data(),size_t(count)*sizeof(float));
    put(out,count);put(out,digest);
    out.write(reinterpret_cast<const char*>(t.rgba.data()),
              std::streamsize(count*sizeof(float)));
    out.close();
    if(!out){error="Cannot write GI cache payload";return false;}
    std::filesystem::rename(std::filesystem::path(temp),path,ec);
    if(ec){
        std::filesystem::copy_file(std::filesystem::path(temp),path,
          std::filesystem::copy_options::overwrite_existing,ec);
        std::filesystem::remove(std::filesystem::path(temp));
    }
    if(ec){error="Cannot publish GI cache";return false;}
    return true;
}
bool readFile(const std::filesystem::path& path,Table& table,std::string& error){
    table=Table{};
    std::ifstream in(path,std::ios::binary);
    if(!in){error="GI cache file does not exist";return false;}
    uint32_t magic=0,version=0;Table tmp;
    if(!get(in,magic)||!get(in,version)||magic!=0x32494743u||version!=2u||
       !get(in,tmp.sceneHash)||!get(in,tmp.nx)||!get(in,tmp.ny)||
       !get(in,tmp.nz)||!get(in,tmp.frames)||!get(in,tmp.firstHour)||
       !get(in,tmp.stepHours)){
        error="GI cache version/header mismatch";return false;
    }
    if(tmp.nx<2||tmp.ny<2||tmp.nz<2||tmp.frames<2||
       tmp.nx>48||tmp.ny>48||tmp.nz>48||tmp.frames>64||
       tmp.stepHours<=0||!std::isfinite(tmp.firstHour)){
        error="Invalid GI cache dimensions";return false;
    }
    for(float* f:{&tmp.origin.x,&tmp.origin.y,&tmp.origin.z,
                  &tmp.span.x,&tmp.span.y,&tmp.span.z})
        if(!get(in,*f)){error="GI cache bounds truncated";return false;}
    uint64_t count=0,digest=0;
    if(!get(in,count)||!get(in,digest)||
       count!=tmp.voxelCount()*size_t(tmp.frames)*4u||
       count>64u*1024u*1024u/sizeof(float)){
        error="GI cache payload length invalid";return false;
    }
    tmp.rgba.resize(size_t(count));
    in.read(reinterpret_cast<char*>(tmp.rgba.data()),
            std::streamsize(count*sizeof(float)));
    if(!in||digest!=hashBytes(offset,tmp.rgba.data(),size_t(count)*sizeof(float))){
        error="GI cache truncated or checksum mismatch";return false;
    }
    for(float f:tmp.rgba)if(!std::isfinite(f)){
        error="GI cache contains invalid lighting";return false;
    }
    table=std::move(tmp);
    return true;
}
} // namespace m3d::gibake
