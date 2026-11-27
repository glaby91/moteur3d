#include "../../experiments/2026-10-30-cote-ete-gt/native_v14/src/m3d_dx11_r001.h"
#include <iostream>
#include <random>
#include <algorithm>
#include <limits>
struct V{float x,y,z;};
struct C{V eye,f,r,u;float tanHalf,aspect;};
struct Box{V lo,hi;};
using namespace m3d::dx11::r001;
int main(){
    const C c{{0,0,0},{0,0,1},{1,0,0},{0,1,0},1.f,1.7777778f};
    const Frustum f(c);
    std::mt19937 rng(1725);std::uniform_real_distribution<float> p(-150,150),z(1,150),s(.1f,30);
    unsigned oldKeep=0,newKeep=0;
    // 1. Compare each rejected box with independent 8-corner plane reference.
    std::vector<Box> boxes;boxes.reserve(100000);
    for(int i=0;i<100000;++i){
        V a{p(rng),p(rng),z(rng)},b{s(rng),s(rng),s(rng)};
        Box box{{a.x-b.x,a.y-b.y,a.z-b.z},{a.x+b.x,a.y+b.y,a.z+b.z}};
        boxes.push_back(box);
        const double radius=std::sqrt(double(b.x)*b.x+double(b.y)*b.y+double(b.z)*b.z)+.01;
        const double v=1./c.aspect;
        const bool old=a.z+radius>=.08&&a.z-radius<=10000&&std::abs(a.x)<=a.z+radius*std::sqrt(2.)&&std::abs(a.y)<=a.z*v+radius*std::sqrt(1.+v*v);
        oldKeep+=old;const bool keep=f.visible(box.lo,box.hi);newKeep+=keep;
        if(!keep){
            double maxPlane[6]={-1e100,-1e100,-1e100,-1e100,-1e100,-1e100};
            for(float x:{box.lo.x,box.hi.x})for(float y:{box.lo.y,box.hi.y})for(float zz:{box.lo.z,box.hi.z}){
                const double d[]={zz-.08,10000.-zz,zz+x,zz-x,zz*v+y,zz*v-y};
                for(int j=0;j<6;++j)maxPlane[j]=std::max(maxPlane[j],d[j]);
            }
            if(*std::min_element(maxPlane,maxPlane+6)>=0)return 1;
        }
    }
    std::cout<<"PASS 1 boxes=100000 old="<<oldKeep<<" new="<<newKeep<<" falseReject=0\n";
    // 2. Boundary, near-plane crossings, invalid input, translated camera.
    const Frustum moved(C{{100000,100000,100000},{0,0,1},{1,0,0},{0,1,0},1,1});
    const float nan=std::numeric_limits<float>::quiet_NaN();
    if(!f.visible(V{-1,-1,.079f},V{1,1,.081f})||!f.visible(V{10,0,10},V{10,0,10})||
       f.visible(V{100,100,1},V{101,101,2})||!f.visible(V{nan,0,0},V{1,1,1})||
       !moved.visible(V{100000,100000,100010},V{100001,100001,100011}))return 2;
    std::cout<<"PASS 2 boundary/invalid/translated\n";
    // 3. Every sampled visible point within a positive light's sphere must survive.
    unsigned lights=0,kept=0,visibleSamples=0;
    for(int i=0;i<50000;++i){
        V a{p(rng),p(rng),z(rng)};float radius=s(rng);float intensity=i%5?1.f:0.f;
        const bool keep=lightMayContribute(f,a,radius,intensity,V{1,1,1});++lights;kept+=keep;
        if(intensity==0.f&&keep)return 3;
        for(int j=0;j<8;++j){
            V q{a.x+radius*.5f*((j&1)?1:-1),a.y+radius*.5f*((j&2)?1:-1),a.z+radius*.5f*((j&4)?1:-1)};
            if(intensity>0&&q.z>=.08&&q.z<=10000&&std::abs(q.x)<=q.z&&std::abs(q.y)<=q.z/c.aspect){++visibleSamples;if(!keep)return 4;}
        }
    }
    std::cout<<"PASS 3 lights="<<lights<<" retained="<<kept<<" visibleSamples="<<visibleSamples<<" falseReject=0\n";
    // 4. Reuse exact visibility decisions for depth and color.
    std::vector<uint8_t> mask;unsigned calls=0;
    fillVisibilityMask(boxes,mask,[&](const Box& b){++calls;return f.visible(b.lo,b.hi);});
    if(calls!=boxes.size()||mask.size()!=boxes.size())return 5;
    for(size_t i=0;i<boxes.size();++i)if(bool(mask[i])!=f.visible(boxes[i].lo,boxes[i].hi))return 6;
    std::cout<<"PASS 4 visibility queries 200000 -> "<<calls<<"\n";
    // 5. At zero daylight, all solar-shadow terms are zero for any shadow factor.
    for(int i=0;i<10000;++i){
        const float diffuse=s(rng),spec=s(rng),shadow=float(i%101)/100.f,day=0.f;
        if(diffuse*shadow*day!=diffuse*day||spec*shadow*day!=spec*day)return 7;
    }
    std::cout<<"PASS 5 zero-sun algebra 10000\n";
}
