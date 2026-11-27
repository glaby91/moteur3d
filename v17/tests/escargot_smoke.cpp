#include "../escargot/debug.h"
#include "../escargot/gpu_packet.h"
#include <set>
namespace e=m3d::escargot;
int main(){
    for(int w=1;w<17;++w)for(int h=1;h<17;++h){auto order=e::spiral(w,h);std::set<size_t> ids(order.begin(),order.end());if(order.size()!=size_t(w*h)||ids.size()!=order.size()||*ids.rbegin()>=size_t(w*h))return 1;}
    e::Surface sf;sf.candidate.bounds={0,0,8,8};sf.cols=2;sf.rows=2;sf.step=4;
    sf.samples.resize(4);e::LayerKey key{1,0,0};
    for(size_t i=0;i<4;++i){auto& s=sf.samples[i];s.x=2+int(i%2)*4;s.y=2+int(i/2)*4;s.receiver=e::State::Measured;s.complete=true;s.receiverDepth=2;s.receiverWeight=1;
        e::Value v;v.object=1;v.state=e::State::Measured;v.depth=3;v.opacity=1;v.visibility=1;v.uv={float(i)*.1f,0,0};v.normal={0,1,0};s.values.emplace(key,v);}
    e::makeLayers(sf);const auto& l=sf.layers.at(key);auto mid=e::reconstruct(sf,l,4,4);
    if(mid.state!=e::State::Reconstructed||std::abs(mid.uv.x-.15f)>1e-5f||mid.visibility!=1)return 2;
    sf.samples[0].values.clear();sf.layers.clear();e::makeLayers(sf);
    auto zero=e::reconstruct(sf,sf.layers.at(key),2,2);if(zero.state!=e::State::Zero||zero.object)return 3;
    mid=e::reconstruct(sf,sf.layers.at(key),4,4);if(mid.visibility!=.75f||std::abs(mid.uv.x-.2f)>1e-5f)return 4;
    // Incomplete zero is unknown rather than an observed optical absence.
    sf.samples[0].complete=false;e::makeLayers(sf);
    mid=e::reconstruct(sf,sf.layers.at(key),4,4);if(mid.visibility!=1)return 5;
    std::vector<e::Candidate> c(9);for(auto& v:c)v.priority=1;e::distribute(c,13);uint32_t sum=0;for(auto v:c)sum+=v.budget;if(sum!=13)return 6;
    m3d::Scene scene;scene.mat.resize(1);m3d::Obj obj;obj.kind=m3d::Kind::Triangle;obj.a={-1,-1,2};obj.b={1,-1,2};obj.c={0,1,2};obj.n0=obj.n1=obj.n2={0,0,-1};scene.obj.push_back(obj);scene.order={0};
    m3d::LumaTraceBvh4 b;b.primitiveCount=1;m3d::LumaTraceBvh4Node n{};n.minX=-1;n.minY=-1;n.minZ=2;n.maxX=1;n.maxY=1;n.maxZ=2;n.primitiveCount=1;b.nodes.push_back(n);
    std::vector<uint32_t> owners{1};e::Traversal trace(scene,b,owners);e::HitSample hit;
    if(!trace.valid()||!trace.closest({{0,0,0},{0,0,1}},.001f,hit)||hit.object!=1||std::abs(hit.depth-2)>1e-5f)return 7;
    if(trace.closest({{0,0,0},{0,0,1}},2.001f,hit))return 8;
    if(trace.closest({{4,0,0},{0,0,1}},.001f,hit))return 9;
    m3d::V r;if(!e::refract({0,-1,0},{0,1,0},1.f/1.5f,r)||std::abs(r.y+1)>1e-5f)return 10;
    if(e::refract(m3d::norm({.9f,-.1f,0}),{0,1,0},1.5f,r))return 11;
    // Two opaque layers, nearer wins regardless of source ID.
    sf.samples[0].complete=true;sf.samples[0].values.emplace(key,e::Value{e::State::Measured,1,0,0,3,1,1,1,{0,0,0},{0,1,0}});
    e::LayerKey second{2,0,1};scene.mat.resize(2);scene.mat[0].colorMip.push_back({1,{0x00ff0000}});scene.mat[1].colorMip.push_back({1,{0x0000ff00}});
    for(auto& s:sf.samples){e::Value v;v.state=e::State::Measured;v.object=2;v.material=1;v.depth=1;v.visibility=v.opacity=v.weight=1;v.normal={0,1,0};s.values.emplace(second,v);}
    sf.layers.clear();e::makeLayers(sf);e::Result stats;e::compose(sf,scene,stats);
    if(sf.image[36].rgb.y<=0||sf.image[36].rgb.x>1e-5f||sf.image[36].alpha!=1)return 12;
    // Complementary opaque source coverage must not produce an artificial alpha hole.
    for(size_t i=0;i<sf.samples.size();++i){
        auto& s=sf.samples[i];if(i<2)s.values.erase(second);else s.values.erase(key);
    }
    e::makeLayers(sf);e::compose(sf,scene,stats);
    if(std::abs(sf.image[36].alpha-1.f)>1e-6f||sf.image[36].rgb.x<=0||sf.image[36].rgb.y<=0)return 13;
    // Thin footprint: every lattice cell exactly once.
    auto line=e::spiral(4096,1);std::set<size_t> unique(line.begin(),line.end());
    if(line.size()!=4096||unique.size()!=4096)return 14;
    // Invalid BVH must be diagnosed, not interpreted as all zero observations.
    b.nodes[0].primitiveCount=2;e::Traversal malformed(scene,b,owners);if(malformed.valid())return 15;
    // The exact observed zero cannot be replaced by the four-neighbor average.
    if(e::reconstruct(sf,sf.layers.at(key),2,6).state!=e::State::Zero)return 16;
    // Compare run reuse with the same compositor forced to compute every cell.
    auto reference=sf; e::Result referenceStats;e::compose(reference,scene,referenceStats,false);
    e::Result optimizedStats;e::compose(sf,scene,optimizedStats,true);
    if(reference.image.size()!=sf.image.size())return 17;
    for(size_t i=0;i<sf.image.size();++i){
        const auto& a=sf.image[i];const auto& b=reference.image[i];
        if(a.rgb.x!=b.rgb.x||a.rgb.y!=b.rgb.y||a.rgb.z!=b.rgb.z||
           a.alpha!=b.alpha||a.weight!=b.weight||a.receiverDepth!=b.receiverDepth)return 18;
    }
    if(!optimizedStats.reusedCells||referenceStats.reusedCells)return 19;
    e::Neighbors na,nb;na.count=nb.count=1;na.ids[0]=nb.ids[0]=3;
    na.distance[0]=0;nb.distance[0]=1;if(e::sameStencil(na,nb))return 20;
    na.distance[0]=2;if(!e::sameStencil(na,nb))return 21;
    // A completed missing source is zero even outside its observed bounds.
    e::Layer remote;remote.bounds={7,7,1,1};
    if(e::reconstruct(sf,remote,2,2).state!=e::State::Zero)return 22;
    // Partial observation invalidates reuse (even if unrelated source).
    sf.samples[0].complete=false;e::makeLayers(sf);e::Result partialStats;e::compose(sf,scene,partialStats);
    if(partialStats.reusedCells)return 23;
    using P=m3d::zreflect::Permission;
    if(e::eligible(false,false,0,.0f,.1f,P::Auto,e::Mode::Reflection))return 24;
    if(!e::eligible(false,false,0,.9f,.15f,P::Auto,e::Mode::Reflection))return 25;
    if(e::eligible(false,false,0,.9f,.5f,P::Auto,e::Mode::Reflection))return 26;
    if(!e::eligible(true,false,1,0,.8f,P::Unspecified,e::Mode::Reflection))return 27;
    if(e::eligible(true,true,1,1,0,P::Disabled,e::Mode::Reflection))return 28;
    if(!e::eligible(false,false,0,0,.9f,P::Enabled,e::Mode::Reflection))return 29;
    // Exact sparse index versus existing exhaustive/ring reference.
    e::Surface grid;grid.candidate.bounds={0,0,32,32};grid.cols=grid.rows=32;grid.step=1;grid.samples.resize(1024);
    for(size_t i=0;i<grid.samples.size();++i){auto& s=grid.samples[i];s.x=int(i%32);s.y=int(i/32);s.complete=(i%19==0);}
    e::makeLayers(grid);e::NeighborIndex index(grid,grid.completeIndices);if(!index.active())return 30;
    for(int y=0;y<32;++y)for(int x=0;x<32;++x){
        auto a=e::nearestComplete(grid,x,y),b=index.query(x,y);
        if(!e::sameStencil(a,b)||a.distance!=b.distance)return 31;
    }
    // Conservative tile shortlist must contain the exact top four for every cell.
    e::Rect tile{4,7,13,17};auto shortIds=e::shortlist(grid,grid.completeIndices,tile);
    for(int y=tile.y;y<tile.y+tile.h;++y)for(int x=tile.x;x<tile.x+tile.w;++x){
        auto truth=e::nearestComplete(grid,x,y);e::Neighbors shortN;
        for(auto id:shortIds)e::insertNeighbor(shortN,grid,x,y,id);
        if(!e::sameStencil(truth,shortN)||truth.distance!=shortN.distance)return 32;
    }
    return 0;
}
