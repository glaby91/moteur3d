#pragma once
#include "src/m3d_v16_bridge.h"
#include <map>
#include <tuple>
#include <utility>
namespace m3d::escargot {
enum class Mode { Reflection, Refraction };
enum class State : uint8_t { Unknown, Zero, Measured, Reconstructed };
struct Rect {
    int x=0,y=0,w=0,h=0;
    bool valid() const {return w>0&&h>0;}
    bool contains(int a,int b)const{return a>=x&&b>=y&&a<x+w&&b<y+h;}
};
struct Options {
    uint32_t rayBudget=4096; // actual BVH queries, including primary and continuation
    uint32_t maxCrossings=8; // safety bound per optical path, not surface count
    int maxDimension=2048;
    uint64_t maxOutputCells=4ull*1024*1024;
    Mode mode=Mode::Reflection;
};
struct HitSample { uint32_t primitive=0, object=0; float depth=INF; V p{},n{},uv{},geometricNormal{}; };
struct Value {
    State state=State::Unknown;
    uint32_t object=0,primitive=0,material=0;
    float depth=0,visibility=0,opacity=0,weight=0;
    V uv{},normal{};
};
struct LayerKey {
    uint32_t object=0, crossing=0,material=0;
    bool operator<(const LayerKey& k)const{return std::tie(object,crossing,material)<std::tie(k.object,k.crossing,k.material);}
};
struct Sample {
    int x=0,y=0;
    State receiver=State::Unknown;
    V position{},normal{},direction{};
    float receiverDepth=0,receiverWeight=0;
    bool complete=false, directionValid=false;
    float primaryTransmission=1.f;
    std::map<LayerKey,Value> values;
};
struct Candidate {
    uint32_t object=0,material=0;
    Rect bounds;
    V lo{},hi{};
    double priority=0;
    uint32_t budget=0;
};
struct Layer { LayerKey key; Rect bounds; std::map<size_t,Value> measured; std::vector<size_t> partial; };
struct Pixel { V rgb{}; float alpha=0,weight=0,receiverDepth=0; };
struct Surface {
    Candidate candidate;
    int step=1,cols=0,rows=0;
    std::vector<Sample> samples;
    std::map<LayerKey,Layer> layers;
    std::vector<size_t> completeIndices,receiverIndices;
    bool sameKnownSet=false;
    std::vector<Pixel> image; // tightly cropped footprint, no full screen per object
    uint32_t queries=0,completePaths=0,unknownPaths=0;
    bool deferred=false,complete=false;
};
struct Result {
    std::vector<Surface> surfaces;
    uint32_t rays=0, candidates=0,deferred=0,paths=0,completePaths=0,unknownPaths=0;
    uint64_t measured=0,reconstructed=0,zero=0,outputCells=0;
    uint64_t completeSearches=0,receiverSearches=0,partialTests=0,layerVisits=0,textureSamples=0,opaqueSkipped=0,coverageCells=0,fallbackCells=0;
    double matrixMs=0,compositionMs=0;
    bool ok=false;
    std::string error;
};
inline bool finite(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline V mul(V a,V b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
inline float sat(float x){return clampf(x,0.f,1.f);}
inline V reflect(V d,V n){return norm(d-n*(2.f*dot(d,n)));}
inline bool refract(V d,V n,float eta,V& out){
    const float c=clampf(-dot(d,n),0.f,1.f),k=1.f-eta*eta*(1.f-c*c);
    if(k<0.f)return false;
    out=norm(d*eta+n*(eta*c-std::sqrt(k)));return finite(out);
}
inline bool waterMaterial(const Material& m){return m.surfaceAuthored?m.waterSurface:(m.waterSurface||m.name=="cse_pool_water");}
inline float transmission(const Material& m){return waterMaterial(m)?1.f:sat(float(m.transmissionPercent)/100.f);}
inline bool validCamera(const Camera& c){
    return finite(c.eye)&&finite(c.f)&&finite(c.r)&&finite(c.u)&&
        std::isfinite(c.tanHalf)&&std::isfinite(c.aspect)&&c.tanHalf>0&&c.aspect>0&&
        std::abs(length(c.f)-1.f)<.001f&&std::abs(length(c.r)-1.f)<.001f&&std::abs(length(c.u)-1.f)<.001f&&
        std::abs(dot(c.f,c.r))<.001f&&std::abs(dot(c.f,c.u))<.001f&&std::abs(dot(c.r,c.u))<.001f;
}
inline bool validOptions(const Options& o){return o.rayBudget>0&&o.rayBudget<=1000000&&o.maxCrossings>0&&o.maxCrossings<=32&&o.maxDimension>0&&o.maxDimension<=4096&&o.maxOutputCells>0&&o.maxOutputCells<=16777216;}
}
