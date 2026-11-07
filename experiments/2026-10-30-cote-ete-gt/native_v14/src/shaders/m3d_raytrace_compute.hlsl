// Pripyat / Moteur3D - D3D11 Compute Shader Model 5.0, no DXR.
// Ray tracing on mixed analytic primitives. Preview uses averaged albedo and sun.
// This is NOT the full CPU PBR lighting path; magenta means BVH traversal failure.
struct Ray { float3 origin; float3 direction; };
struct GpuBvhNode {float3 boundsMin; uint left; float3 boundsMax; uint right; uint first; uint count; uint pad0; uint pad1;};
struct GpuObject {
 float3 mn; uint kind;
 float3 mx; uint material;
 float3 a; float r;
 float3 b; float r1;
 float3 c; float k;
 float3 n0; float len;
 float3 n1; float len2;
 float3 n2; float pad0;
 float3 center; float pad1;
 float3 core; float pad2;
};
struct GpuMaterial { float4 shade; };
cbuffer Frame : register(b0) {
 float3 eye; float tanHalfFov;
 float3 forward; float aspect;
 float3 right; float nearPlane;
 float3 up; float farPlane;
 uint width; uint height; uint nodeCount; uint objectCount;
 uint rootIndex; uint maxTraversal; uint reserved0; uint reserved1;
 float3 sunDir; float ambient;
};
StructuredBuffer<GpuBvhNode> Nodes : register(t0);
StructuredBuffer<GpuObject> Objects : register(t1);
StructuredBuffer<uint> PrimitiveOrder : register(t2);
StructuredBuffer<GpuMaterial> Materials : register(t3);
RWTexture2D<float4> Output : register(u0);
bool hitBounds(Ray r,float3 mn,float3 mx,float limit){
 float lo=max(nearPlane,0.0001),hi=limit;
 [unroll]for(uint k=0;k<3;k++){
  float d=r.direction[k],o=r.origin[k];
  if(abs(d)<1.e-9){if(o<mn[k]||o>mx[k])return false;}
  else{float a=(mn[k]-o)/d,b=(mx[k]-o)/d;
   lo=max(lo,min(a,b));hi=min(hi,max(a,b));if(hi<lo)return false;}
 }
 return true;
}
float3 normalBox(GpuObject o,float3 p){
 float3 a=abs(p-o.mn),b=abs(p-o.mx);
 float d=a.x;float3 n=float3(-1,0,0);
 if(b.x<d){d=b.x;n=float3(1,0,0);}
 if(a.y<d){d=a.y;n=float3(0,-1,0);}
 if(b.y<d){d=b.y;n=float3(0,1,0);}
 if(a.z<d){d=a.z;n=float3(0,0,-1);}
 if(b.z<d)n=float3(0,0,1);
 return n;
}
float sdRoundedBox(GpuObject o,float3 p){
 float3 q=abs(p-o.center)-o.core;
 return length(max(q,0.0))+min(max(q.x,max(q.y,q.z)),0.0)-o.r;
}
bool hitObject(Ray ray,GpuObject o,float limit,out float t,out float3 n){
 t=limit;n=float3(0,1,0);
 float dir2=dot(ray.direction,ray.direction);
 if(o.kind==0){ // triangle
  float3 e1=o.b-o.a,e2=o.c-o.a,p=cross(ray.direction,e2);
  float det=dot(e1,p);
  if(abs(det)<1.e-10)return false;
  float3 q=ray.origin-o.a;
  float u=dot(q,p)/det;if(u<0||u>1)return false;
  float3 v=cross(q,e1);float w=dot(ray.direction,v)/det;
  if(w<0||u+w>1)return false;
  t=dot(e2,v)/det;
  n=normalize(o.n0*(1-u-w)+o.n1*u+o.n2*w);
 }else if(o.kind==5){ // ellipsoid
  if(min(o.b.x,min(o.b.y,o.b.z))<=0)return false;
  float3 p=(ray.origin-o.a)/o.b,d=ray.direction/o.b;
  float A=dot(d,d),B=dot(p,d),C=dot(p,p)-1,D=B*B-A*C;
  if(D<0||A<1.e-15)return false;
  float q=(-B-sqrt(D))/A;t=q>.0001?q:(-B+sqrt(D))/A;
  float3 h=ray.origin+ray.direction*t;
  n=normalize((h-o.a)/(o.b*o.b));
 }else if(o.kind==1||o.kind==2){ // box, rounded box
  float lo=-1.e25,hi=limit;
  [unroll]for(uint k=0;k<3;k++){
   float p=ray.origin[k],d=ray.direction[k],mn=o.mn[k],mx=o.mx[k];
   if(abs(d)<1.e-12){if(p<mn||p>mx)return false;}
   else{float a=(mn-p)/d,b=(mx-p)/d;
    lo=max(lo,min(a,b));hi=min(hi,max(a,b));if(lo>hi)return false;}
  }
  if(o.kind==1){t=lo>.0001?lo:hi;n=normalBox(o,ray.origin+ray.direction*t);}
  else{
   t=max(lo,.0001);bool ok=false;
   [loop]for(uint it=0;it<64&&t<hi;it++){
    float d=sdRoundedBox(o,ray.origin+ray.direction*t);
    if(d<.001){ok=true;break;}t+=max(d*.82,.0005);
   }
   if(!ok||t>=hi)return false;
   float3 q=ray.origin+ray.direction*t-o.center;
   float3 d=abs(q)-o.core,v=max(d,0.0)*sign(q);
   if(dot(v,v)>1.e-12)n=normalize(v);
   else if(d.x>=d.y&&d.x>=d.z)n=float3(q.x>=0?1:-1,0,0);
   else if(d.y>=d.z)n=float3(0,q.y>=0?1:-1,0);
   else n=float3(0,0,q.z>=0?1:-1);
  }
 }else if(o.kind==3){ // capsule
  float3 ba=o.b-o.a,oa=ray.origin-o.a;
  float baba=max(o.len2,1.e-10),rdba=dot(ray.direction,ba),oaba=dot(oa,ba);
  float A=baba*dir2-rdba*rdba,B=baba*dot(ray.direction,oa)-oaba*rdba;
  float C=baba*dot(oa,oa)-oaba*oaba-o.r*o.r*baba;
  float h=B*B-A*C;
  if(A>1.e-12&&h>=0){float tc=(-B-sqrt(h))/A,y=oaba+tc*rdba;if(tc>.0001&&y>0&&y<baba)t=tc;}
  [unroll]for(uint i=0;i<2;i++){
   float3 q=ray.origin-(i==0?o.a:o.b);
   float bb=dot(q,ray.direction),cc=dot(q,q)-o.r*o.r,D=bb*bb-dir2*cc;
   if(D>=0){float sq=sqrt(D),t0=(-bb-sq)/dir2,t1=(-bb+sq)/dir2;
    if(t0>.0001&&t0<t)t=t0;else if(t1>.0001&&t1<t)t=t1;
   }
  }
  float3 q=ray.origin+ray.direction*t-o.a;
  float axis=saturate(dot(q,ba)/baba);
  n=normalize(q-ba*axis);
 }else if(o.kind==4){ // cone/frustum
  float3 p=ray.origin-o.a;
  float z=dot(p,o.n0),zd=dot(ray.direction,o.n0);
  float3 q=p-o.n0*z,v=ray.direction-o.n0*zd;
  float radius=o.r+o.k*z,rd=o.k*zd;
  float A=dot(v,v)-rd*rd,B=dot(q,v)-radius*rd,C=dot(q,q)-radius*radius;
  if(abs(A)>1.e-12){float D=B*B-A*C;
   if(D>=0){float sq=sqrt(D);
    [unroll]for(uint i=0;i<2;i++){float x=i==0?(-B-sq)/A:(-B+sq)/A,h=z+x*zd;
     if(x>.0001&&x<t&&h>=0&&h<=o.len)t=x;}
   }
  }
  if(abs(zd)>1.e-12){
   [unroll]for(uint i=0;i<2;i++){
    float h=i==0?0:o.len,rad=i==0?o.r:o.r1,x=(h-z)/zd;
    if(x>.0001&&x<t){float3 qq=q+v*x;if(dot(qq,qq)<=rad*rad)t=x;}
   }
  }
  float3 hit=ray.origin+ray.direction*t-o.a;float ax=dot(hit,o.n0);
  if(ax<.0002)n=-o.n0;
  else if(ax>o.len-.0002)n=o.n0;
  else n=normalize(hit-o.n0*(ax+o.k));
 }else return false;
 return t>.0001&&t<limit;
}
float4 tracePrimary(Ray ray){
 if(nodeCount==0||rootIndex>=nodeCount)return float4(.02,.03,.06,1);
 uint stack[64],sp=0;stack[sp++]=rootIndex;
 float closest=farPlane;float3 normal=float3(0,1,0);uint material=0;
 bool found=false,overflow=false;uint visits=0;
 [loop]while(sp>0){
  if(++visits>maxTraversal){overflow=true;break;}
  uint index=stack[--sp];
  if(index>=nodeCount){overflow=true;break;}
  GpuBvhNode node=Nodes[index];
  if(!hitBounds(ray,node.boundsMin,node.boundsMax,closest))continue;
  if(node.count>0){
   [loop]for(uint k=0;k<node.count;k++){
    uint id=PrimitiveOrder[node.first+k];if(id>=objectCount){overflow=true;break;}
    GpuObject obj=Objects[id];float t;float3 n;
    if(hitObject(ray,obj,closest,t,n)&&t<closest){
     closest=t;normal=n;material=obj.material;found=true;
    }
   }
  }else{
   if(sp+2>64){overflow=true;break;}
   stack[sp++]=node.left;stack[sp++]=node.right;
  }
  if(overflow)break;
 }
 if(overflow)return float4(1,0,1,1);
 if(!found){
  if(ray.direction.y<-.00001){float t=-ray.origin.y/ray.direction.y;
   if(t>.0001&&t<farPlane){float c=.16+.24*ambient;return float4(c,c,c,1);}
  }
  return float4(.02,.03,.06,1);
 }
 GpuMaterial m=Materials[material];
 float3 n=normalize(normal);
 float light=.16+ambient*(.28+.70*saturate(dot(n,normalize(sunDir))));
 float3 reflected=reflect(-normalize(sunDir),n);
 float spec=pow(saturate(dot(-ray.direction,reflected)),26)*m.shade.a*.25*ambient;
 return float4(saturate(m.shade.rgb*light+float3(spec,spec,spec)),1);
}
[numthreads(8,8,1)]
void CSMain(uint3 tid:SV_DispatchThreadID){
 if(tid.x>=width||tid.y>=height)return;
 float2 uv=((float2(tid.xy)+.5)/float2(width,height))*2-1;
 Ray ray;ray.origin=eye;
 // Match CPU m3d_render.cpp and D3D11 raster: tanHalfFov is HORIZONTAL.
 // Keep vertical field of view divided by aspect, never multiplied into X.
 ray.direction=normalize(forward+right*uv.x*tanHalfFov-up*uv.y*tanHalfFov/aspect);
 Output[tid.xy]=tracePrimary(ray);
}
