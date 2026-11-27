#include "src/m3d_zreflect_geometry.h"
#include <iostream>
using namespace m3d;using namespace m3d::zreflect;
static void tri(Scene& s,V a,V b,V c){Obj o;o.kind=Kind::Triangle;o.a=a;o.b=b;o.c=c;s.obj.push_back(o);}
static void quad(Scene& s,V a,V b,V c,V d){tri(s,a,b,c);tri(s,a,c,d);}
int main(){
    Policy p;
    if(!opticalEligible(false,false,.65,0,.15,Permission::Auto,p))return 1;
    if(opticalEligible(false,false,0,.82,.28,Permission::Auto,p))return 2;
    if(opticalEligible(true,false,.99,0,.04,Permission::Disabled,p))return 3;
    if(!opticalEligible(false,false,0,0,.8,Permission::Enabled,p))return 4;
    Scene s;quad(s,{-1,-1,3},{1,-1,3},{1,1,3},{-1,1,3});
    auto patches=extractSlab(s,0,2,false,false,p);if(patches.size()!=1||patches[0].side!=0)return 5;
    Camera c;c.eye={0,0,0};c.f={0,0,1};c.r={1,0,0};c.u={0,1,0};c.tanHalf=1;c.aspect=1;
    if(std::abs(projectedPatch(s,patches[0],c)-1./9.)>1.e-5)return 6;
    Scene bent;quad(bent,{-1,-1,3},{1,-1,3},{1,1,3},{-1,1,3});
    quad(bent,{1,-1,3},{1,-1,5},{1,1,5},{1,1,3});
    if(!extractSlab(bent,0,4,false,false,p).empty())return 7;
    Scene small;tri(small,{0,0,3},{.1f,0,3},{0,.1f,3});
    if(!extractSlab(small,0,1,false,false,p).empty())return 8;
    if(extractSlab(small,0,1,false,true,p).size()!=1)return 9;
    if(p.futureCameraLimit!=3)return 10;
    // The two adjacent quantization bins must still share one coplanar patch.
    Scene boundary;tri(boundary,{0,0,3.00049f},{1,0,3.00049f},{0,1,3.00049f});
    tri(boundary,{1,0,3.00051f},{1,1,3.00051f},{0,1,3.00051f});
    std::map<PlaneKey,std::vector<Patch>> buckets;
    addTriangle(buckets,boundary.obj[0],0);addTriangle(buckets,boundary.obj[1],1);
    if(buckets.size()!=1||buckets.begin()->second[0].triangles.size()!=2)return 11;
    tri(boundary,{0,0,3.005f},{1,0,3.005f},{0,1,3.005f});
    addTriangle(buckets,boundary.obj[2],2);if(buckets.size()!=2)return 12;
    std::cout<<"Z-Reflect restrictive policy and geometry OK\n";return 0;
}
