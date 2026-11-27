#include "../escargot/debug.h"
#include <set>
namespace e=m3d::escargot;
int main(){
    for(int w=1;w<17;++w)for(int h=1;h<17;++h){auto order=e::spiral(w,h);std::set<size_t> ids(order.begin(),order.end());if(order.size()!=size_t(w*h)||ids.size()!=order.size()||*ids.rbegin()>=size_t(w*h))return 1;}
    e::Surface sf;sf.candidate.bounds={0,0,5,5};sf.cols=2;sf.rows=2;sf.step=3;
    sf.samples.resize(4);e::LayerKey key{1,0,0};
    for(size_t i=0;i<4;++i){auto& s=sf.samples[i];s.x=int(i%2)*4;s.y=int(i/2)*4;s.receiver=e::State::Measured;s.complete=true;s.receiverDepth=2;s.receiverWeight=1;
        e::Value v;v.object=1;v.state=e::State::Measured;v.depth=3;v.opacity=1;v.visibility=1;v.uv={float(i)*.1f,0,0};v.normal={0,1,0};s.values.emplace(key,v);}
    e::makeLayers(sf);const auto& l=sf.layers.at(key);auto mid=e::reconstruct(sf,l,2,2);
    if(mid.state!=e::State::Reconstructed||std::abs(mid.uv.x-.15f)>1e-5f||mid.visibility!=1)return 2;
    sf.samples[0].values.clear();sf.layers.clear();e::makeLayers(sf);
    auto zero=e::reconstruct(sf,sf.layers.at(key),0,0);if(zero.state!=e::State::Zero||zero.object)return 3;
    mid=e::reconstruct(sf,sf.layers.at(key),2,2);if(mid.visibility!=.75f||std::abs(mid.uv.x-.2f)>1e-5f)return 4;
    // Incomplete zero is unknown rather than an observed optical absence.
    sf.samples[0].complete=false;
    mid=e::reconstruct(sf,sf.layers.at(key),2,2);if(mid.visibility!=1)return 5;
    std::vector<e::Candidate> c(9);for(auto& v:c)v.priority=1;e::distribute(c,13);uint32_t sum=0;for(auto v:c)sum+=v.budget;if(sum!=13)return 6;
    m3d::Scene scene;scene.mat.resize(1);m3d::Obj obj;obj.kind=m3d::Kind::Triangle;obj.a={-1,-1,2};obj.b={1,-1,2};obj.c={0,1,2};obj.n0=obj.n1=obj.n2={0,0,-1};scene.obj.push_back(obj);scene.order={0};
    m3d::LumaTraceBvh4 b;b.primitiveCount=1;m3d::LumaTraceBvh4Node n{};n.minX=-1;n.minY=-1;n.minZ=2;n.maxX=1;n.maxY=1;n.maxZ=2;n.primitiveCount=1;b.nodes.push_back(n);
    std::vector<uint32_t> owners{1};e::Traversal trace(scene,b,owners);e::HitSample hit;
    if(!trace.closest({{0,0,0},{0,0,1}},.001f,hit)||hit.object!=1||std::abs(hit.depth-2)>1e-5f)return 7;
    if(trace.closest({{0,0,0},{0,0,1}},2.001f,hit))return 8;
    if(trace.closest({{4,0,0},{0,0,1}},.001f,hit))return 9;
    m3d::V r;if(!e::refract({0,-1,0},{0,1,0},1.f/1.5f,r)||std::abs(r.y+1)>1e-5f)return 10;
    if(e::refract(m3d::norm({.9f,-.1f,0}),{0,1,0},1.5f,r))return 11;
    // Two opaque layers, nearer wins regardless of source ID.
    sf.samples[0].complete=true;sf.samples[0].values.emplace(key,e::Value{e::State::Measured,1,0,0,3,1,1,1,{0,0,0},{0,1,0}});
    e::LayerKey second{2,0,1};scene.mat.resize(2);scene.mat[0].colorMip.push_back({1,{0x00ff0000}});scene.mat[1].colorMip.push_back({1,{0x0000ff00}});
    for(auto& s:sf.samples){e::Value v;v.state=e::State::Measured;v.object=2;v.material=1;v.depth=1;v.visibility=v.opacity=v.weight=1;v.normal={0,1,0};s.values.emplace(second,v);}
    sf.layers.clear();e::makeLayers(sf);e::Result stats;e::compose(sf,scene,stats);
    if(sf.image[12].rgb.y<=0||sf.image[12].rgb.x>1e-5f||sf.image[12].alpha!=1)return 12;
    return 0;
}
