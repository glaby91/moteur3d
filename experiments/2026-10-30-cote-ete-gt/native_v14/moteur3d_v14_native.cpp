#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

// Moteur 3D natif V14 : lancer de rayons logiciel, BVH et rendu parallèle par tuiles.
// Aucun navigateur, WebView, WebGL ou API de rendu 3D matériel.
namespace m3d {
constexpr float PI=3.14159265358979323846f, INF=1e30f;
struct V { float x=0,y=0,z=0; V operator+(V b)const{return{x+b.x,y+b.y,z+b.z};} V operator-(V b)const{return{x-b.x,y-b.y,z-b.z};} V operator-()const{return{-x,-y,-z};} V operator*(float s)const{return{x*s,y*s,z*s};} V operator/(float s)const{return{x/s,y/s,z/s};} };
inline V operator*(float s,V a){return a*s;} inline float dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;} inline V cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};} inline float len(V a){return std::sqrt(dot(a,a));} inline V norm(V a){float l=len(a);return l>1e-12f?a/l:V{0,1,0};} inline V had(V a,V b){return{a.x*b.x,a.y*b.y,a.z*b.z};} inline V clampv(V a,float lo=0,float hi=1){return{std::clamp(a.x,lo,hi),std::clamp(a.y,lo,hi),std::clamp(a.z,lo,hi)};}
struct Ray{V o,d;};
struct Mat{V color;float rough=.7f,metal=0,emission=0;};
enum class Kind:uint8_t{Sphere,Ellipsoid,Box,Triangle,Cylinder};
struct Obj{Kind kind;int mat;V a,b,c;float r=0;V mn,mx;};
struct Node{V mn,mx;int left=-1,right=-1,start=0,count=0;};
struct Hit{float t=INF;V p,n;int mat=-1;};
struct Cam{V eye,forward,right,up;float tanHalf,aspect;};
struct Light{V p,color;float power,radius;};
struct Scene{std::vector<Mat> mats;std::vector<Obj> objs;std::vector<int> order;std::vector<Node> bvh;std::vector<Light> lights;V sunDir;int root=-1;};

static V color(float r,float g,float b){return{r/255.f,g/255.f,b/255.f};}
static void bounds(Obj&o){
 if(o.kind==Kind::Sphere){o.mn=o.a-V{o.r,o.r,o.r};o.mx=o.a+V{o.r,o.r,o.r};}
 else if(o.kind==Kind::Ellipsoid){o.mn=o.a-o.b;o.mx=o.a+o.b;}
 else if(o.kind==Kind::Box){o.mn={std::min(o.a.x,o.b.x),std::min(o.a.y,o.b.y),std::min(o.a.z,o.b.z)};o.mx={std::max(o.a.x,o.b.x),std::max(o.a.y,o.b.y),std::max(o.a.z,o.b.z)};}
 else if(o.kind==Kind::Triangle){o.mn={std::min({o.a.x,o.b.x,o.c.x}),std::min({o.a.y,o.b.y,o.c.y}),std::min({o.a.z,o.b.z,o.c.z})};o.mx={std::max({o.a.x,o.b.x,o.c.x}),std::max({o.a.y,o.b.y,o.c.y}),std::max({o.a.z,o.b.z,o.c.z})};}
 else {V e={o.r,o.r,o.r};o.mn={std::min(o.a.x,o.b.x)-e.x,std::min(o.a.y,o.b.y)-e.y,std::min(o.a.z,o.b.z)-e.z};o.mx={std::max(o.a.x,o.b.x)+e.x,std::max(o.a.y,o.b.y)+e.y,std::max(o.a.z,o.b.z)+e.z};}
 const float e=.001f;o.mn=o.mn-V{e,e,e};o.mx=o.mx+V{e,e,e};
}
static int mat(Scene&s,V c,float rough=.75f,float metal=0,float em=0){s.mats.push_back({c,rough,metal,em});return int(s.mats.size()-1);}
static void add(Scene&s,Kind k,int m,V a,V b={},V c={},float r=0){Obj o{k,m,a,b,c,r};bounds(o);s.objs.push_back(o);}
static void sphere(Scene&s,int m,V p,float r){add(s,Kind::Sphere,m,p,{}, {},r);}
static void ellipsoid(Scene&s,int m,V p,V radii){add(s,Kind::Ellipsoid,m,p,radii);}
static void box(Scene&s,int m,V a,V b){add(s,Kind::Box,m,a,b);}
static void tri(Scene&s,int m,V a,V b,V c){add(s,Kind::Triangle,m,a,b,c);}
static void quad(Scene&s,int m,V a,V b,V c,V d){tri(s,m,a,b,c);tri(s,m,a,c,d);}
static void cylinder(Scene&s,int m,V a,V b,float r){add(s,Kind::Cylinder,m,a,b,{},r);}
static void torus(Scene&s,int m,V center,float xSide,float major,float minor){
 constexpr int N=20,M=6;V v[N][M];
 for(int i=0;i<N;i++){float a=2*PI*i/N;for(int j=0;j<M;j++){float b=2*PI*j/M;float rr=major+minor*std::cos(b);v[i][j]={center.x+xSide+minor*std::sin(b),center.y+rr*std::cos(a),center.z+rr*std::sin(a)};}}
 for(int i=0;i<N;i++)for(int j=0;j<M;j++){int ni=(i+1)%N,nj=(j+1)%M;quad(s,m,v[i][j],v[ni][j],v[ni][nj],v[i][nj]);}
}

static void addCar(Scene&s,V pos,int paint,bool detail){
 const int rubber=mat(s,color(22,23,24),.94f),rim=mat(s,color(160,173,181),.28f,.78f),dark=mat(s,color(14,23,29),.32f,.18f),chrome=mat(s,color(156,162,161),.3f,.7f),glass=mat(s,color(38,62,75),.19f,.22f),lamp=mat(s,color(255,218,158),.18f,0,1.2f),tail=mat(s,color(210,28,18),.28f,0,.22f),grill=mat(s,color(18,22,24),.6f,.22f),body=paint;
 auto P=[&](float x,float y,float z){return V{pos.x+x,pos.y+y,pos.z+z};};
 // Sculpted lower shell and long tapered hood/trunk.
 box(s,body,P(-.83f,.37f,-2.18f),P(.83f,.86f,2.03f));
 ellipsoid(s,body,P(0,.66f,-1.34f),{.87f,.23f,1.02f});
 ellipsoid(s,body,P(0,.69f,1.55f),{.72f,.22f,.57f});
 // Fastback roof, front/rear screens and two framed side windows.
 const float x=.67f;
 quad(s,body,P(-x,1.87f,-.43f),P(x,1.87f,-.43f),P(x,1.87f,.68f),P(-x,1.87f,.68f));
 quad(s,glass,P(-.69f,1.77f,-.48f),P(.69f,1.77f,-.48f),P(.75f,.96f,-1.20f),P(-.75f,.96f,-1.20f));
 quad(s,glass,P(.68f,1.78f,.72f),P(-.68f,1.78f,.72f),P(-.76f,1.02f,1.78f),P(.76f,1.02f,1.78f));
 for(float side:{-1.f,1.f}){
  quad(s,glass,P(side*.78f,.99f,-.91f),P(side*.68f,1.75f,-.35f),P(side*.68f,1.75f,.60f),P(side*.78f,1.02f,1.36f));
  cylinder(s,body,P(side*.80f,.96f,-.97f),P(side*.69f,1.82f,-.39f),.035f);
  cylinder(s,body,P(side*.69f,1.82f,.63f),P(side*.79f,1.03f,1.48f),.04f);
  cylinder(s,body,P(side*.78f,1.02f,1.36f),P(side*.75f,.88f,1.96f),.05f);
 }
 // Roof rails and door seams.
 cylinder(s,body,P(-.67f,1.88f,-.43f),P(-.67f,1.88f,.68f),.045f);cylinder(s,body,P(.67f,1.88f,-.43f),P(.67f,1.88f,.68f),.045f);
 for(float side:{-1.f,1.f}){
  cylinder(s,chrome,P(side*.835f,.63f,-.62f),P(side*.835f,.63f,1.12f),.012f);
  box(s,grill,P(side*.838f,.50f,-.12f),P(side*.85f,.74f,.42f));
  box(s,dark,P(side*.842f,.90f,.08f),P(side*.854f,.95f,.37f));
 }
 // Front grille, paired round lamps and bumpers, front faces toward -Z.
 box(s,grill,P(-.51f,.48f,-2.245f),P(.51f,.73f,-2.205f));
 for(int k=0;k<5;k++)box(s,chrome,P(-.46f,.50f+k*.045f,-2.26f),P(.46f,.51f+k*.045f,-2.245f));
 box(s,chrome,P(-.82f,.34f,-2.27f),P(.82f,.40f,-2.17f));
 for(float side:{-1.f,1.f})for(float dx:{.55f,.72f})sphere(s,lamp,P(side*dx,.66f,-2.21f),.105f);
 // Rear lamp strip and bumpers.
 box(s,chrome,P(-.78f,.37f,2.08f),P(.78f,.44f,2.16f));
 for(float side:{-1.f,1.f})for(int i=0;i<3;i++)box(s,tail,P(side*.68f,.57f+i*.055f,2.055f),P(side*.78f,.60f+i*.055f,2.09f));
 // Four tires sit just outside the fenders; detailed multi-spoke alloy wheels.
 for(float side:{-1.f,1.f})for(float z:{-1.36f,1.31f}){
  V c=P(side*.91f,.43f,z);ellipsoid(s,rubber,c,{.17f,.355f,.355f});ellipsoid(s,rim,P(side*1.105f,.43f,z),{.045f,.225f,.225f});
  torus(s,rim,c,side*.17f,.276f,.028f);ellipsoid(s,body,P(side*1.19f,.43f,z),{.035f,.074f,.074f});
  if(detail)torus(s,body,c,-side*.08f,.385f,.042f); // rolled fender lip around each wheel
  if(detail)for(int k=0;k<10;k++){float a=2*PI*k/10;V end=P(side*1.18f,.43f+.205f*std::cos(a),z+.205f*std::sin(a));cylinder(s,rim,P(side*1.18f,.43f,z),end,.018f);}
 }
 if(detail){
  // Two restrained hood vents and side exhaust.
  for(float side:{-1.f,1.f}){box(s,grill,P(side*.43f,.94f,-1.4f),P(side*.66f,.96f,-.99f));for(int k=0;k<3;k++)box(s,chrome,P(side*.45f,.965f,-1.35f+k*.1f),P(side*.63f,.976f,-1.32f+k*.1f));cylinder(s,chrome,P(side*.87f,.30f,-.7f),P(side*.87f,.30f,.85f),.035f);}
 }
}

static void palm(Scene&s,V base,float h){
 int trunk=mat(s,color(75,53,37),.9f),leaf=mat(s,color(37,75,47),.9f),leaf2=mat(s,color(64,100,50),.85f);
 V top=base+V{.09f,h,0};cylinder(s,trunk,base,top,.095f);
 for(int k=0;k<8;k++){float a=2*PI*k/8;V end=top+V{std::cos(a)*1.5f,-.36f,std::sin(a)*1.5f};V mid=top+V{std::cos(a)*.68f,-.10f,std::sin(a)*.68f};cylinder(s,leaf2,top,mid,.025f);for(int j=0;j<5;j++){float t=.2f+j*.15f;V p=top+(end-top)*t;V q=p+V{-std::sin(a)*.22f,-.12f,std::cos(a)*.22f};tri(s,j%2?leaf:leaf2,p,q,p+V{std::cos(a)*.34f,-.08f,std::sin(a)*.34f});tri(s,j%2?leaf:leaf2,p,q,p+V{-std::cos(a)*.34f,-.08f,-std::sin(a)*.34f});}}
}
static void addScene(Scene&s){
 int road=mat(s,color(37,43,44),.97f),sidewalk=mat(s,color(135,127,105),.9f),rail=mat(s,color(44,49,48),.56f,.4f),sand=mat(s,color(181,156,111),.9f),sea=mat(s,color(29,103,130),.24f,.16f),white=mat(s,color(216,213,191),.8f),lampMat=mat(s,color(255,188,112),.22f,0,1.7f);
 int orange=mat(s,color(193,72,29),.34f,.32f),red=mat(s,color(190,39,32),.35f,.28f),purple=mat(s,color(113,47,145),.38f,.24f);
 // Broad coastal asphalt, tiled sidewalk and ocean plane.
 box(s,road,-V{7,.12f,95},V{7,0,40});
 box(s,sidewalk,{-9,-.01f,-80},{-7,.08f,38});
 box(s,sand,{-50,-.18f,-100},{-9,-.02f,45});
 // Ocean sheet is represented by a large box surface with low rolling wave ridges.
 box(s,sea,{-60,-.35f,-130},{-9.05f,-.10f,70});
 int wave=mat(s,color(48,133,154),.3f,.13f);
 for(int k=0;k<18;k++){float z=-42+k*5.1f;box(s,wave,{-53,-.085f,z},{-9.1f,-.065f,z+.045f});}
 // Broken lane markings.
 for(int k=-12;k<12;k++)box(s,white,{-.045f,.006f,k*5.f-2},{.045f,.012f,k*5.f});
 // Guardrail, posts and coastal promenade lamps.
 for(int k=-24;k<=8;k++){
  float z=k*3.4f;cylinder(s,rail,{-7.15f,.13f,z},{-7.15f,.94f,z},.055f);
  cylinder(s,rail,{-7.18f,.72f,z-1.7f},{-7.18f,.72f,z+1.7f},.055f);
  if(k%6==0){int pole=mat(s,color(40,42,39),.6f,.25f);cylinder(s,pole,{-8.2f,.1f,z},{-8.2f,3.2f,z},.065f);cylinder(s,pole,{-8.2f,3.05f,z},{-7.75f,3.05f,z},.04f);sphere(s,lampMat,{-7.72f,3.02f,z},.16f);s.lights.push_back({{-7.72f,2.95f,z},color(255,171,96),18,4.3f});}
 }
 palm(s,{-10,.05f,-24},5.0f);palm(s,{-11,.05f,-8},7.0f);palm(s,{-10,.05f,11},5.7f);
 // Three same-direction restomod coupes; offset lanes suggest an overtake.
 addCar(s,{0,.03f,-10.0f},orange,true);addCar(s,{2.25f,.03f,-2.0f},purple,false);addCar(s,{-2.25f,.03f,-6.5f},red,false);
 s.sunDir=norm(V{-.42f,.78f,-.36f});
}

static bool boxHit(const Ray&r,V mn,V mx,float maxT,float&nearT){float lo=.0005f,hi=maxT;for(int k=0;k<3;k++){float o=k==0?r.o.x:k==1?r.o.y:r.o.z,d=k==0?r.d.x:k==1?r.d.y:r.d.z,a=k==0?mn.x:k==1?mn.y:mn.z,b=k==0?mx.x:k==1?mx.y:mx.z;if(std::abs(d)<1e-9f){if(o<a||o>b)return false;}else{float x=(a-o)/d,y=(b-o)/d;if(x>y)std::swap(x,y);lo=std::max(lo,x);hi=std::min(hi,y);if(hi<lo)return false;}}nearT=lo;return hi>.0005f;}
static bool intersect(const Ray&r,const Obj&o,float maxT,Hit&h){
 float t=INF;V n;
 if(o.kind==Kind::Sphere){V q=r.o-o.a;float A=dot(r.d,r.d),B=dot(q,r.d),C=dot(q,q)-o.r*o.r,D=B*B-A*C;if(D<0)return false;float root=std::sqrt(D);t=(-B-root)/A;if(t<.0005f)t=(-B+root)/A;if(t<.0005f)return false;n=norm((r.o+r.d*t-o.a)/o.r);}
 else if(o.kind==Kind::Ellipsoid){V q=r.o-o.a,iv={1/o.b.x,1/o.b.y,1/o.b.z},qo=had(q,iv),rd=had(r.d,iv);float A=dot(rd,rd),B=dot(qo,rd),C=dot(qo,qo)-1,D=B*B-A*C;if(D<0)return false;float root=std::sqrt(D);t=(-B-root)/A;if(t<.0005f)t=(-B+root)/A;if(t<.0005f)return false;V p=r.o+r.d*t;n=norm({(p.x-o.a.x)/(o.b.x*o.b.x),(p.y-o.a.y)/(o.b.y*o.b.y),(p.z-o.a.z)/(o.b.z*o.b.z)});}
 else if(o.kind==Kind::Box){if(!boxHit(r,o.mn,o.mx,maxT,t))return false;V p=r.o+r.d*t;float eps=.002f;if(std::abs(p.x-o.mn.x)<eps)n={-1,0,0};else if(std::abs(p.x-o.mx.x)<eps)n={1,0,0};else if(std::abs(p.y-o.mn.y)<eps)n={0,-1,0};else if(std::abs(p.y-o.mx.y)<eps)n={0,1,0};else if(std::abs(p.z-o.mn.z)<eps)n={0,0,-1};else n={0,0,1};}
 else if(o.kind==Kind::Triangle){V e1=o.b-o.a,e2=o.c-o.a,p=cross(r.d,e2);float det=dot(e1,p);if(std::abs(det)<1e-8f)return false;float inv=1/det;V q=r.o-o.a;float u=dot(q,p)*inv;if(u<0||u>1)return false;V vq=cross(q,e1);float v=dot(r.d,vq)*inv;if(v<0||u+v>1)return false;t=dot(e2,vq)*inv;if(t<.0005f||t>=maxT)return false;n=norm(cross(e1,e2));if(dot(n,r.d)>0)n=n*-1;}
 else {V ba=o.b-o.a,oa=r.o-o.a;float baba=dot(ba,ba),bard=dot(ba,r.d),baoa=dot(ba,oa),rdoa=dot(r.d,oa),oaoa=dot(oa,oa),A=baba*dot(r.d,r.d)-bard*bard,B=baba*rdoa-baoa*bard,C=baba*oaoa-baoa*baoa-o.r*o.r*baba,D=B*B-A*C;if(D<0||std::abs(A)<1e-9f)return false;t=(-B-std::sqrt(D))/A;float y=baoa+t*bard;if(t<.0005f||y<0||y>baba)return false;V p=r.o+r.d*t,axis=o.a+ba*(y/baba);n=norm(p-axis);}
 if(t>=maxT)return false;h={t,r.o+r.d*t,n,o.mat};return true;
}
static int buildNode(Scene&s,int start,int count){
 Node n;n.mn={INF,INF,INF};n.mx={-INF,-INF,-INF};V cmn=n.mn,cmx=n.mx;
 for(int i=start;i<start+count;i++){const Obj&o=s.objs[s.order[i]];n.mn={std::min(n.mn.x,o.mn.x),std::min(n.mn.y,o.mn.y),std::min(n.mn.z,o.mn.z)};n.mx={std::max(n.mx.x,o.mx.x),std::max(n.mx.y,o.mx.y),std::max(n.mx.z,o.mx.z)};V c=(o.mn+o.mx)*.5f;cmn={std::min(cmn.x,c.x),std::min(cmn.y,c.y),std::min(cmn.z,c.z)};cmx={std::max(cmx.x,c.x),std::max(cmx.y,c.y),std::max(cmx.z,c.z)};}
 int id=int(s.bvh.size());s.bvh.push_back(n);if(count<=6){s.bvh[id].start=start;s.bvh[id].count=count;return id;}
 V ext=cmx-cmn;int axis=ext.y>ext.x?1:0;if((axis==0?ext.x:ext.y)<ext.z)axis=2;
 int mid=start+count/2;std::nth_element(s.order.begin()+start,s.order.begin()+mid,s.order.begin()+start+count,[&](int a,int b){V ca=(s.objs[a].mn+s.objs[a].mx)*.5f,cb=(s.objs[b].mn+s.objs[b].mx)*.5f;return axis==0?ca.x<cb.x:axis==1?ca.y<cb.y:ca.z<cb.z;});
 int l=buildNode(s,start,mid-start),r=buildNode(s,mid,start+count-mid);s.bvh[id].left=l;s.bvh[id].right=r;return id;
}
static void buildBVH(Scene&s){s.order.resize(s.objs.size());for(int i=0;i<int(s.order.size());i++)s.order[i]=i;s.bvh.reserve(s.objs.size()*2);s.root=buildNode(s,0,int(s.order.size()));}
static bool closest(const Scene&s,const Ray&r,Hit&best,std::atomic<uint64_t>*tests=nullptr){
 bool found=false;std::array<int,128> stack{};int top=0;if(s.root>=0)stack[top++]=s.root;
 while(top){const Node&n=s.bvh[stack[--top]];float tNear;if(tests)tests->fetch_add(1,std::memory_order_relaxed);if(!boxHit(r,n.mn,n.mx,best.t,tNear))continue;
  if(n.count){for(int i=n.start;i<n.start+n.count;i++){if(tests)tests->fetch_add(1,std::memory_order_relaxed);Hit h;if(intersect(r,s.objs[s.order[i]],best.t,h)){best=h;found=true;}}}
  else{if(top<126){stack[top++]=n.left;stack[top++]=n.right;}}
 }
 // Analytic road and ocean planes.
 if(std::abs(r.d.y)>1e-8f){float t=-r.o.y/r.d.y;if(t>.0005f&&t<best.t){V p=r.o+r.d*t;if(std::abs(p.x)<7.1f&&p.z<42&&p.z>-100){V n={0,1,0};if(r.d.y>0)n=n*-1;best={t,p,n,0};found=true;}}
  float seaY=-.06f; t=(seaY-r.o.y)/r.d.y;if(t>.0005f&&t<best.t){V p=r.o+r.d*t;if(p.x<-9&&p.z<70&&p.z>-130){best={t,p,{0,1,0},1};found=true;}}}
 return found;
}
static bool occluded(const Scene&s,Ray r,float maxT){Hit h;h.t=maxT;return closest(s,r,h);}
static V sky(V d){float t=std::clamp(d.y*.5f+.5f,0.f,1.f);V c=color(78,155,215)*(t)+color(218,168,132)*(1-t);V sun=norm(V{-.42f,.78f,-.36f});float a=dot(d,sun);if(a>.99935f)c=c*.25f+color(255,223,170)*.75f;else if(a>.996f)c=c+color(255,196,126)*((a-.996f)*60);return c;}
static V shade(const Scene&s,const Ray&r,const Hit&h){
 if(h.mat<0)return sky(r.d);V base=s.mats[h.mat].color;float rough=s.mats[h.mat].rough;V n=h.n,p=h.p;
 if(h.mat==0){float checker=(int(std::floor(p.x*3.2f))+int(std::floor(p.z*3.2f)))&1;base=checker?color(31,36,37):color(35,39,40);float dash=std::abs(p.x)<.05f&&std::fmod(std::abs(p.z+2.5f),5.f)<2.5f;if(dash)base=color(153,150,132);}
 if(h.mat==1){float wave=.5f+.5f*std::sin(p.z*1.1f+std::sin(p.x*.24f)*3.f);base=color(18,77,105)*(1-wave*.16f)+color(39,128,151)*(wave*.16f);}
 V sun=norm(s.sunDir),view=norm(r.o-p);float diff=std::max(0.f,dot(n,sun));float sh=1.f;
 // Keep the expensive BVH shadow ray for the directional sun only. Local lamps
 // intentionally use unshadowed diffuse fill, as in the previous software build.
 if(diff>.01f&&h.mat>=2){Ray sr{p+n*.012f,sun};if(occluded(s,sr,45.f))sh=.32f;}
 V result=base*(.16f+diff*.55f*sh);
 for(const Light&l:s.lights){V delta=l.p-p;float d=len(delta);if(d>=l.radius)continue;V ld=delta/(d>1e-4f?d:1);float fall=std::pow(std::max(0.f,1-d/l.radius),2)*l.power;float lam=std::max(0.f,dot(n,ld));if(lam>0)result=result+had(base,l.color)*(fall*lam*.035f);}
 V hvec=norm(sun+view);float spec=std::pow(std::max(0.f,dot(n,hvec)),std::max(3.f,96*(1-rough)));result=result+color(255,218,173)*(spec*(.18f+diff*.24f)*sh);
 float fog=std::clamp((h.t-34.f)/120.f,0.f,.65f);result=result*(1-fog)+color(166,177,181)*fog;
 return clampv(result,0,1);
}
static Ray cameraRay(const Cam&c,int x,int y,int w,int h){float sx=(2*(x+.5f)/w-1)*c.tanHalf,sy=(1-2*(y+.5f)/h)*c.tanHalf/c.aspect;return{c.eye,norm(c.forward+c.right*sx+c.up*sy)};}
static uint32_t pack(V c){c=clampv(c);auto enc=[](float x){x=x/(1+x*.2f);x=std::pow(std::clamp(x,0.f,1.f),1/2.2f);return uint32_t(x*255+.5f);};return 0xff000000u|(enc(c.x)<<16)|(enc(c.y)<<8)|enc(c.z);}
struct RenderStats{uint64_t rayTests=0;double ms=0;unsigned threads=1;};
static RenderStats render(const Scene&s,const Cam&cam,int w,int h,std::vector<uint32_t>&pixels,unsigned threads,const std::atomic<uint64_t>&cancel,uint64_t token){
 auto start=std::chrono::steady_clock::now();std::atomic<int> next{0};std::atomic<uint64_t> tests{0};int nx=(w+15)/16,ny=(h+15)/16,total=nx*ny;
 auto worker=[&]{for(;;){if(cancel.load(std::memory_order_relaxed)!=token)return;int ti=next.fetch_add(1);if(ti>=total)return;int x0=(ti%nx)*16,y0=(ti/nx)*16;
  for(int y=y0;y<std::min(y0+16,h);y++)for(int x=x0;x<std::min(x0+16,w);x++){Ray r=cameraRay(cam,x,y,w,h);Hit hit;closest(s,r,hit,&tests);V c=shade(s,r,hit);pixels[size_t(y)*w+x]=pack(c);}
 }};
 threads=std::max(1u,std::min(threads,unsigned(total)));std::vector<std::thread> pool;pool.reserve(threads);for(unsigned i=0;i<threads;i++)pool.emplace_back(worker);for(auto&t:pool)t.join();
 RenderStats q;q.rayTests=tests.load();q.ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();q.threads=threads;return q;
}
static Cam camera(float angleDeg,float elevDeg,float distance,float aspect){float a=angleDeg*PI/180,e=elevDeg*PI/180;V target{0,1.0f,-5.6f};V eye=target+V{distance*std::sin(a)*std::cos(e),distance*std::sin(e),distance*std::cos(a)*std::cos(e)};Cam c;c.eye=eye;c.forward=norm(target-eye);c.right=norm(cross(c.forward,{0,1,0}));c.up=cross(c.right,c.forward);c.tanHalf=std::tan(50*PI/360);c.aspect=aspect;return c;}
}

#ifdef _WIN32
static constexpr UINT WM_RENDERED=WM_APP+1;
enum{ID_RES=101,ID_THREADS=102,ID_RENDER=103,ID_HOME=104,ID_FRONT=105,ID_REAR=106,ID_THREADLABEL=107};
static int gW=1280,gH=720,gReqW=1280,gReqH=720;static std::vector<uint32_t> gPixels;static std::mutex gPixelsMutex,gRequestMutex;static std::condition_variable gRequestCv;static std::atomic<uint64_t> gGeneration{0};static std::thread gCoordinator;static HWND gWindow=nullptr,gResolution=nullptr,gThreadSlider=nullptr,gThreadLabel=nullptr,gStatus=nullptr,gTimeLabel=nullptr,gRayLabel=nullptr,gBvhLabel=nullptr,gObjectLabel=nullptr;static HFONT gFont=nullptr;static m3d::Scene gScene;static float gAngle=-137,gElevation=7,gDistance=17;static m3d::RenderStats gStats;static unsigned gThreads=std::max(1u,std::min(16u,std::thread::hardware_concurrency()>1?std::thread::hardware_concurrency()-1:1u));static bool gStop=false,gPending=false;
static void queueRender(){++gGeneration;{std::lock_guard<std::mutex> lock(gRequestMutex);gPending=true;}if(gStatus)SetWindowTextW(gStatus,L"Calcul logiciel CPU en cours — rendu multithreadé par tuiles…");gRequestCv.notify_one();}
static void renderWorker(){for(;;){std::unique_lock<std::mutex> lk(gRequestMutex);gRequestCv.wait(lk,[]{return gStop||gPending;});if(gStop)return;gPending=false;uint64_t token=gGeneration.load();int w=gReqW,h=gReqH;float angle=gAngle,elev=gElevation,dist=gDistance;unsigned threads=gThreads;lk.unlock();std::vector<uint32_t> frame(size_t(w)*h);auto stats=m3d::render(gScene,m3d::camera(angle,elev,dist,float(w)/h),w,h,frame,threads,gGeneration,token);if(gGeneration.load()!=token)continue;{std::lock_guard<std::mutex> lock(gPixelsMutex);gW=w;gH=h;gPixels.swap(frame);gStats=stats;}PostMessageW(gWindow,WM_RENDERED,0,0);}}
static HWND child(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id){HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);if(gFont)SendMessageW(c,WM_SETFONT,(WPARAM)gFont,TRUE);return c;}
static void layout(HWND h){RECT r;GetClientRect(h,&r);int w=r.right-r.left,hh=r.bottom-r.top,panel=std::clamp(w/3,355,440),left=w-panel;MoveWindow(gResolution,left+22,76,panel-44,250,TRUE);MoveWindow(gThreadSlider,left+22,176,panel-44,34,TRUE);MoveWindow(gThreadLabel,left+22,213,panel-44,24,TRUE);MoveWindow(GetDlgItem(h,ID_RENDER),left+22,260,panel-44,38,TRUE);MoveWindow(GetDlgItem(h,ID_HOME),left+22,308,(panel-52)/2,34,TRUE);MoveWindow(GetDlgItem(h,ID_FRONT),left+30+(panel-52)/2,308,(panel-52)/2,34,TRUE);MoveWindow(GetDlgItem(h,ID_REAR),left+22,350,panel-44,34,TRUE);MoveWindow(gStatus,left+22,405,panel-44,44,TRUE);MoveWindow(gTimeLabel,left+22,490,panel-44,32,TRUE);MoveWindow(gRayLabel,left+22,526,panel-44,30,TRUE);MoveWindow(gBvhLabel,left+22,560,panel-44,48,TRUE);MoveWindow(gObjectLabel,left+22,616,panel-44,48,TRUE);InvalidateRect(h,nullptr,FALSE);}
static void drawText(HDC dc,int x,int y,const wchar_t*s,COLORREF c,int size=16,bool bold=false){HFONT f=CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");HFONT old=(HFONT)SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,c);TextOutW(dc,x,y,s,(int)wcslen(s));SelectObject(dc,old);DeleteObject(f);}
static void setStatLabels(){m3d::RenderStats stats;int w,h;{std::lock_guard<std::mutex> lock(gPixelsMutex);stats=gStats;w=gW;h=gH;}wchar_t b[160];swprintf_s(b,L"Dernier rendu : %.1f ms  |  %u threads",stats.ms,stats.threads);SetWindowTextW(gTimeLabel,b);swprintf_s(b,L"Pixels calculés : %d × %d  (%llu rayons)",w,h,(unsigned long long)w*h);SetWindowTextW(gRayLabel,b);swprintf_s(b,L"Tests BVH : %llu  |  ≈ %.1f par rayon",(unsigned long long)stats.rayTests,double(stats.rayTests)/std::max(1,w*h));SetWindowTextW(gBvhLabel,b);swprintf_s(b,L"Géométrie : %llu primitives, dont triangles de détail",(unsigned long long)gScene.objs.size());SetWindowTextW(gObjectLabel,b);SetWindowTextW(gStatus,L"Image terminée — rendu logiciel CPU, sans accélération graphique matérielle.");}
static void applyResolution(){int ix=(int)SendMessageW(gResolution,CB_GETCURSEL,0,0);int w=ix==0?960:ix==1?1280:ix==2?1680:1920,h=ix==0?540:ix==1?720:ix==2?945:1080;{std::lock_guard<std::mutex> lock(gRequestMutex);gReqW=w;gReqH=h;}queueRender();}
static void setCamera(float angle,float elevation,float distance){std::lock_guard<std::mutex> lock(gRequestMutex);gAngle=angle;gElevation=elevation;gDistance=distance;}
static LRESULT CALLBACK windowProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){case WM_CREATE:{gWindow=h;INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_BAR_CLASSES};InitCommonControlsEx(&ic);gFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");gResolution=child(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,ID_RES);SendMessageW(gResolution,CB_ADDSTRING,0,(LPARAM)L"Rapide — 960 × 540");SendMessageW(gResolution,CB_ADDSTRING,0,(LPARAM)L"Équilibré — 1280 × 720");SendMessageW(gResolution,CB_ADDSTRING,0,(LPARAM)L"Précis — 1680 × 945");SendMessageW(gResolution,CB_ADDSTRING,0,(LPARAM)L"Natif — 1920 × 1080");SendMessageW(gResolution,CB_SETCURSEL,1,0);gThreadSlider=child(h,TRACKBAR_CLASSW,L"",TBS_AUTOTICKS,ID_THREADS);SendMessageW(gThreadSlider,TBM_SETRANGE,TRUE,MAKELONG(1,(short)std::min(32u,std::max(1u,std::thread::hardware_concurrency()))));SendMessageW(gThreadSlider,TBM_SETPOS,TRUE,gThreads);gThreadLabel=child(h,L"STATIC",L"",0,ID_THREADLABEL);child(h,L"BUTTON",L"Calculer l’image",BS_PUSHBUTTON,ID_RENDER);child(h,L"BUTTON",L"Vue initiale",BS_PUSHBUTTON,ID_HOME);child(h,L"BUTTON",L"Vue avant",BS_PUSHBUTTON,ID_FRONT);child(h,L"BUTTON",L"Vue arrière",BS_PUSHBUTTON,ID_REAR);gStatus=child(h,L"STATIC",L"Initialisation du rendu…",SS_LEFT,108);gTimeLabel=child(h,L"STATIC",L"Dernier rendu : —",SS_LEFT,109);gRayLabel=child(h,L"STATIC",L"Pixels calculés : —",SS_LEFT,110);gBvhLabel=child(h,L"STATIC",L"Tests BVH : —",SS_LEFT,111);gObjectLabel=child(h,L"STATIC",L"Géométrie : —",SS_LEFT,112);gCoordinator=std::thread(renderWorker);layout(h);SendMessageW(gResolution,CB_SETCURSEL,1,0);queueRender();SendMessageW(gThreadSlider,TBM_SETPOS,TRUE,gThreads);wchar_t b[80];swprintf_s(b,L"Threads CPU utilisés : %u",gThreads);SetWindowTextW(gThreadLabel,b);return 0;}case WM_SIZE:layout(h);return 0;case WM_COMMAND:if(LOWORD(w)==ID_RES&&HIWORD(w)==CBN_SELCHANGE){applyResolution();return 0;}if(LOWORD(w)==ID_RENDER){queueRender();return 0;}if(LOWORD(w)==ID_HOME){setCamera(-137,7,17);SendMessageW(gResolution,CB_SETCURSEL,1,0);applyResolution();return 0;}if(LOWORD(w)==ID_FRONT){setCamera(-45,7,17);queueRender();return 0;}if(LOWORD(w)==ID_REAR){setCamera(-317,9,17);queueRender();return 0;}break;case WM_HSCROLL:if((HWND)l==gThreadSlider){unsigned t=(unsigned)SendMessageW(gThreadSlider,TBM_GETPOS,0,0);{std::lock_guard<std::mutex> lock(gRequestMutex);gThreads=t;}wchar_t b[80];swprintf_s(b,L"Threads CPU utilisés : %u",t);SetWindowTextW(gThreadLabel,b);if(LOWORD(w)==TB_ENDTRACK||LOWORD(w)==TB_THUMBPOSITION){queueRender();}return 0;}break;case WM_KEYDOWN:{if(w==VK_ESCAPE){DestroyWindow(h);return 0;}float angle,elev,dist;{std::lock_guard<std::mutex> lock(gRequestMutex);angle=gAngle;elev=gElevation;dist=gDistance;}if(w==VK_LEFT)angle-=5;else if(w==VK_RIGHT)angle+=5;else if(w==VK_UP)elev=std::min(35.f,elev+3);else if(w==VK_DOWN)elev=std::max(-3.f,elev-3);else return 0;setCamera(angle,elev,dist);queueRender();return 0;}case WM_RENDERED:{setStatLabels();InvalidateRect(h,nullptr,FALSE);return 0;}case WM_CTLCOLORSTATIC:{HDC dc=(HDC)w;SetTextColor(dc,RGB(221,232,241));SetBkColor(dc,RGB(24,43,60));return (LRESULT)GetStockObject(NULL_BRUSH);}case WM_CTLCOLORBTN:{HDC dc=(HDC)w;SetTextColor(dc,RGB(233,241,247));SetBkColor(dc,RGB(43,78,101));return (LRESULT)GetStockObject(NULL_BRUSH);}case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);int ww=r.right,hh=r.bottom,panel=std::clamp(ww/3,355,440),left=ww-panel;HBRUSH bg=CreateSolidBrush(RGB(12,25,37)),pn=CreateSolidBrush(RGB(24,43,60)),canvas=CreateSolidBrush(RGB(3,9,14));RECT all{0,0,ww,hh};FillRect(dc,&all,bg);RECT pr{left,0,ww,hh};FillRect(dc,&pr,pn);drawText(dc,20,18,L"MOTEUR 3D V14  |  RENDU LOGICIEL CPU",RGB(232,241,248),20,true);drawText(dc,20,43,L"Côte d’été · fastback restomod · mer · palmiers · trois voitures",RGB(153,183,202),14);drawText(dc,left+22,20,L"PILOTAGE DU RENDU",RGB(232,241,248),18,true);drawText(dc,left+22,51,L"Qualité de sortie",RGB(180,204,220),14);drawText(dc,left+22,143,L"Nombre de threads CPU",RGB(180,204,220),14);drawText(dc,left+22,235,L"Actions",RGB(180,204,220),14);drawText(dc,left+22,465,L"DIAGNOSTICS",RGB(232,241,248),17,true);RECT cr{20,74,left-20,hh-20};FillRect(dc,&cr,canvas);{std::lock_guard<std::mutex> lock(gPixelsMutex);if(!gPixels.empty()){BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=gW;bi.bmiHeader.biHeight=-gH;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;int cw=cr.right-cr.left,ch=cr.bottom-cr.top;float scale=std::min(float(cw)/gW,float(ch)/gH);int dw=(int)(gW*scale),dh=(int)(gH*scale),x=cr.left+(cw-dw)/2,y=cr.top+(ch-dh)/2;SetStretchBltMode(dc,HALFTONE);StretchDIBits(dc,x,y,dw,dh,0,0,gW,gH,gPixels.data(),&bi,DIB_RGB_COLORS,SRCCOPY);}else drawText(dc,cr.left+40,cr.top+40,L"Calcul du premier rendu…",RGB(220,230,240),18);}DeleteObject(bg);DeleteObject(pn);DeleteObject(canvas);EndPaint(h,&ps);return 0;}case WM_DESTROY:{gGeneration++;{std::lock_guard<std::mutex> lock(gRequestMutex);gStop=true;}gRequestCv.notify_one();if(gCoordinator.joinable())gCoordinator.join();if(gFont)DeleteObject(gFont);PostQuitMessage(0);return 0;}}return DefWindowProcW(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE inst,HINSTANCE,PWSTR,int show){m3d::addScene(gScene);m3d::buildBVH(gScene);WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=windowProc;wc.hInstance=inst;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName=L"Moteur3DV14Native";RegisterClassExW(&wc);HWND h=CreateWindowExW(0,wc.lpszClassName,L"Moteur 3D V14 — application native CPU",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1480,900,nullptr,nullptr,inst,nullptr);if(!h)return 1;ShowWindow(h,show);UpdateWindow(h);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return int(msg.wParam);}
#else
int main(int argc,char**argv){int w=1280,h=720;std::string out="moteur3d_v14_native.ppm";if(argc>1)out=argv[1];if(argc>3){w=std::max(320,std::atoi(argv[2]));h=std::max(180,std::atoi(argv[3]));}m3d::Scene s;m3d::addScene(s);m3d::buildBVH(s);std::vector<uint32_t> p(size_t(w)*h);std::atomic<uint64_t> cancel{0};unsigned n=std::max(1u,std::thread::hardware_concurrency());auto stats=m3d::render(s,m3d::camera(-137,7,17,float(w)/h),w,h,p,n,cancel,0);FILE*f=std::fopen(out.c_str(),"wb");if(!f){std::perror("output");return 1;}std::fprintf(f,"P6\n%d %d\n255\n",w,h);for(uint32_t v:p){unsigned char rgb[3]={uint8_t((v>>16)&255),uint8_t((v>>8)&255),uint8_t(v&255)};std::fwrite(rgb,1,3,f);}std::fclose(f);std::cout<<"CPU software render: "<<w<<"x"<<h<<", "<<stats.threads<<" threads, "<<stats.rayTests<<" BVH tests, "<<stats.ms<<" ms, "<<out<<"\n";}
#endif
