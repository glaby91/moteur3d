#include "m3d_engine.h"

namespace m3d {
struct RayBoxCache{V inv;uint8_t parallel=0,negative=0;};
static RayBoxCache makeRayBoxCache(const Ray&r){RayBoxCache c;const float d[3]={r.d.x,r.d.y,r.d.z};float inv[3]{};for(int k=0;k<3;k++){if(std::abs(d[k])<1e-12f){c.parallel|=(uint8_t)(1u<<k);inv[k]=0;}else{inv[k]=1.f/d[k];if(d[k]<0)c.negative|=(uint8_t)(1u<<k);}}c.inv={inv[0],inv[1],inv[2]};return c;}
static bool boundsHit(const Ray&r,const RayBoxCache&c,const Node&n,float limit,float&nearDist){
 float x0=-INF,x1=INF,y0=-INF,y1=INF,z0=-INF,z1=INF;
 if(c.parallel&1){if(r.o.x<n.mn.x||r.o.x>n.mx.x)return false;}else if(c.negative&1){x0=(n.mx.x-r.o.x)*c.inv.x;x1=(n.mn.x-r.o.x)*c.inv.x;}else{x0=(n.mn.x-r.o.x)*c.inv.x;x1=(n.mx.x-r.o.x)*c.inv.x;}
 if(c.parallel&2){if(r.o.y<n.mn.y||r.o.y>n.mx.y)return false;}else if(c.negative&2){y0=(n.mx.y-r.o.y)*c.inv.y;y1=(n.mn.y-r.o.y)*c.inv.y;}else{y0=(n.mn.y-r.o.y)*c.inv.y;y1=(n.mx.y-r.o.y)*c.inv.y;}
 if(c.parallel&4){if(r.o.z<n.mn.z||r.o.z>n.mx.z)return false;}else if(c.negative&4){z0=(n.mx.z-r.o.z)*c.inv.z;z1=(n.mn.z-r.o.z)*c.inv.z;}else{z0=(n.mn.z-r.o.z)*c.inv.z;z1=(n.mx.z-r.o.z)*c.inv.z;}
 float lo=std::max(0.f,std::max(x0,std::max(y0,z0))),hi=std::min(limit,std::min(x1,std::min(y1,z1)));if(hi<lo)return false;nearDist=lo;return hi>=0;
}
static bool intersect(const Ray&r,const Obj&o,float limit,float&t){t=INF;
 if(o.kind==Kind::Triangle){V e1=o.b-o.a,e2=o.c-o.a,p=cross(r.d,e2);float det=dot(e1,p);if(std::abs(det)<1e-10f)return false;float inv=1/det;V q=r.o-o.a;float u=dot(q,p)*inv;if(u<0||u>1)return false;V z=cross(q,e1);float v=dot(r.d,z)*inv;if(v<0||u+v>1)return false;t=dot(e2,z)*inv;}
 else if(o.kind==Kind::Ellipse){V p={(r.o.x-o.a.x)/o.b.x,(r.o.y-o.a.y)/o.b.y,(r.o.z-o.a.z)/o.b.z},d={r.d.x/o.b.x,r.d.y/o.b.y,r.d.z/o.b.z};float A=dot(d,d),B=dot(p,d),C=dot(p,p)-1,D=B*B-A*C;if(D<0||A<1e-15f)return false;float q=(-B-std::sqrt(D))/A;t=q>.0001f?q:(-B+std::sqrt(D))/A;}
 else if(o.kind==Kind::Box||o.kind==Kind::RoundBox){float lo=-INF,hi=limit;for(int k=0;k<3;k++){float org=k==0?r.o.x:k==1?r.o.y:r.o.z,d=k==0?r.d.x:k==1?r.d.y:r.d.z,mn=k==0?o.mn.x:k==1?o.mn.y:o.mn.z,mx=k==0?o.mx.x:k==1?o.mx.y:o.mx.z;if(std::abs(d)<1e-12f){if(org<mn||org>mx)return false;}else{float a=(mn-org)/d,b=(mx-org)/d;if(a>b)std::swap(a,b);lo=std::max(lo,a);hi=std::min(hi,b);if(lo>hi)return false;}}
  if(o.kind==Kind::Box)t=lo>.0001f?lo:hi;else{t=std::max(lo,.0001f);bool hit=false;for(int i=0;i<64&&t<hi;i++){V p=r.o+r.d*t-o.center;float x=std::abs(p.x)-o.core.x,y=std::abs(p.y)-o.core.y,z=std::abs(p.z)-o.core.z;float d=std::sqrt(std::max(x,0.f)*std::max(x,0.f)+std::max(y,0.f)*std::max(y,0.f)+std::max(z,0.f)*std::max(z,0.f))+std::min(std::max(x,std::max(y,z)),0.f)-o.r;if(d<.001f){hit=true;break;}t+=std::max(d*.82f,.0005f);}if(!hit||t>=hi)return false;}}
 else if(o.kind==Kind::Capsule){V ba=o.b-o.a,oa=r.o-o.a;float baba=o.len2,rdba=dot(r.d,ba),oaba=dot(oa,ba),rdoa=dot(r.d,oa),oaoa=dot(oa,oa),A=baba*dot(r.d,r.d)-rdba*rdba,B=baba*rdoa-oaba*rdba,C=baba*oaoa-oaba*oaba-o.r*o.r*baba,h=B*B-A*C;if(A>1e-12f&&h>=0){float tc=(-B-std::sqrt(h))/A,y=oaba+tc*rdba;if(tc>.0001f&&y>0&&y<baba)t=tc;}for(V c:{o.a,o.b}){V q=r.o-c;float bb=dot(q,r.d),cc=dot(q,q)-o.r*o.r,disc=bb*bb-dot(r.d,r.d)*cc;if(disc>=0){float sq=std::sqrt(disc),t0=(-bb-sq)/dot(r.d,r.d),t1=(-bb+sq)/dot(r.d,r.d);if(t0>.0001f&&t0<t)t=t0;else if(t1>.0001f&&t1<t)t=t1;}}}
 else {V p=r.o-o.a;float z=dot(p,o.n0),zd=dot(r.d,o.n0);V q=p-o.n0*z,v=r.d-o.n0*zd;float r0=o.r+o.k*z,rd=o.k*zd,A=dot(v,v)-rd*rd,B=dot(q,v)-r0*rd,C=dot(q,q)-r0*r0;if(std::abs(A)>1e-12f){float D=B*B-A*C;if(D>=0){float s=std::sqrt(D);for(float t0:{(-B-s)/A,(-B+s)/A}){float h=z+t0*zd;if(t0>.0001f&&t0<t&&h>=0&&h<=o.len)t=t0;}}}if(std::abs(zd)>1e-12f)for(auto cap:{std::pair<float,float>{0,o.r},std::pair<float,float>{o.len,o.r1}}){float t0=(cap.first-z)/zd;if(t0>.0001f&&t0<t){V vq=q+v*t0;if(dot(vq,vq)<=cap.second*cap.second)t=t0;}}}
 return t>.0001f&&t<limit;
}
static V normalAt(const Obj&o,V p){if(o.kind==Kind::Triangle){V e1=o.b-o.a,e2=o.c-o.a,q=p-o.a;float d00=dot(e1,e1),d01=dot(e1,e2),d11=dot(e2,e2),d20=dot(q,e1),d21=dot(q,e2),den=d00*d11-d01*d01,inv=std::abs(den)>1e-20f?1/den:0,b1=(d11*d20-d01*d21)*inv,b2=(d00*d21-d01*d20)*inv,b0=1-b1-b2;return norm(o.n0*b0+o.n1*b1+o.n2*b2);}if(o.kind==Kind::Ellipse)return norm({(p.x-o.a.x)/(o.b.x*o.b.x),(p.y-o.a.y)/(o.b.y*o.b.y),(p.z-o.a.z)/(o.b.z*o.b.z)});if(o.kind==Kind::Box){float d[6]={std::abs(p.x-o.mn.x),std::abs(p.x-o.mx.x),std::abs(p.y-o.mn.y),std::abs(p.y-o.mx.y),std::abs(p.z-o.mn.z),std::abs(p.z-o.mx.z)};int k=int(std::min_element(d,d+6)-d);return k==0?V{-1,0,0}:k==1?V{1,0,0}:k==2?V{0,-1,0}:k==3?V{0,1,0}:k==4?V{0,0,-1}:V{0,0,1};}if(o.kind==Kind::RoundBox){V q=p-o.center;float x=std::abs(q.x)-o.core.x,y=std::abs(q.y)-o.core.y,z=std::abs(q.z)-o.core.z;V v{std::max(x,0.f)*(q.x>=0?1.f:-1.f),std::max(y,0.f)*(q.y>=0?1.f:-1.f),std::max(z,0.f)*(q.z>=0?1.f:-1.f)};if(length(v)>1e-8f)return norm(v);if(x>=y&&x>=z)return{q.x>=0?1.f:-1.f,0,0};if(y>=z)return{0,q.y>=0?1.f:-1.f,0};return{0,0,q.z>=0?1.f:-1.f};}if(o.kind==Kind::Capsule){V ba=o.b-o.a,q=p-o.a;float h=clampf(dot(q,ba)/o.len2,0,1);return norm(q-ba*h);}V q=p-o.a;float ax=dot(q,o.n0);if(ax<.0002f)return o.n0*-1;if(ax>o.len-.0002f)return o.n0;return norm(q-o.n0*(ax+o.k));}
void closest(const Scene&s,const Ray&r,Hit&best,Stats*st,bool ground,int ignoreObject,bool ignoreTransparent){
 best=Hit{};best.id=-1;best.mat=-1;
 if(ground&&r.d.y<-.000001f){float t=-r.o.y/r.d.y;if(t>.0001f){V p=r.o+r.d*t;best={t,p,{0,1,0},-1,-2};}}
 const RayBoxCache box=makeRayBoxCache(r);
 struct Entry{int32_t index;float nearDist;};
 std::array<Entry,128> stack{};int top=0;std::vector<Entry> overflow;
 auto push=[&](Entry e){if(overflow.empty()&&top<(int)stack.size())stack[top++]=e;else overflow.push_back(e);};
 auto pop=[&](){if(!overflow.empty()){Entry e=overflow.back();overflow.pop_back();return e;}return stack[--top];};
 if(s.root>=0){if(st)st->aabb++;float q;if(boundsHit(r,box,s.node[s.root],best.t,q))push({s.root,q});}
 while(top||!overflow.empty()){
  Entry entry=pop();int idx=entry.index;if(entry.nearDist>best.t)continue;const Node&n=s.node[idx];if(st)st->nodes++;
  if(n.count){for(int i=n.start;i<n.start+n.count;i++){uint32_t oi=s.order[i];const Obj&o=s.obj[oi];if((int)oi==ignoreObject||(ignoreTransparent&&s.mat[o.mat].transmissionPercent))continue;if(st)st->tests++;float t;if(intersect(r,o,best.t,t)){best.t=t;best.id=(int)oi;best.mat=o.mat;}}}
  else{if(st)st->aabb+=2;float a=INF,b=INF;bool ha=boundsHit(r,box,s.node[n.left],best.t,a),hb=boundsHit(r,box,s.node[n.right],best.t,b);if(ha&&hb){if(a<b){push({n.right,b});push({n.left,a});}else{push({n.left,a});push({n.right,b});}}else if(ha)push({n.left,a});else if(hb)push({n.right,b});}
 }
 if(best.id>=0){best.p=r.o+r.d*best.t;best.n=normalAt(s.obj[best.id],best.p);}
}
int groundMaterial(const Scene&s,V p){float shore=12.f+.55f*std::sin(p.z*.08f)+.30f*std::sin(p.z*.21f);for(size_t i=0;i<s.mat.size();i++){auto&n=s.mat[i].name;if((p.x>shore&&n=="sea")||((p.x>4.84f||p.x< -4.85f)&&p.x<=shore&&n=="sand")||(p.x<=4.84f&&p.x>=-4.85f&&n=="asphalt"))return (int)i;}return 0;}
}
