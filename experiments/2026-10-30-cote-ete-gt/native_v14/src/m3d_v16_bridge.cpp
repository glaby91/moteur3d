#include "m3d_v16_bridge.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numeric>
#include <random>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace m3d::v16 {
namespace {
float f(const QJsonValue& v, float fallback=0.f) { return v.isDouble()?float(v.toDouble()):fallback; }
V v3(const QJsonValue& v,V fallback={}) {
    if (!v.isArray()) return fallback;
    const auto a=v.toArray();
    if (a.size()!=3) return fallback;
    return {f(a[0]),f(a[1]),f(a[2])};
}
float axis(V v,int i){return i==0?v.x:i==1?v.y:v.z;}
V vmin(V a,V b){return{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)};}
V vmax(V a,V b){return{std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)};}
int byte(float v){return int(std::lround(clampf(v,0.f,1.f)*255));}
uint32_t rgb(V v){return uint32_t((byte(v.x)<<16)|(byte(v.y)<<8)|byte(v.z));}
uint16_t rgb565(V c){return uint16_t((byte(c.x)*31/255)<<11|(byte(c.y)*63/255)<<5|(byte(c.z)*31/255));}
V unpack(uint32_t c){return{float(c>>16&255)/255,float(c>>8&255)/255,float(c&255)/255};}
V unpack565(uint16_t c){return{float(c>>11)*1.f/31,float(c>>5&63)/63,float(c&31)/31};}
float srgbToLinear(float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
float linearToSrgb(float c){return c<=.0031308f?c*12.92f:1.055f*std::pow(c,1.f/2.4f)-.055f;}
V srgbToLinear(V c){return{srgbToLinear(c.x),srgbToLinear(c.y),srgbToLinear(c.z)};}
V linearToSrgb(V c){return{linearToSrgb(c.x),linearToSrgb(c.y),linearToSrgb(c.z)};}
uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return x;}
float noise(int x,int y,uint32_t seed){return float(hash32(uint32_t(x)*73856093u ^ uint32_t(y)*19349663u ^ seed)&0xffffu)/65535.f;}
V scale(V a,V b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
float smoothNoise(float x,float y,uint32_t seed) {
    const int ix=int(std::floor(x)),iy=int(std::floor(y));
    const float fx=x-ix,fy=y-iy;
    const float sx=fx*fx*(3.f-2.f*fx),sy=fy*fy*(3.f-2.f*fy);
    const float a=noise(ix,iy,seed),b=noise(ix+1,iy,seed);
    const float c=noise(ix,iy+1,seed),d=noise(ix+1,iy+1,seed);
    return ((a+(b-a)*sx)*(1.f-sy)+(c+(d-c)*sx)*sy);
}


void triangle(Scene& scene,V a,V b,V c,V uv0,V uv1,V uv2,int material,V normal={}) {
    const V e1=b-a,e2=c-a;
    const V n=length(normal)>0?norm(normal):norm(cross(e1,e2));
    if (length(cross(e1,e2))<1e-8f) return;
    Obj o;
    o.kind=Kind::Triangle;o.mat=uint8_t(material);
    o.a=a;o.b=b;o.c=c;o.n0=n;o.n1=n;o.n2=n;
    o.uv0=uv0;o.uv1=uv1;o.uv2=uv2;
    o.mn=vmin(a,vmin(b,c));o.mx=vmax(a,vmax(b,c));
    o.center=e1;o.core=e2;
    o.uv0.z=dot(e1,e1);o.uv1.z=dot(e1,e2);o.uv2.z=dot(e2,e2);
    const float d=o.uv0.z*o.uv2.z-o.uv1.z*o.uv1.z;
    o.len2=std::abs(d)>1e-20f?1.f/d:0.f;
    scene.obj.push_back(o);
}
void box(Scene& scene,V position,V size,float rotationY,int mat) {
    const float rad=rotationY*PI/180.f,cs=std::cos(rad),sn=std::sin(rad);
    auto world=[&](float x,float y,float z)->V{
        return {position.x+x*cs+z*sn,position.y+y,position.z-x*sn+z*cs};
    };
    const float x=size.x*.5f,y=size.y*.5f,z=size.z*.5f;
    const std::array<std::array<V,4>,6> faces{{
        {{{-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z}}},
        {{{x,-y,-z},{-x,-y,-z},{-x,y,-z},{x,y,-z}}},
        {{{x,-y,z},{x,-y,-z},{x,y,-z},{x,y,z}}},
        {{{-x,-y,-z},{-x,-y,z},{-x,y,z},{-x,y,-z}}},
        {{{-x,y,z},{x,y,z},{x,y,-z},{-x,y,-z}}},
        {{{-x,-y,-z},{x,-y,-z},{x,-y,z},{-x,-y,z}}}
    }};
    for (int k=0;k<6;k++){
        const auto& v=faces[k];
        V a=world(v[0].x,v[0].y,v[0].z),b=world(v[1].x,v[1].y,v[1].z),
          c=world(v[2].x,v[2].y,v[2].z),d=world(v[3].x,v[3].y,v[3].z);
        // Full image per face: both the original 9.5 x 4.3 m Energetik
        // fresco and the user's portrait on the 8 x 13 m OUTER facade.
        // These image assets must not repeat at the regular PBR tile rate.
        const bool uniqueArt=mat>=0&&size_t(mat)<scene.mat.size()&&
          (scene.mat[size_t(mat)].name=="cse_energetik_fresco"||
           scene.mat[size_t(mat)].name=="cse_personnage_facade");
        const float us=uniqueArt?1.f:(k<2?size.x:k<4?size.z:size.x)*.5f;
        const float vs=uniqueArt?1.f:(k<4?size.y:size.z)*.5f;
        triangle(scene,a,b,c,{0,0,0},{us,0,0},{us,vs,0},mat);
        triangle(scene,a,c,d,{0,0,0},{us,vs,0},{0,vs,0},mat);
    }
}
void sphere(Scene& scene,V position,V size,int mat) {
    constexpr int S=12,R=8;
    auto pt=[&](int i,int j)->V {
        const float a=float(i)*PI/R,b=float(j)*2*PI/S;
        return {position.x+size.x*.5f*std::sin(a)*std::cos(b),
                position.y+size.y*.5f*std::cos(a),
                position.z+size.z*.5f*std::sin(a)*std::sin(b)};
    };
    for(int i=0;i<R;i++)for(int j=0;j<S;j++){
        V a=pt(i,j),b=pt(i+1,j),c=pt(i+1,j+1),d=pt(i,j+1);
        const auto add=[&](V p,V q,V r,V u,V v,V w){
            V n=norm(cross(q-p,r-p));
            if(dot(n,p-position)<0.f){std::swap(q,r);std::swap(v,w);}
            triangle(scene,p,q,r,u,v,w,mat);
        };
        if(i>0) add(a,b,d,{float(j)/S,float(i)/R,0},{float(j)/S,float(i+1)/R,0},{float(j+1)/S,float(i)/R,0});
        if(i<R-1)add(b,c,d,{float(j)/S,float(i+1)/R,0},{float(j+1)/S,float(i+1)/R,0},{float(j+1)/S,float(i)/R,0});
    }
}
// Organic, lightweight foliage silhouette; shared between CPU and Compute backends.
// Geometry remains triangles: no alpha-blending and no engine-specific special case.
void foliageCanopy(Scene& scene,V position,V size,int material) {
    constexpr int segments=18,rings=9;
    const float halfX=size.x*.5f,halfY=size.y*.5f,halfZ=size.z*.5f;
    auto point=[&](int ring,int segment)->V {
        const float latitude=float(ring)*PI/rings;
        const float azimuth=float(segment)*2.f*PI/segments;
        const float ripple=1.f+.075f*std::sin(5.f*azimuth+2.f*latitude)
                             +.055f*std::cos(9.f*azimuth-3.f*latitude)
                             +.04f*std::sin(7.f*latitude+4.f*azimuth);
        const float widest=std::sin(latitude)*ripple;
        return {position.x+halfX*widest*std::cos(azimuth),
                position.y+halfY*std::cos(latitude),
                position.z+halfZ*widest*std::sin(azimuth)};
    };
    for(int ring=0;ring<rings;++ring)
        for(int segment=0;segment<segments;++segment) {
            const V a=point(ring,segment),b=point(ring+1,segment),
                    c=point(ring+1,segment+1),d=point(ring,segment+1);
            const auto add=[&](V p,V q,V r,V uv0,V uv1,V uv2) {
                if(dot(cross(q-p,r-p),p-position)<0) {
                    std::swap(q,r);
                    std::swap(uv1,uv2);
                }
                triangle(scene,p,q,r,uv0,uv1,uv2,material);
            };
            if(ring>0) add(a,b,d,
                {float(segment)/segments,float(ring)/rings,0},
                {float(segment)/segments,float(ring+1)/rings,0},
                {float(segment+1)/segments,float(ring)/rings,0});
            if(ring<rings-1) add(b,c,d,
                {float(segment)/segments,float(ring+1)/rings,0},
                {float(segment+1)/segments,float(ring+1)/rings,0},
                {float(segment+1)/segments,float(ring)/rings,0});
        }
    // Sparse leaf-shaped facets break up the perfectly round "green ball" outline.
    // Fixed deterministic phases ensure stable benchmarks and reproducible images.
    for(int i=0;i<36;++i) {
        const float azimuth=float(i)*2.39996323f;
        const float elevation=.35f*std::sin(i*1.731f);
        const float cy=std::sin(elevation),radius=std::cos(elevation);
        const V n=norm({radius*std::cos(azimuth),cy,radius*std::sin(azimuth)});
        const V center={position.x+n.x*halfX*.93f,position.y+n.y*halfY*.93f,
                        position.z+n.z*halfZ*.93f};
        const V helper=std::abs(n.y)<.8f?V{0,1,0}:V{1,0,0};
        const V tangent=norm(cross(n,helper));
        const float width=(.12f+.035f*float(i%3))*std::min(size.x,size.z);
        const float height=width*(1.15f+.20f*float(i%4));
        const V base=center-tangent*(width*.45f);
        const V end=center+tangent*(width*.4f)+V{0,height*.24f,0}+n*(height*.24f);
        const V sides=center+V{0,height*.4f,0};
        triangle(scene,base,sides,end,{0,0,0},{.5f,1.f,0},{1,0,0},material,n);
    }
}
int buildBvh(Scene& s,int first,int count){
    Node node;node.mn={INF,INF,INF};node.mx={-INF,-INF,-INF};
    V cmin{INF,INF,INF},cmax{-INF,-INF,-INF};
    for(int i=first;i<first+count;i++){
        const auto& o=s.obj[s.order[size_t(i)]];
        node.mn=vmin(node.mn,o.mn);node.mx=vmax(node.mx,o.mx);
        V centroid=(o.mn+o.mx)*.5f;
        cmin=vmin(cmin,centroid);cmax=vmax(cmax,centroid);
    }
    const int index=int(s.node.size());
    s.node.push_back(node);
    if(count<=6){s.node[size_t(index)].start=first;s.node[size_t(index)].count=count;return index;}
    V d=cmax-cmin;
    const int axisId=(d.x>=d.y&&d.x>=d.z)?0:((d.y>=d.z)?1:2);
    const int midpoint=first+count/2;
    std::nth_element(s.order.begin()+first,s.order.begin()+midpoint,s.order.begin()+first+count,[&](uint32_t a,uint32_t b){
        const Obj& x=s.obj[a];const Obj& y=s.obj[b];
        return axis((x.mn+x.mx)*.5f,axisId)<axis((y.mn+y.mx)*.5f,axisId);
    });
    const int left=buildBvh(s,first,midpoint-first);
    const int right=buildBvh(s,midpoint,first+count-midpoint);
    s.node[size_t(index)].left=left;s.node[size_t(index)].right=right;
    return index;
}
// Content-addressed, persistent PBR mip cache. The renderer reads exactly the
// same color/normal/roughness texels; no reduction in texture resolution,
// filtering quality, material detail or illumination. Bump the format/key
// version whenever the procedural PBR math or its dependencies change.
// Set M3D_V17_DISABLE_PBR_CACHE=1 for a cold A/B run. An optional
// M3D_V17_PBR_CACHE_DIR isolates test or benchmark data.
constexpr uint32_t kPbrCacheMagic=0x43535750u; // "PWSC", native little-endian
constexpr uint32_t kPbrCacheVersion=1u;
struct PbrCacheHeader{
    uint32_t magic,version,resolution,levels;
};
QString pbrCachePath(V base,float roughness,const QString& pattern,int resolution) {
    const char* disabled=std::getenv("M3D_V17_DISABLE_PBR_CACHE");
    if(disabled&&std::strcmp(disabled,"1")==0)return {};
    const char* overrideRoot=std::getenv("M3D_V17_PBR_CACHE_DIR");
    const QString root=overrideRoot&&*overrideRoot
        ?QString::fromLocal8Bit(overrideRoot)
        :QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
            +QStringLiteral("/CSE-WARP/PBR/v1");
    if(root.isEmpty())return {};
    // Raw IEEE-754 parameter bytes identify every arithmetic input exactly.
    QByteArray key("CSE-WARP-PBR-20261111-v1|Qt");
    key.append(QT_VERSION_STR);
    key.append(pattern.toUtf8());
    key.append('\0');
    key.append(reinterpret_cast<const char*>(&base),sizeof(base));
    key.append(reinterpret_cast<const char*>(&roughness),sizeof(roughness));
    key.append(reinterpret_cast<const char*>(&resolution),sizeof(resolution));
    const auto digest=QCryptographicHash::hash(key,QCryptographicHash::Sha256).toHex();
    return QDir(root).filePath(QString::fromLatin1(digest)+QStringLiteral(".pbr"));
}
uint32_t mipLevelCount(uint32_t size) {
    uint32_t count=0;
    for(;size;size>>=1)++count;
    return count;
}
qint64 pbrCacheLength(uint32_t resolution) {
    qint64 total=qint64(sizeof(PbrCacheHeader))+32;
    for(uint32_t size=resolution;size;size>>=1)
        total+=qint64(size)*size*8; // RGB32 + normal565 + roughness565
    return total;
}
bool loadPbrCache(const QString& path,Material& mat,int resolution) {
    QFile in(path);
    if(!in.open(QIODevice::ReadOnly)||in.size()!=pbrCacheLength(uint32_t(resolution)))
        return false;
    PbrCacheHeader header{};
    if(in.read(reinterpret_cast<char*>(&header),sizeof(header))!=qint64(sizeof(header))
       ||header.magic!=kPbrCacheMagic||header.version!=kPbrCacheVersion
       ||header.resolution!=uint32_t(resolution)
       ||header.levels!=mipLevelCount(uint32_t(resolution)))return false;
    std::vector<ColorLevel> color;
    std::vector<Level> normal,rough;
    color.reserve(header.levels);normal.reserve(header.levels);rough.reserve(header.levels);
    QCryptographicHash digest(QCryptographicHash::Sha256);
    const auto readPixels=[&](auto& pixels)->bool {
        const qint64 bytes=qint64(pixels.size()*sizeof(pixels[0]));
        auto* raw=reinterpret_cast<char*>(pixels.data());
        if(in.read(raw,bytes)!=bytes)return false;
        digest.addData(raw,bytes);
        return true;
    };
    for(uint32_t size=uint32_t(resolution);size;size>>=1){
        ColorLevel c;c.size=size;c.p.resize(size_t(size)*size);
        Level n;n.size=size;n.p.resize(size_t(size)*size);
        Level r;r.size=size;r.p.resize(size_t(size)*size);
        if(!readPixels(c.p)||!readPixels(n.p)||!readPixels(r.p))return false;
        color.push_back(std::move(c));
        normal.push_back(std::move(n));
        rough.push_back(std::move(r));
    }
    const QByteArray expected=in.read(32);
    if(expected.size()!=32||digest.result()!=expected)return false;
    mat.colorMip=std::move(color);
    mat.normalMip=std::move(normal);
    mat.roughnessMip=std::move(rough);
    return true;
}
void savePbrCache(const QString& path,const Material& mat,int resolution) {
    const uint32_t count=mipLevelCount(uint32_t(resolution));
    if(mat.colorMip.size()!=count||mat.normalMip.size()!=count||
       mat.roughnessMip.size()!=count)return;
    if(!QDir().mkpath(QFileInfo(path).absolutePath()))return;
    QSaveFile out(path);
    if(!out.open(QIODevice::WriteOnly))return;
    const PbrCacheHeader header{kPbrCacheMagic,kPbrCacheVersion,
                                uint32_t(resolution),count};
    if(out.write(reinterpret_cast<const char*>(&header),sizeof(header))
       !=qint64(sizeof(header))){out.cancelWriting();return;}
    QCryptographicHash digest(QCryptographicHash::Sha256);
    const auto writePixels=[&](const auto& pixels)->bool {
        const qint64 bytes=qint64(pixels.size()*sizeof(pixels[0]));
        const auto* raw=reinterpret_cast<const char*>(pixels.data());
        if(out.write(raw,bytes)!=bytes)return false;
        digest.addData(raw,bytes);
        return true;
    };
    for(uint32_t level=0;level<count;++level){
        if(!writePixels(mat.colorMip[level].p) ||
           !writePixels(mat.normalMip[level].p) ||
           !writePixels(mat.roughnessMip[level].p)){
            out.cancelWriting();return;
        }
    }
    const QByteArray checksum=digest.result();
    if(out.write(checksum)!=checksum.size()){out.cancelWriting();return;}
    if(!out.commit())return;
    // Bound long-term disk usage without evicting textures from active scenes.
    constexpr qint64 kMaxDiskCache=4ll*1024*1024*1024;
    QDir dir(QFileInfo(path).absolutePath());
    const auto entries=dir.entryInfoList({QStringLiteral("*.pbr")},
        QDir::Files,QDir::Time|QDir::Reversed); // oldest first
    qint64 total=0;
    for(const auto& entry:entries)total+=entry.size();
    for(const auto& entry:entries){
        if(total<=kMaxDiskCache)break;
        if(QFile::remove(entry.absoluteFilePath()))total-=entry.size();
    }
}

void generatePbr(Material& mat,V base,float roughness,const QString& pattern,int resolution) {
    const QString cacheFile=pbrCachePath(base,roughness,pattern,resolution);
    if(!cacheFile.isEmpty() && loadPbrCache(cacheFile,mat,resolution))return;
    ColorLevel first;first.size=uint32_t(resolution);first.p.resize(size_t(resolution)*resolution);
    Level nr;nr.size=uint32_t(resolution);nr.p.resize(first.p.size());
    Level rr;rr.size=uint32_t(resolution);rr.p.resize(first.p.size());
    const uint32_t seed=hash32(uint32_t(qHash(pattern)));
    // Reusable material-level height field: individual staggered bricks and
    // depressed mortar are represented only in textures, never extra geometry.
    auto brickHeight=[](float u,float v){
        const float row=v*8.f;
        const float rowIndex=std::floor(row);
        const float offset=(int(rowIndex)&1)?.5f:0.f;
        const float column=u*14.f+offset;
        const float x=column-std::floor(column),y=row-rowIndex;
        const float dx=std::min(x,1.f-x),dy=std::min(y,1.f-y);
        const float edge=clampf(std::min(dx/.08f,dy/.14f),0.f,1.f);
        return edge*edge*(3.f-2.f*edge);
    };
    // Per-texel evaluation is pure and independent. Split rows across CPU
    // workers without allocating extra image buffers or changing the order of
    // arithmetic in any texel. The serial path remains available for A/B QA.
    const auto generateRows=[&](int firstRow,int lastRow){
    for(int y=firstRow;y<lastRow;y++)for(int x=0;x<resolution;x++){
        const float u=float(x)/resolution,v=float(y)/resolution;
        const float n=smoothNoise(float(x)/7.f,float(y)/7.f,seed),fine=noise(x,y,seed^0x9e3779b9u);
        float factor=1.f;
        // Subtle large-scale patterns read better than high-frequency salt-and-pepper.
        // All patterns feed the same mipmap pipeline used by CPU and GPU.
        if(pattern=="wood") {
            const float grain=std::sin(u*93.f+2.f*std::sin(v*8.f)+
                                       .4f*std::sin(u*17.f+v*21.f));
            factor=.89f+.095f*grain+.035f*(n-.5f);
        }
        else if(pattern=="concrete"||pattern=="plaster") {
            factor=.965f+.065f*(n-.5f)+.012f*(fine-.5f);
        }
        else if(pattern=="brick") {
            // Staggered bonds, dark mortar and uneven fired-clay tint.
            const float height=brickHeight(u,v);
            const float coarse=smoothNoise(u*33.f,v*21.f,seed^0x7f4a7c15u);
            factor=.43f+.55f*height+.12f*(coarse-.5f)+.035f*(n-.5f);
        }
        else if(pattern=="stone") {
            const float veins=std::sin(u*12.f+v*8.f+std::sin(v*15.f)*1.5f);
            factor=.94f+.05f*veins+.035f*(n-.5f);
        }
        else if(pattern=="roof") {
            // Dark bituminous roof: broad wear patches, very fine grain.
            const float large=smoothNoise(u*9.f,v*9.f,seed^0x72c95aa9u);
            const float grain=smoothNoise(u*83.f,v*83.f,seed^0xb5297a4du);
            factor=.91f+.14f*(large-.5f)+.035f*(grain-.5f);
        }
        else if(pattern=="porcelain") {
            // Glazed interior tile: low-frequency clouds with very low bump.
            const float cloud=smoothNoise(u*5.f,v*5.f,seed^0x9e3779b9u);
            factor=.973f+.045f*(cloud-.5f);
        }
        else if(pattern=="fabric")
            factor=.91f+.065f*std::sin(u*260.f)*std::sin(v*260.f);
        else if(pattern=="lawn"||pattern=="foliage") {
            // Break up repetitive lattice/checkerboard patterns at Full HD.
            const float broad=smoothNoise(u*13.f,v*13.f,seed^0x85ebca6bu);
            const float medium=smoothNoise(u*41.f,v*41.f,seed^0xc2b2ae35u);
            factor=.92f+.15f*(broad-.5f)+.065f*(medium-.5f)+.035f*(n-.5f);
        }
        else if(pattern=="water") {
            // Gentle caustic-like light variation without opaque stripes.
            factor=.97f+.015f*std::sin(u*36.f+v*21.f);
        }
        else if(pattern=="glass")factor=1.f;
        else if(pattern=="metal")factor=.975f+.018f*(n-.5f);
        const V color=scale(base,{factor,factor,factor});
        const size_t i=size_t(y)*resolution+x;
        first.p[i]=rgb({clampf(color.x,0,1),clampf(color.y,0,1),clampf(color.z,0,1)});
        V normal={0,0,1};
        if(pattern=="water") {
            // Ripples are smooth and low amplitude, not per-texel white noise.
            const float a=2.f*PI*(u*4.f+v*2.f);
            const float b=2.f*PI*(u*2.f-v*3.f);
            normal=norm({.025f*std::cos(a)+.015f*std::cos(b),
                         .014f*std::sin(a)-.011f*std::sin(b),1.f});
        } else if(pattern=="glass"||pattern=="metal"||pattern=="porcelain") {
            normal={0,0,1};
        } else if(pattern=="brick") {
            const float du=1.f/float(resolution),dv=du;
            const float sx=brickHeight(u+du,v)-brickHeight(u-du,v);
            const float sy=brickHeight(u,v+dv)-brickHeight(u,v-dv);
            // Bright brick faces and recessed mortar joints, with one-pixel
            // gradients that mip-filter at distance.
            normal=norm({-sx*.57f,-sy*.57f,1.f});
        } else if(pattern=="wood") {
            // Longitudinal grain grooves: the relief follows the color fibers.
            // This is genuine tangent-space normal mapping, not image noise.
            const float phase=u*93.f+2.f*std::sin(v*8.f)+
                              .4f*std::sin(u*17.f+v*21.f);
            normal=norm({-.125f*std::cos(phase),
                         .028f*std::sin(phase)*std::cos(v*8.f),1.f});
        } else if(pattern=="stone") {
            const float vein=u*12.f+v*8.f+1.5f*std::sin(v*15.f);
            normal=norm({-.11f*std::cos(vein),
                         -.065f*std::cos(vein)+.028f*(n-.5f),1.f});
        } else if(pattern=="concrete"||pattern=="plaster"||pattern=="roof") {
            // Smooth, coherent height gradients react to grazing daylight;
            // old unrelated random X/Y normal offsets looked like flat grain.
            const float xf=float(x)/7.f,yf=float(y)/7.f;
            const float hx=smoothNoise(xf+1.f/7.f,yf,seed)-
                           smoothNoise(xf-1.f/7.f,yf,seed);
            const float hy=smoothNoise(xf,yf+1.f/7.f,seed)-
                           smoothNoise(xf,yf-1.f/7.f,seed);
            const float strength=pattern=="roof"?.10f:
                                 pattern=="concrete"?.33f:.24f;
            normal=norm({-hx*strength,-hy*strength,1.f});
        } else {
            const float d=(n-.5f)*.065f;
            normal=norm({d,d*.55f,1.f});
        }
        nr.p[i]=rgb565({(normal.x+1)*.5f,(normal.y+1)*.5f,(normal.z+1)*.5f});
        const float variation=pattern=="water"||pattern=="glass"?.006f:.055f;
        const float r=clampf(roughness+(n-.5f)*variation,.03f,.98f);
        rr.p[i]=rgb565({r,r,r});
    }
    };
    const char* forceSerial=std::getenv("M3D_V17_PBR_SERIAL");
    const bool serial=forceSerial && std::strcmp(forceSerial,"1")==0;
    const unsigned cpuCount=std::max(1u,std::thread::hardware_concurrency());
    // Fewer stripes than 128 texels/worker would waste scheduling overhead.
    // Cap to 8 concurrent workers on large CPU machines to avoid overloading
    // laptops with shared-memory GPUs; still scales well beyond 4 threads.
    const unsigned workerCount=(serial||resolution<512)?1u:
        std::min(8u,std::min(cpuCount,unsigned(resolution/128)));
    if(workerCount==1u)generateRows(0,resolution);
    else{
        std::vector<std::thread> workers;
        workers.reserve(workerCount-1);
        for(unsigned k=1;k<workerCount;++k){
            const int begin=int(size_t(k)*resolution/workerCount);
            const int end=int(size_t(k+1)*resolution/workerCount);
            workers.emplace_back([&,begin,end]{generateRows(begin,end);});
        }
        generateRows(0,int(resolution/workerCount));
        for(auto& worker:workers)worker.join();
    }
    mat.colorMip.push_back(std::move(first));
    mat.normalMip.push_back(std::move(nr));
    mat.roughnessMip.push_back(std::move(rr));
    while(mat.colorMip.back().size>1) {
        const auto& source=mat.colorMip.back();
        const auto& normals=mat.normalMip.back();
        const auto& rough=mat.roughnessMip.back();
        const int prev=int(source.size),next=prev/2;
        ColorLevel target;target.size=uint32_t(next);target.p.resize(size_t(next)*next);
        Level nt;nt.size=uint32_t(next);nt.p.resize(target.p.size());
        Level rt;rt.size=uint32_t(next);rt.p.resize(target.p.size());
        for(int y=0;y<next;y++)for(int x=0;x<next;x++){
            V sum{},normal{};float squared=0;
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
                const size_t at=size_t(y*2+dy)*prev+x*2+dx;
                sum=sum+srgbToLinear(unpack(source.p[at]));
                const V n=unpack565(normals.p[at])*2.f-V{1,1,1};normal=normal+n;
                const float r=unpack565(rough.p[at]).x;squared+=r*r;
            }
            sum=linearToSrgb(sum*.25f);normal=norm(normal);
            const float roughAvg=std::sqrt(squared*.25f);
            const size_t out=size_t(y)*next+x;
            target.p[out]=rgb(sum);
            nt.p[out]=rgb565((normal+V{1,1,1})*.5f);
            rt.p[out]=rgb565({roughAvg,roughAvg,roughAvg});
        }
        mat.colorMip.push_back(std::move(target));
        mat.normalMip.push_back(std::move(nt));
        mat.roughnessMip.push_back(std::move(rt));
    }
    if(!cacheFile.isEmpty())savePbrCache(cacheFile,mat,resolution);
}
// Genuine JPEG image albedo for scene-local paintings. Every mip is
// averaged in linear light (no fabricated mosaic or blurry resize chain).
bool replaceAlbedoWithImage(Material& mat,const QString& file,QString& error){
    QImage src(file);
    if(src.isNull()){error=QStringLiteral("Cannot decode image texture: %1").arg(file);return false;}
    if(mat.colorMip.empty()){error="Missing PBR mip array for image";return false;}
    const int size=int(mat.colorMip.front().size);
    QImage scaled=src.convertToFormat(QImage::Format_RGB32)
        .scaled(size,size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation)
        .mirrored(false,true); // V=0 on the bottom edge of the vertical plate.
    auto& first=mat.colorMip.front();
    for(int y=0;y<size;y++){
        const QRgb* row=reinterpret_cast<const QRgb*>(scaled.constScanLine(y));
        for(int x=0;x<size;x++){
            const QRgb px=row[x];
            first.p[size_t(y)*size+x]=
                (uint32_t(qRed(px))<<16)|(uint32_t(qGreen(px))<<8)|uint32_t(qBlue(px));
        }
    }
    for(size_t level=1;level<mat.colorMip.size();level++){
        const ColorLevel& previous=mat.colorMip[level-1];
        ColorLevel& next=mat.colorMip[level];
        const int d=int(next.size),from=int(previous.size);
        for(int y=0;y<d;y++)for(int x=0;x<d;x++){
            V sum{};
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
                sum=sum+srgbToLinear(unpack(previous.p[
                    size_t(2*y+dy)*from+size_t(2*x+dx)]));
            next.p[size_t(y)*d+x]=rgb(linearToSrgb(sum*.25f));
        }
    }
    return true;
}
bool finite3(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
} // namespace

BuildResult buildRuntime(const SceneDatabase& source,int maxTextureResolution,
    const std::function<void(int,const QString&)>& onProgress) {
    const auto report=[&](int percent,const QString& phase) {
        if(onProgress)onProgress(percent,phase);
    };
    BuildResult result;
    result.runtime.title=source.name;
    maxTextureResolution=std::clamp(maxTextureResolution,64,2048);
    if(source.sectors.empty()){result.error="No sectors";return result;}
    report(10,QStringLiteral("Lecture et validation des ressources de scène"));
    auto& render=result.runtime.renderScene;
    // Pavillon 01 has a real, bounded terrain mesh; never generate the old infinite ground plane.
    render.finiteGround=true;
    // V16 only uses authored lights from its JSON scene. Older V14 demonstrations
    // can keep their hardcoded legacy light rig without leaking lamps/cars
    // into new architectural or material-test scenes.
    render.useLegacyWorldLights=false;
    // Unique per-runtime token avoids re-uploading gigabytes of 2K mips
    // during camera movement. All later V16 editor material mutations increment
    // gpuCacheRevision; untracked legacy scenes still use a full content hash.
    static std::atomic<uint64_t> nextGpuSceneIdentity{1};
    render.gpuCacheIdentity=nextGpuSceneIdentity.fetch_add(1,std::memory_order_relaxed);
    render.gpuCacheRevision=0;
    QHash<QString,QJsonObject> meshes;
    QHash<QString,QJsonObject> sets;
    QVector<QJsonObject> instanceDocs;
    QVector<QJsonObject> materialDocs;
    for(const auto& sector:source.sectors){
        auto accumulate=[&](const QString& kind,const QString& key,QHash<QString,QJsonObject>& target){
            for(const auto& file:sector.resources.value(kind)){
                for(const auto& entry:file.data.value(key).toArray()){
                    const auto item=entry.toObject();
                    const QString id=item.value("id").toString();
                    if(!id.isEmpty()) target.insert(id,item);
                }
            }
        };
        accumulate("geometry","meshes",meshes);
        accumulate("pbr","sets",sets);
        for(const auto& file:sector.resources.value("materials"))
            for(const auto& item:file.data.value("materials").toArray())materialDocs.append(item.toObject());
        for(const auto& file:sector.resources.value("instances"))
            for(const auto& item:file.data.value("instances").toArray())instanceDocs.append(item.toObject());
        for(const auto& file:sector.resources.value("cameras"))
            for(const auto& item:file.data.value("cameras").toArray()){
                const auto j=item.toObject();
                CameraPreset c;c.id=j.value("id").toString();c.name=j.value("name").toString(c.id);
                c.eye=v3(j.value("position"));c.target=v3(j.value("target"));
                c.fov=f(j.value("fovDegrees"),55.f);
                result.runtime.cameras.push_back(c);
            }
        for(const auto& file:sector.resources.value("lighting")) {
            const float requestedHour=f(file.data.value("hour"),15.f);
            if(std::isfinite(requestedHour) && requestedHour>=6.f && requestedHour<=19.f)
                result.runtime.daylightHour=requestedHour;
            for(const auto& item:file.data.value("lights").toArray()){
                const auto j=item.toObject();Light l;
                l.p=v3(j.value("position"));l.radius=f(j.value("radius"),8.f);
                l.intensity=f(j.value("intensity"),1.f);
                l.color=v3(j.value("color"),{255,220,190});
                render.lights.push_back(l);
            }
        }
    }
    if(materialDocs.isEmpty()||meshes.isEmpty()||instanceDocs.isEmpty()){
        result.error="Missing meshes, instances or materials";return result;
    }
    if(materialDocs.size()>256){result.error="Material count exceeds 256";return result;}
    report(15,QStringLiteral("Préparation des textures PBR"));
    auto stageStarted=std::chrono::steady_clock::now();
    const auto stageElapsedMs=[&](){
        return std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-stageStarted).count();
    };
    int completedMaterials=0;
    for(const auto& j:materialDocs){
        const QString id=j.value("id").toString();
        if(id.isEmpty()||result.runtime.materialIndex.contains(id)){
            result.error="Duplicate or missing material: "+id;return result;
        }
        const auto base=v3(j.value("baseColor"),{.7f,.7f,.7f});
        const float rough=f(j.value("roughness"),.72f);
        const float metal=f(j.value("metallic"),0.f);
        const float transmission=f(j.value("transmission"),0.f);
        if(!finite3(base)||rough<0||rough>1||metal<0||metal>1||transmission<0||transmission>1){
            result.error="Invalid PBR properties: "+id;return result;
        }
        Material material;material.name=id.toStdString();
        material.reflectivityPercent=uint8_t(std::lround(clampf(.04f+metal*.80f,0,1)*100));
        material.transmissionPercent=uint8_t(std::lround(transmission*100));
        material.refractiveIndex=f(j.value("ior"),1.5f);
        material.metallic=metal;
        material.roughnessScale=clampf(f(j.value("roughnessScale"),1.f),.01f,2.f);
        const auto texId=j.value("textureSet").toString();
        if(!sets.contains(texId)){result.error="Missing texture set: "+texId;return result;}
        const auto set=sets.value(texId);
        const int requested=int(set.value("resolution").toInt(1024));
        int size=std::min(maxTextureResolution,requested);
        if(size<64||size>2048||(size&(size-1))){result.error="Invalid mip resolution";return result;}
        generatePbr(material,base,rough,set.value("pattern").toString("plaster"),size);
        // Restrict optional painting source to the loaded scene's own root.
        const QString imageRel=set.value("imageFile").toString();
        if(!imageRel.isEmpty()){
            const QDir sceneDir(source.rootPath);
            const QString imagePath=QDir::cleanPath(sceneDir.absoluteFilePath(imageRel));
            const QString rootCanonical=QFileInfo(source.rootPath).canonicalFilePath();
            const QString imageCanonical=QFileInfo(imagePath).canonicalFilePath();
            const QString relative=sceneDir.relativeFilePath(imagePath);
            if(source.rootPath.isEmpty()||QDir::isAbsolutePath(imageRel)||
               relative==".."||relative.startsWith("../")||
               rootCanonical.isEmpty()||imageCanonical.isEmpty()||
               !(imageCanonical.startsWith(rootCanonical+"/")||
                 imageCanonical.startsWith(rootCanonical+"\\"))
               ){
                result.error="Image texture escapes/missing within scene root: "+imageRel;
                return result;
            }
            if(!replaceAlbedoWithImage(material,imageCanonical,result.error))return result;
        }
        result.runtime.textureResolution=std::max(result.runtime.textureResolution,size);
        result.runtime.materialIndex.insert(id,int(render.mat.size()));
        render.mat.push_back(std::move(material));
        ++completedMaterials;
        report(15+int(65.0*completedMaterials/materialDocs.size()),
            QStringLiteral("Textures PBR : %1 / %2 matériaux")
                .arg(completedMaterials).arg(materialDocs.size()));
    }
    result.runtime.pbrBuildMs=stageElapsedMs();
    stageStarted=std::chrono::steady_clock::now();
    QSet<QString> instanceIds;
    int completedInstances=0;
    for(const auto& j:instanceDocs){
        ObjectInfo object;
        object.id=j.value("id").toString();object.parent=j.value("parent").toString();
        object.material=j.value("material").toString();
        const QString meshId=j.value("geometry").toString();
        if(object.id.isEmpty()||instanceIds.contains(object.id)||!meshes.contains(meshId)
            ||!result.runtime.materialIndex.contains(object.material)){
            result.error="Invalid or duplicate object: "+object.id;return result;
        }
        instanceIds.insert(object.id);
        const auto size=v3(j.value("size"),{1,1,1});
        const auto pos=v3(j.value("position"));
        if(!finite3(size)||!finite3(pos)||size.x<=0||size.y<=0||size.z<=0){
            result.error="Invalid transform: "+object.id;return result;
        }
        const int material=result.runtime.materialIndex.value(object.material);
        object.firstTriangle=int(render.obj.size());
        const auto type=meshes.value(meshId).value("type").toString();
        if(type=="box"){
            const float angle=f(j.value("rotationY"));
            box(render,pos,size,angle,material);
            // Preserve the box source as lightweight metadata while keeping
            // all 12 triangles in render.obj for CPU, Compute and BVH.
            // Rotated or transparent geometry remains on the legacy path.
            if(std::abs(angle)<1.e-5f && render.obj.size()>=size_t(object.firstTriangle+12)){
                SourceBox src;
                src.firstTriangle=uint32_t(object.firstTriangle);
                src.triangleCount=uint32_t(render.obj.size()-size_t(object.firstTriangle));
                src.mn=pos-size*.5f;
                src.mx=pos+size*.5f;
                src.material=uint32_t(material);
                if(src.triangleCount==12)render.sourceBoxes.push_back(src);
            }
        }
        else if(type=="uv_sphere")sphere(render,pos,size,material);
        else if(type=="foliage_canopy"){
            foliageCanopy(render,pos,size,material);
            SourceFoliage source;
            source.firstTriangle=uint32_t(object.firstTriangle);
            source.triangleCount=uint32_t(render.obj.size()-size_t(object.firstTriangle));
            source.center=pos;
            source.radii=size*.5f;
            source.material=uint32_t(material);
            if(source.triangleCount>0)render.sourceFoliage.push_back(source);
        }
        else {result.error="Unsupported V16 geometry: "+meshId;return result;}
        object.triangleCount=int(render.obj.size())-object.firstTriangle;
        result.runtime.objects.push_back(object);
        ++completedInstances;
        // Every progress tick corresponds to an actual generated instance.
        if(completedInstances==instanceDocs.size() || completedInstances%16==0)
            report(80+int(15.0*completedInstances/instanceDocs.size()),
                QStringLiteral("Géométrie : %1 / %2 objets")
                    .arg(completedInstances).arg(instanceDocs.size()));
    }
    if(render.obj.empty()){result.error="No triangles generated";return result;}
    result.runtime.geometryBuildMs=stageElapsedMs();
    stageStarted=std::chrono::steady_clock::now();
    render.order.resize(render.obj.size());
    std::iota(render.order.begin(),render.order.end(),0u);
    report(96,QStringLiteral("Construction de l'accélérateur BVH"));
    render.root=buildBvh(render,0,int(render.order.size()));
    result.runtime.bvhBuildMs=stageElapsedMs();
    report(100,QStringLiteral("Scène prête"));
    return result;
}
Camera makeCamera(const CameraPreset& preset,int width,int height) {
    Camera c;
    c.eye=preset.eye;c.f=norm(preset.target-preset.eye);
    c.r=norm(cross(c.f,{0,1,0}));
    c.u=norm(cross(c.r,c.f));
    c.tanHalf=std::tan(clampf(preset.fov,20.f,100.f)*PI/360.f);
    c.aspect=float(width)/std::max(height,1);
    c.pixelAngle=2*c.tanHalf/std::max(width,1);
    return c;
}
}
