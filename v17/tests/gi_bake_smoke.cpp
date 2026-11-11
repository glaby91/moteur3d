// V17.312: deterministic offline GI rays, multi-hour table, cache integrity.
#include "src/m3d_gi_bake.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
using namespace m3d;
static void test(bool ok,const char* message){
    if(!ok){std::fprintf(stderr,"GI BAKE TEST FAILED: %s\n",message);std::exit(1);}
}
static void triangle(Scene& s,V a,V b,V c){
    Obj o{};
    o.kind=Kind::Triangle;o.mat=0;o.a=a;o.b=b;o.c=c;
    o.n0=o.n1=o.n2=norm(cross(b-a,c-a));
    o.mn={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};
    o.mx={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
    s.obj.push_back(o);
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    Scene scene;scene.finiteGround=true;
    Material mat{};mat.name="warmFloor";
    ColorLevel level{};level.size=1;level.p.push_back(0x00c09052u);
    mat.colorMip.push_back(level);scene.mat.push_back(mat);
    const V a{-2,0,-2},b{2,0,-2},c{2,0,2},d{-2,0,2};
    triangle(scene,a,c,b);triangle(scene,a,d,c);
    Node node{};node.mn={-3,-1,-3};node.mx={3,2,3};
    node.start=0;node.count=2;
    scene.node.push_back(node);scene.root=0;scene.order={0u,1u};
    gibake::Options opt;
    opt.nx=4;opt.ny=3;opt.nz=4;opt.rays=16;
    opt.frames=5;opt.firstHour=8.f;opt.stepHours=2.5f;
    gibake::Table first,second;std::string error;
    test(gibake::bake(scene,opt,first,error),"first bake unsuccessful");
    test(gibake::bake(scene,opt,second,error),"second bake unsuccessful");
    test(first.valid()&&first.rgba==second.rgba,
         "bake is not deterministic");
    test(first.sceneHash==gibake::signature(scene),"scene fingerprint wrong");
    const V probe=first.origin+first.span*.5f;
    const V morning=gibake::sample(first,probe,9.f);
    const V evening=gibake::sample(first,probe,17.5f);
    test(length(morning-evening)>0.00001f,"time-dependent colors unchanged");
    unsigned low=0,high=0;float weight=0;
    gibake::hourIndices(first,9.f,low,high,weight);
    test(low==0&&high==1&&std::abs(weight-.4f)<.0001f,
         "incorrect time interpolation");
    const std::filesystem::path file(argv[1]);
    test(gibake::writeFile(file,first,error),"file write failed");
    gibake::Table restored;
    test(gibake::readFile(file,restored,error),"file reload failed");
    test(restored.sceneHash==first.sceneHash&&
         restored.rgba==first.rgba,"round-trip mismatch");
    scene.mat[0].colorMip.front().p[0]=0x005080deu;
    test(gibake::signature(scene)!=restored.sceneHash,
         "stale GI cache not rejected after material edit");
    std::fstream corrupted(file,std::ios::binary|std::ios::in|std::ios::out);
    corrupted.seekg(-1,std::ios::end);char c=0;corrupted.get(c);
    corrupted.seekp(-1,std::ios::end);c^=char(0x33);
    corrupted.put(c);corrupted.close();
    gibake::Table invalid;
    test(!gibake::readFile(file,invalid,error),
         "corrupt GI cache checksum not rejected");
    std::filesystem::remove(file);
    std::printf("GI BAKE PASS: deterministic real ray transport, %u hour states, disk cache and checksum\n",
                first.frames);
    return 0;
}
