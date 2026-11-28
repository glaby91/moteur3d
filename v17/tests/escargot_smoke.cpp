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
    // Cloned traversal owns independent scratch but uses identical verified data.
    b.nodes[0].primitiveCount=1;m3d::Scene stableScene=scene;
    e::Traversal original(stableScene,b,owners),cloned(original);
    if(!original.valid()||!cloned.valid())return 33;
    for(int j=-20;j<=20;++j){m3d::Ray ray{{float(j)*.05f,0,0},{0,0,1}};e::HitSample a,bh;
        if(original.closest(ray,.001f,a)!=cloned.closest(ray,.001f,bh)||a.object!=bh.object||a.primitive!=bh.primitive||a.depth!=bh.depth)return 34;}
    // Exact cache key: camera, optics, dimensions and scene revision.
    m3d::Camera keyCam{};keyCam.eye={1,2,3};keyCam.f={0,0,1};keyCam.r={1,0,0};keyCam.u={0,1,0};keyCam.tanHalf=.6f;
    e::Options keyOpt;auto keyA=e::packetKey(stableScene,keyCam,1280,720,keyOpt);
    if(!(keyA==e::packetKey(stableScene,keyCam,1280,720,keyOpt)))return 35;
    auto shifted=keyCam;shifted.eye.x+=.025f;
    if(keyA==e::packetKey(stableScene,shifted,1280,720,keyOpt))return 36;
    if(keyA==e::packetKey(stableScene,keyCam,1279,720,keyOpt))return 37;
    auto changedOpt=keyOpt;++changedOpt.rayBudget;
    if(keyA==e::packetKey(stableScene,keyCam,1280,720,changedOpt))return 38;
    stableScene.markGpuGeometryChanged();if(keyA==e::packetKey(stableScene,keyCam,1280,720,keyOpt))return 39;
    // Two distinct transparent interfaces only one representable float apart.
    // Skipping the previous triangle must NOT erase the next interface.
    m3d::Scene thin;thin.mat.resize(1);
    auto t0=obj,t1=obj;
    const float adjacent=std::nextafter(2.f,std::numeric_limits<float>::infinity());
    t1.a.z=t1.b.z=t1.c.z=adjacent;
    thin.obj={t0,t1};thin.order={1,0};
    m3d::LumaTraceBvh4 tree;tree.primitiveCount=2;auto leaf=n;leaf.maxZ=adjacent;leaf.primitiveCount=2;tree.nodes.push_back(leaf);
    std::vector<uint32_t> thinOwners{1,2};
    e::Traversal walk(thin,tree,thinOwners);m3d::Ray testRay{{0,0,0},{0,0,1}};e::HitSample first,next;
    if(!walk.valid()||!walk.closest(testRay,.001f,first)||first.primitive!=0||first.depth!=2.f)return 40;
    if(!walk.closest(testRay,first.depth,next,first.primitive)||next.primitive!=1||next.depth!=adjacent)return 41;
    if(walk.closest(testRay,next.depth,first,next.primitive))return 42;
    // Exact depth tie remains lowest original ID even if leaf order is reversed.
    thin.obj[1]=t0;e::Traversal tied(thin,tree,thinOwners);
    if(!tied.closest(testRay,.001f,first)||first.primitive!=0)return 43;
    m3d::EscargotRayQuery defaultQuery{};if(defaultQuery.previousPrimitive!=0xffffffffu)return 44;
    // Real CPU traversal through a transparent interface, then reflection
    // through that interface onto an opaque source behind the camera.
    m3d::Scene waveScene;waveScene.mat.resize(2);
    waveScene.mat[0].transmissionPercent=100;waveScene.mat[1].transmissionPercent=0;
    auto glass=obj,receiver=obj,source=obj;
    glass.a.z=glass.b.z=glass.c.z=1.f;glass.mat=0;
    receiver.mat=1;source.a.z=source.b.z=source.c.z=-1.f;source.mat=1;
    waveScene.obj={glass,receiver,source};waveScene.order={2,1,0};
    m3d::LumaTraceBvh4 waveTree;waveTree.primitiveCount=3;auto waveLeaf=n;
    waveLeaf.minZ=-1.f;waveLeaf.maxZ=2.f;waveLeaf.primitiveCount=3;waveTree.nodes.push_back(waveLeaf);
    std::vector<uint32_t> waveOwners{1,2,3};e::Traversal waveWalk(waveScene,waveTree,waveOwners);
    if(!waveWalk.valid())return 45;
    m3d::Camera waveCam{};waveCam.eye={0,0,0};waveCam.f={0,0,1};waveCam.r={1,0,0};waveCam.u={0,1,0};waveCam.tanHalf=.5f;waveCam.aspect=1;
    auto runWave=[&](uint32_t budget,e::Result& result,e::WaveStats& ws){
        e::Options opts;opts.rayBudget=budget;opts.captureQueries=true;
        result.surfaces.resize(1);auto& surface=result.surfaces[0];surface.candidate.object=2;surface.candidate.material=1;surface.candidate.bounds={0,0,1,1};surface.candidate.budget=budget;
        auto traceBatch=[&](const std::vector<m3d::EscargotRayQuery>& q,std::vector<e::HitSample>& hits,std::string&){
            hits.resize(q.size());for(size_t i=0;i<q.size();++i)waveWalk.closest({{q[i].origin[0],q[i].origin[1],q[i].origin[2]},{q[i].direction[0],q[i].direction[1],q[i].direction[2]}},q[i].low,hits[i],q[i].previousPrimitive);
            return true;
        };
        return e::sampleWavefront(result.surfaces,waveScene,waveCam,1,1,opts,result,ws,traceBatch);
    };
    e::Result fullWave;e::WaveStats fullStats;if(!runWave(8,fullWave,fullStats))return 46;
    const auto& waveSample=fullWave.surfaces[0].samples[0];
    if(fullWave.rays!=4||fullStats.batches!=4||fullWave.completePaths!=1||waveSample.receiver!=e::State::Measured||waveSample.values.size()!=2||waveSample.values.count({1,0,0})!=1||waveSample.values.count({3,0,1})!=1)return 47;
    const auto& records=fullWave.surfaces[0].recorded;
    if(records.size()!=4||records[0].primitive!=0||records[1].primitive!=1||records[2].primitive!=0||records[3].primitive!=2||records[1].query.previousPrimitive!=0||records[2].query.previousPrimitive!=0xffffffffu||records[3].query.previousPrimitive!=0)return 48;
    e::Result limited;e::WaveStats limitedStats;if(!runWave(2,limited,limitedStats)||limited.rays!=2||limited.completePaths!=0||limited.unknownPaths!=1||limited.surfaces[0].samples[0].receiver!=e::State::Measured)return 49;
    m3d::EscargotRayQuery q{{0,0,0},.001f,{0,0,1},0xffffffffu};
    m3d::EscargotRayHit raw{};raw.primitive=0;raw.status=1;raw.depth=1;raw.u=.25f;raw.v=.5f;
    e::HitSample decoded;
    if(!e::decodeWaveHit(waveScene,waveOwners,q,raw,decoded)||decoded.object!=1||decoded.depth!=1||decoded.p.z!=1)return 50;
    q.previousPrimitive=0;if(e::decodeWaveHit(waveScene,waveOwners,q,raw,decoded))return 51;
    raw.status=2;if(e::decodeWaveHit(waveScene,waveOwners,q,raw,decoded))return 52;
    // Synthetic transcript of the known four-hit reflection, one final
    // readback. Reconstruct without tracing new rays; reject corrupted query.
    auto runResident=[&](bool corrupt,e::Result& result,e::ResidentStats& rs){
        e::Options opts;opts.rayBudget=8;opts.captureQueries=true;
        result.surfaces.resize(1);auto& surface=result.surfaces[0];surface.candidate.object=2;surface.candidate.material=1;surface.candidate.bounds={0,0,1,1};surface.candidate.budget=8;
        unsigned calls=0;
        auto transcript=[&](const std::vector<m3d::EscargotPathQuery>& paths,std::vector<m3d::EscargotPathEvent>& events,m3d::EscargotPathTiming&,std::string&){
            ++calls;if(paths.size()!=1||paths[0].offset!=0||paths[0].quota!=8||!e::samePathQuery(paths[0].initial,records[0].query))return false;
            events.resize(8);
            for(size_t i=0;i<records.size();++i){auto& ev=events[i];ev.query=records[i].query;ev.hit.primitive=records[i].primitive;ev.hit.status=1;ev.hit.depth=records[i].depth;ev.hit.u=.25f;ev.hit.v=.5f;}
            events[0].hit.pad[0]=4;events[0].hit.pad[1]=0;
            if(corrupt)events[2].query.low+=.01f;
            return true;
        };
        const bool ok=e::sampleResidentPaths(result.surfaces,waveScene,waveOwners,waveCam,1,1,opts,result,rs,transcript);
        return calls==1&&ok;
    };
    e::Result residentResult;e::ResidentStats residentStats;
    if(!runResident(false,residentResult,residentStats)||residentResult.rays!=4||residentResult.completePaths!=1||residentStats.paths!=1||residentStats.eventBytes!=8*sizeof(m3d::EscargotPathEvent))return 53;
    if(residentResult.surfaces[0].samples[0].values.size()!=2)return 54;
    e::Result rejected;e::ResidentStats rejectedStats;
    if(runResident(true,rejected,rejectedStats)||rejected.error.empty())return 55;
    e::makeLayers(fullWave.surfaces[0]);
    auto packet32=e::packGpu(fullWave,waveScene,3,32,false);
    auto plan32=e::planGpuBuild(fullWave,waveScene,3,32,false);
    auto plan16=e::planGpuBuild(fullWave,waveScene,3,16,true);
    if(!packet32.ok||!plan32.ok||!plan16.ok||plan32.native.jobCount!=1||plan16.native.jobCount!=1)return 56;
    std::string packetWhy;
    if(!e::logicalPacketEqual(packet32.data,packet32.data,packetWhy))return 57;
    auto relocated=packet32.data;
    const size_t descRecord=2+2*2+1,oldTile=size_t(relocated[4*descRecord]),newTile=relocated.size()/4;
    const std::array<float,4> tileCopy{relocated[4*oldTile],relocated[4*oldTile+1],relocated[4*oldTile+2],relocated[4*oldTile+3]};
    relocated.insert(relocated.end(),tileCopy.begin(),tileCopy.end());relocated[4*descRecord]=float(newTile);
    if(!e::logicalPacketEqual(packet32.data,relocated,packetWhy))return 58;
    relocated[4*newTile+3]+=1;
    if(e::logicalPacketEqual(packet32.data,relocated,packetWhy))return 59;
    return 0;
}
