#include "src/m3d_zreflect_index.h"
#include <chrono>
#include <iostream>
#include <vector>
using namespace m3d;using namespace m3d::zreflect;
static IndexedPlane plane(uint32_t id,V lo,V hi){Bounds b;b.add(lo);b.add(hi);return {id,b};}
static Camera camera(V eye,V forward,V right,float tanHalf=0.25f){
    Camera c;c.eye=eye;c.f=forward;c.r=right;c.u={0,1,0};c.tanHalf=tanHalf;c.aspect=1;return c;
}
static std::vector<uint32_t> exhaustive(const std::vector<IndexedPlane>& p,const Camera& c){
    std::vector<uint32_t> v;for(const auto& x:p)if(potentiallyVisible(x.bounds,c))v.push_back(x.id);
    std::sort(v.begin(),v.end());return v;
}
int main(){
    std::vector<IndexedPlane> p;p.reserve(60000);
    p.push_back(plane(0,{-1,-1,10},{0,1,10}));p.push_back(plane(1,{0,-1,10},{1,1,10}));
    for(uint32_t i=2;i<60000;++i){
        const float x=100.f+float(i%300)*3.f,z=30.f+float(i/300)*2.f;
        p.push_back(plane(i,{x,-1,-z},{x+1,1,-z}));
    }
    SpatialIndex index;index.build(p);std::vector<uint32_t> out;
    auto c=camera({0,0,0},{0,0,1},{1,0,0});
    const auto stats=index.query(c,out);
    if(out!=std::vector<uint32_t>{0,1}||out!=exhaustive(p,c)||stats.planeTests>=128||stats.nodeVisits>=128)return 1;
    const auto t0=std::chrono::steady_clock::now();
    for(int i=0;i<1000;++i){auto s=index.query(c,out);if(s.candidates!=2)return 2;}
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-t0).count()/1000.;
    for(int i=0;i<64;++i){float a=float(i)*6.28318530718f/64.f;auto v=camera({float(i-32),0,0},{std::sin(a),0,std::cos(a)},{std::cos(a),0,-std::sin(a)},.8f);
        index.query(v,out);if(out!=exhaustive(p,v))return 3;
    }
    // No artificial cap, even if all descriptors overlap and are visible.
    std::vector<IndexedPlane> overlap;
    for(uint32_t i=0;i<64;++i)overlap.push_back(plane(i,{-1,-1,10},{1,1,10}));
    index.build(overlap);index.query(c,out);if(out.size()!=64)return 4;
    Bounds invalid;overlap.push_back({64,invalid});index.build(overlap);index.query(c,out);if(out.size()!=65)return 5;
    index.build({});index.query(c,out);if(!out.empty())return 6;
    std::cout<<"ZReflect stress: logical=60000 active=2 nodeVisits="<<stats.nodeVisits<<" planeTests="<<stats.planeTests<<" meanQueryUs="<<us<<" (synthetic sparse case)\n";
    return 0;
}
