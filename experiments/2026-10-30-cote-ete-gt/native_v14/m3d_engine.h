#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "src/m3d_physics.h"
namespace m3d {
constexpr float PI=3.14159265358979323846f, INF=std::numeric_limits<float>::infinity();
struct V { float x=0,y=0,z=0; V operator+(V b)const{return{x+b.x,y+b.y,z+b.z};} V operator-(V b)const{return{x-b.x,y-b.y,z-b.z};} V operator*(float s)const{return{x*s,y*s,z*s};} V operator/(float s)const{return{x/s,y/s,z/s};} };
inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;} inline V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};} inline float length(V a){return std::sqrt(dot(a,a));} inline V norm(V a){float l=length(a);return l>1e-20f?a/l:V{0,1,0};} inline float clampf(float x,float a,float b){return std::max(a,std::min(b,x));}
enum class Kind:uint8_t{Triangle=0,Box=1,RoundBox=2,Capsule=3,Cone=4,Ellipse=5};
struct Obj { Kind kind{}; uint8_t mat=0; V mn,mx,a,b,c,n0,n1,n2,uv0,uv1,uv2,center,core; float r=0,r1=0,k=0,len=0,len2=0; };
struct Node { V mn,mx; int32_t left=-1,right=-1,start=0,count=0; };
struct Level { uint32_t size=0; std::vector<uint16_t> p; };
struct Material { std::string name; uint8_t reflectivityPercent=0; std::vector<Level> mip; };
struct Scene { std::vector<Obj> obj; std::vector<Node> node; std::vector<uint32_t> order; std::vector<Material> mat; int32_t root=-1; };
struct Ray { V o,d; };
struct Hit { float t=INF; V p,n; int mat=-1,id=-1; };
struct Camera { V eye,f,r,u; float tanHalf=0,aspect=1,pixelAngle=0,focusDistance=12,apertureRadius=0; };
struct Stats { uint64_t rays=0,tests=0,aabb=0,nodes=0,hits=0,sky=0,texels=0,depthReads=0,shadowRays=0; double ms=0,postMs=0,shadowMs=0; unsigned threads=1; };
struct Light { V p; float radius,intensity; V color; };
struct ShadowMap { int size=1024; float span=30,dist=42; V base={0,1.5f,0},right,up;std::vector<float>d,min,max; };
extern V sun,skyZenith,skyHorizon,sunTint;
extern float daylight,lampFactor;
extern std::vector<Light> pointLights;
void updateLighting(float hour);
bool loadScene(const wchar_t* path,Scene& s,std::wstring& error);
void closest(const Scene& s,const Ray& r,Hit& best,Stats* st,bool ground=true);
int groundMaterial(const Scene& s,V p);
ShadowMap buildShadow(const Scene& s,Stats& st);
V shade(const Scene& s,const ShadowMap& sm,const Camera& cam,const Ray& r,const Hit& h,Stats& st,bool fog,bool reflections=true);
Camera camera(float angleDeg,float elevationDeg,float distance,int w,int h);
Stats render(const Scene& s,const ShadowMap& sm,const Camera& cam,int w,int h,std::vector<uint32_t>& pixels,unsigned threads,bool fog,const std::atomic<uint64_t>& generation,uint64_t token,unsigned samplesPerPixel=1,bool depthOfField=false);
std::vector<uint32_t> upscaleBilinear(const std::vector<uint32_t>& src,int sw,int sh,int dw,int dh,unsigned threads);
std::vector<uint32_t> downsample2x(const std::vector<uint32_t>& src,int sw,int sh,unsigned threads);
std::vector<uint32_t> antialiasEdges4(const std::vector<uint32_t>& src,int w,int h,unsigned threads);
}
